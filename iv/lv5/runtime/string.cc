#include <cassert>
#include <vector>
#include <limits>
#include <utility>
#include <iv/detail/tuple.h>
#include <iv/ustring.h>
#include <iv/ustringpiece.h>
#include <iv/character.h>
#include <iv/conversions.h>
#include <iv/platform_math.h>
#include <iv/lv5/error_check.h>
#include <iv/lv5/constructor_check.h>
#include <iv/lv5/arguments.h>
#include <iv/lv5/jsval.h>
#include <iv/lv5/error.h>
#include <iv/lv5/jsobject.h>
#include <iv/lv5/jsstring.h>
#include <iv/lv5/jsstringobject.h>
#include <iv/lv5/jsstring_iterator.h>
#include <iv/lv5/jsregexp.h>
#include <iv/lv5/internal.h>
#include <iv/lv5/runtime/regexp.h>
#include <iv/lv5/runtime/string.h>
namespace iv {
namespace lv5 {
namespace runtime {
namespace detail {

static inline JSVal StringToStringValueOfImpl(JSVal this_binding,
                                              const char* msg, Error* e);

static inline bool IsTrimmed(char16_t c) {
  return core::character::IsWhiteSpace(c) ||
         core::character::IsLineTerminator(c);
}

inline int64_t SplitMatch(JSString* str, uint32_t q, JSString* rhs) {
  const std::size_t rs = rhs->size();
  const std::size_t s = str->size();
  if (q + rs > s) {
    return -1;
  }
  if (str->Is8Bit() == rhs->Is8Bit()) {
    // same type
    if (str->Is8Bit()) {
      if (std::char_traits<uint8_t>::compare(
              rhs->Get8Bit()->data(),
              str->Get8Bit()->data() + q, rs) != 0) {
        return -1;
      }
    } else {
      if (std::char_traits<char16_t>::compare(
              rhs->Get16Bit()->data(),
              str->Get16Bit()->data() + q, rs) != 0) {
        return -1;
      }
    }
  } else {
    if (str->Is8Bit()) {
      const Fiber8* left = str->Get8Bit();
      const Fiber16* right = rhs->Get16Bit();
      if (core::CompareIterators(
              right->begin(),
              right->begin() + rs,
              left->begin() + q,
              left->begin() + q + rs) != 0) {
        return -1;
      }
    } else {
      const Fiber16* left = str->Get16Bit();
      const Fiber8* right = rhs->Get8Bit();
      if (core::CompareIterators(
              right->begin(),
              right->begin() + rs,
              left->begin() + q,
              left->begin() + q + rs) != 0) {
        return -1;
      }
    }
  }
  return q + rs;
}

inline JSVal StringSplit(Context* ctx,
                         JSString* target,
                         JSString* rhs, uint32_t lim, Error* e) {
  const uint32_t rsize = rhs->size();
  if (rsize == 0) {
    if (target->empty()) {
      // "".split("") => []
      return JSArray::New(ctx);
    } else {
      return target->Split(ctx, lim, e);
    }
  } else if (rsize == 1) {
    return target->Split(ctx, rhs->At(0), lim, e);
  }
  const uint32_t size = target->size();
  uint32_t p = 0;
  uint32_t q = p;
  JSVector* vec = JSVector::New(ctx);
  vec->reserve(16);
  while (q != size) {
    const int64_t rs = detail::SplitMatch(target, q, rhs);
    if (rs == -1) {
      ++q;
    } else {
      const uint32_t end = static_cast<uint32_t>(rs);
      if (end == p) {
        ++q;
      } else {
        vec->push_back(target->Substring(ctx, p, q));
        if (vec->size() == lim) {
          return vec->ToJSArray();
        }
        q = p = end;
      }
    }
  }
  vec->push_back(target->Substring(ctx, p, size));
  return vec->ToJSArray();
}

struct Replace {
  enum State {
    kNormal,
    kDollar,
    kDigit,
    kDigitZero
  };
};

template<typename T>
class Replacer : private core::Noncopyable<> {
 public:
  template<typename Builder>
  void Replace(Builder* builder, Error* e) {
    using std::get;
    if (reg_.Match(ctx_, str_, 0, &vec_)) {
      builder->AppendJSString(*str_, 0, vec_[0]);
      static_cast<T*>(this)->DoReplace(builder, e);
      builder->AppendJSString(*str_, vec_[1], str_->size());
    } else {
      builder->AppendJSString(*str_, 0, str_->size());
    }
  }

  template<typename Builder>
  void ReplaceGlobal(Builder* builder, Error* e) {
    using std::get;
    int previous_index = 0;
    int not_matched_index = previous_index;
    const int size = str_->size();
    do {
      if (!reg_.Match(ctx_, str_, previous_index, &vec_)) {
        break;
      }
      builder->AppendJSString(*str_, not_matched_index, vec_[0]);

      int last_index = vec_[1];
      if (vec_[0] == vec_[1]) {
        ++last_index;
      }

      const int this_index = last_index;
      not_matched_index = vec_[1];
      if (previous_index == this_index) {
        ++previous_index;
      } else {
        previous_index = this_index;
      }
      static_cast<T*>(this)->DoReplace(builder, IV_LV5_ERROR_VOID(e));
      if (previous_index > size || previous_index < 0) {
        break;
      }
    } while (true);
    builder->AppendJSString(*str_, not_matched_index, str_->size());
  }

 protected:
  Replacer(Context* ctx, JSString* str, const JSRegExp& reg)
    : ctx_(ctx),
      str_(str),
      reg_(reg),
      vec_(reg.num_of_captures() * 2) {
  }

  Context* ctx_;
  JSString* str_;
  const JSRegExp& reg_;
  std::vector<int> vec_;
};

class StringReplacer : public Replacer<StringReplacer> {
 public:
  typedef StringReplacer this_type;
  typedef Replacer<this_type> super_type;
  StringReplacer(Context* ctx,
                 JSString* str,
                 const JSRegExp& reg,
                 JSString* replace)
    : super_type(ctx, str, reg),
      replace_(replace) {
  }

  template<typename Builder>
  void DoReplace(Builder* builder, Error* e) {
    if (replace_->Is8Bit()) {
      DoReplaceImpl(replace_->Get8Bit(), builder, e);
    } else {
      DoReplaceImpl(replace_->Get16Bit(), builder, e);
    }
  }

  template<typename Builder, typename FiberType>
  void DoReplaceImpl(const FiberType* fiber, Builder* builder, Error* e) {
    using std::get;
    Replace::State state = Replace::kNormal;
    char16_t upper_digit_char = '\0';
    for (typename FiberType::const_iterator it = fiber->begin(),
         last = fiber->end();
         it != last; ++it) {
start:  // NOLINT
      const char16_t ch = *it;
      if (state == Replace::kNormal) {
        if (ch == '$') {
          state = Replace::kDollar;
        } else {
          builder->Append(ch);
        }
      } else if (state == Replace::kDollar) {
        switch (ch) {
          case '$':  // $$ pattern
            state = Replace::kNormal;
            builder->Append('$');
            break;

          case '&':  // $& pattern
            state = Replace::kNormal;
            builder->AppendJSString(
                *str_,
                vec_[0], vec_[1]);
            break;

          case '`':  // $` pattern
            state = Replace::kNormal;
            builder->AppendJSString(
                *str_,
                0, vec_[0]);
            break;

          case '\'':  // $' pattern
            state = Replace::kNormal;
            builder->AppendJSString(
                *str_,
                vec_[1], str_->size());
            break;

          default:
            if (core::character::IsDecimalDigit(ch)) {
              state = (ch == '0') ? Replace::kDigitZero : Replace::kDigit;
              upper_digit_char = ch;
            } else {
              state = Replace::kNormal;
              builder->Append('$');
              builder->Append(ch);
            }
        }
      } else if (state == Replace::kDigit) {  // twin digit pattern search
        state = Replace::kNormal;
        if (core::character::IsDecimalDigit(ch)) {  // twin digit
          const std::size_t single_n = core::Radix36Value(upper_digit_char);
          const std::size_t n = single_n * 10 + core::Radix36Value(ch);
          if ((vec_.size() / 2) > n) {
            // check undefined
            if (vec_[n * 2] != -1 && vec_[n * 2 + 1] != -1) {
              builder->AppendJSString(
                  *str_,
                  vec_[n * 2], vec_[n * 2 + 1]);
            }
          } else {
            // single digit pattern search
            if ((vec_.size() / 2) > single_n) {
              // check undefined
              if (vec_[single_n * 2] != -1 && vec_[single_n * 2 + 1] != -1) {
                builder->AppendJSString(
                    *str_,
                    vec_[single_n * 2], vec_[single_n * 2 + 1]);
              }
            } else {
              builder->Append('$');
              builder->Append(upper_digit_char);
            }
            builder->Append(ch);  // Because it is not dollar
          }
        } else {
          const std::size_t n = core::Radix36Value(upper_digit_char);
          if ((vec_.size() / 2) > n) {
            // check undefined
            if (vec_[n * 2] != -1 && vec_[n * 2 + 1] != -1) {
              builder->AppendJSString(
                  *str_,
                  vec_[n * 2], vec_[n * 2 + 1]);
            }
          } else {
            builder->Append('$');
            builder->Append(upper_digit_char);
          }
          goto start;
        }
      } else {  // twin digit pattern search
        assert(state == Replace::kDigitZero);
        state = Replace::kNormal;
        if (core::character::IsDecimalDigit(ch)) {
          const std::size_t n =
              core::Radix36Value(upper_digit_char)*10 + core::Radix36Value(ch);
          if ((vec_.size() / 2) > n) {
            // check undefined
            if (vec_[n * 2] != -1 && vec_[n * 2 + 1] != -1) {
              builder->AppendJSString(
                  *str_,
                  vec_[n * 2], vec_[n * 2 + 1]);
            }
          } else {
            builder->Append("$0");
            builder->Append(ch);  // Because it is not dollar
          }
        } else {
          // $0 is not used
          builder->Append("$0");
          goto start;
        }
      }
    }

    if (state == Replace::kDollar) {
      builder->Append('$');
    } else if (state == Replace::kDigit) {
      const std::size_t n = core::Radix36Value(upper_digit_char);
      if ((vec_.size() / 2) > n) {
        // check undefined
        if (vec_[n * 2] != -1 && vec_[n * 2 + 1] != -1) {
          builder->AppendJSString(
              *str_,
              vec_[n * 2], vec_[n * 2 + 1]);
        }
      } else {
        builder->Append('$');
        builder->Append(upper_digit_char);
      }
    } else if (state == Replace::kDigitZero) {
      builder->Append("$0");
    }
  }

 private:
  JSString* replace_;
};

class FunctionReplacer : public Replacer<FunctionReplacer> {
 public:
  typedef FunctionReplacer this_type;
  typedef Replacer<this_type> super_type;
  FunctionReplacer(Context* ctx,
                   JSString* str,
                   const JSRegExp& reg,
                   JSFunction* function)
    : super_type(ctx, str, reg),
      function_(function) {
  }

  template<typename Builder>
  void DoReplace(Builder* builder, Error* e) {
    using std::get;
    ScopedArguments a(ctx_, 2 + (vec_.size() / 2), IV_LV5_ERROR_VOID(e));
    std::size_t i = 0;
    for (std::size_t len = vec_.size() / 2; i < len; ++i) {
      // check undefined
      if (vec_[i * 2] != -1 && vec_[i * 2 + 1] != -1) {
        a[i] = str_->Substring(ctx_, vec_[i * 2], vec_[i * 2 + 1]);
      }
    }
    a[i++] = vec_[0];
    a[i++] = str_;
    const JSVal result = function_->Call(&a, JSUndefined, IV_LV5_ERROR_VOID(e));
    const JSString* const replaced_str =
        result.ToString(ctx_, IV_LV5_ERROR_VOID(e));
    builder->AppendJSString(*replaced_str);
  }

 private:
  JSFunction* function_;
};

template<typename Builder, typename FiberType>
inline void ReplaceOnce(Builder* builder,
                        JSString* str,
                        JSString* search_str,
                        typename FiberType::size_type loc,
                        FiberType* replace_str) {
  Replace::State state = Replace::kNormal;
  for (typename FiberType::const_iterator it = replace_str->begin(),
       last = replace_str->end(); it != last; ++it) {
    const char16_t ch = *it;
    if (state == Replace::kNormal) {
      if (ch == '$') {
        state = Replace::kDollar;
      } else {
        builder->Append(ch);
      }
    } else {
      assert(state == Replace::kDollar);
      switch (ch) {
        case '$':  // $$ pattern
          state = Replace::kNormal;
          builder->Append('$');
          break;

        case '&':  // $& pattern
          state = Replace::kNormal;
          builder->AppendJSString(*search_str);
          break;

        case '`':  // $` pattern
          state = Replace::kNormal;
          builder->AppendJSString(*str, 0, loc);
          break;

        case '\'':  // $' pattern
          state = Replace::kNormal;
          builder->AppendJSString(
              *str,
              loc + search_str->size(), str->size());
          break;

        default:
          state = Replace::kNormal;
          builder->Append('$');
          builder->Append(ch);
      }
    }
  }
  if (state == Replace::kDollar) {
    builder->Append('$');
  }
}


template<typename FiberType>
JSVal StringTrimHelper(Context* ctx, const FiberType* fiber, Error* e);

}  // namespace detail

// section 15.5.1
JSVal StringConstructor(const Arguments& args, Error* e) {
  if (args.IsConstructorCalled()) {
    JSString* str;
    if (!args.empty()) {
      str = args.front().ToString(args.ctx(), IV_LV5_ERROR(e));
    } else {
      str = JSString::NewEmptyString(args.ctx());
    }
    return JSStringObject::New(args.ctx(), str);
  } else {
    if (!args.empty()) {
      return args.front().ToString(args.ctx(), e);
    } else {
      return JSString::NewEmptyString(args.ctx());
    }
  }
}

// section 15.5.3.2 String.fromCharCode([char0 [, char1[, ...]]])
JSVal StringFromCharCode(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.fromCharCode", args, e);
  Context* const ctx = args.ctx();
  JSStringBuilder builder;
  for (Arguments::const_iterator it = args.begin(),
       last = args.end(); it != last; ++it) {
    const uint32_t ch = it->ToUInt32(ctx, IV_LV5_ERROR(e));
    builder.Append(ch);
  }
  return builder.Build(ctx, false, e);
}

// ES6
// section 15.5.3.3 String.fromCodePoint(...codePoints)
JSVal StringFromCodePoint(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.fromCodePoint", args, e);
  Context* const ctx = args.ctx();
  JSStringBuilder builder;
  std::back_insert_iterator<JSStringBuilder> out(builder);
  for (Arguments::const_iterator it = args.begin(),
       last = args.end(); it != last; ++it) {
    const double nextCP = it->ToNumber(ctx, IV_LV5_ERROR(e));
    if (!JSVal::SameValue(nextCP, core::DoubleToInteger(nextCP)) ||
        nextCP < 0 ||
        nextCP > 0x10FFFF) {
      e->Report(Error::Range, "code point out of range");
      return JSEmpty;
    }
    const uint32_t cp = static_cast<uint32_t>(nextCP);
    out = core::unicode::CodePointToUTF16(cp, out);
  }
  return builder.Build(ctx, false, e);
}

// ES6
// section 15.5.3.4 String.raw(callSite, ...substitutions)
JSVal StringRaw(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.raw", args, e);
  Context* const ctx = args.ctx();
  JSObject* cooked = args.At(0).ToObject(ctx, IV_LV5_ERROR(e));
  const JSVal raw_value = cooked->Get(ctx, symbol::raw(), IV_LV5_ERROR(e));
  JSObject* raw = raw_value.ToObject(ctx, IV_LV5_ERROR(e));
  const uint32_t literal_segments =
      internal::GetLength(ctx, raw, IV_LV5_ERROR(e));
  if (!literal_segments) {
    return JSString::NewEmptyString(ctx);
  }
  JSStringBuilder elements;
  uint32_t next_index = 0;
  while (next_index < literal_segments) {
    JSVal next =
        raw->Get(ctx, symbol::MakeSymbolFromIndex(next_index), IV_LV5_ERROR(e));
    const JSString* next_seg = next.ToString(ctx, IV_LV5_ERROR(e));
    elements.AppendJSString(*next_seg);
    ++next_index;
    if (next_index == literal_segments) {
      return elements.Build(ctx, false, e);
    }
    next = args.At(next_index);
    const JSString* next_sub = next.ToString(ctx, IV_LV5_ERROR(e));
    elements.AppendJSString(*next_sub);
  }
  UNREACHABLE();
  return JSEmpty;  // makes compiler happy
}

static inline JSVal detail::StringToStringValueOfImpl(JSVal this_binding,
                                                      const char* msg,
                                                      Error* e) {
  if (this_binding.IsString()) {
    return this_binding.string();
  }

  if (this_binding.IsObject() &&
      this_binding.object()->IsClass<Class::String>()) {
    return static_cast<JSStringObject*>(this_binding.object())->value();
  }

  e->Report(Error::Type, msg);
  return JSEmpty;
}

// section 15.5.4.2 String.prototype.toString()
JSVal StringToString(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.toString", args, e);
  return detail::StringToStringValueOfImpl(
      args.this_binding(),
      "String.prototype.toString is not generic function", e);
}

// section 15.5.4.3 String.prototype.valueOf()
JSVal StringValueOf(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.valueOf", args, e);
  return detail::StringToStringValueOfImpl(
      args.this_binding(),
      "String.prototype.valueOf is not generic function", e);
}

// section 15.5.4.4 String.prototype.charAt(pos)
JSVal StringCharAt(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.charAt", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  JSString* const str = val.ToString(args.ctx(), IV_LV5_ERROR(e));
  const JSVal first = args.At(0);
  uint32_t pos;
  if (first.GetUInt32(&pos)) {
    if (pos >= str->size()) {
      return JSString::NewEmptyString(args.ctx());
    }
  } else {
    const double position = first.ToInteger(args.ctx(), IV_LV5_ERROR(e));
    if (position < 0 || position >= str->size()) {
      return JSString::NewEmptyString(args.ctx());
    }
    pos = static_cast<uint32_t>(position);
  }
  return JSString::NewSingle(args.ctx(), str->At(pos));
}

// section 15.5.4.5 String.prototype.charCodeAt(pos)
JSVal StringCharCodeAt(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.charCodeAt", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  JSString* const str = val.ToString(args.ctx(), IV_LV5_ERROR(e));
  const JSVal first = args.At(0);
  uint32_t pos;
  if (first.GetUInt32(&pos)) {
    if (pos >= str->size()) {
      return JSNaN;
    }
  } else {
    const double position = first.ToInteger(args.ctx(), IV_LV5_ERROR(e));
    if (position < 0 || position >= str->size()) {
      return JSNaN;
    }
    pos = static_cast<uint32_t>(position);
  }
  return JSVal::UInt16(str->At(pos));
}

// ES6
// section 15.5.4.5 String.prototype.codePointAt(pos)
JSVal StringCodePointAt(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.charCodeAt", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  JSString* const str = val.ToString(args.ctx(), IV_LV5_ERROR(e));
  const JSVal arg1 = args.At(0);
  uint32_t pos;
  if (arg1.GetUInt32(&pos)) {
    if (pos >= str->size()) {
      return JSUndefined;
    }
  } else {
    const double position = arg1.ToInteger(args.ctx(), IV_LV5_ERROR(e));
    if (position < 0 || position >= str->size()) {
      return JSUndefined;
    }
    pos = static_cast<uint32_t>(position);
  }
  const char16_t first = str->At(pos);
  if (first < 0xD800 || first > 0xDBFF || (pos + 1) == str->size()) {
    return JSVal::UInt16(first);
  }
  const char16_t second = str->At(pos + 1);
  if (second < 0xDC00 || second > 0xDFFF) {
    return JSVal::UInt16(first);
  }
  return JSVal::UInt32(core::unicode::DecodeSurrogatePair(first, second));
}

// section 15.5.4.6 String.prototype.concat([string1[, string2[, ...]]])
JSVal StringConcat(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.concat", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  JSString* const str = val.ToString(args.ctx(), IV_LV5_ERROR(e));
  JSStringBuilder builder;
  builder.AppendJSString(*str);
  for (Arguments::const_iterator it = args.begin(),
       last = args.end(); it != last; ++it) {
    const JSString* const r = it->ToString(args.ctx(), IV_LV5_ERROR(e));
    builder.AppendJSString(*r);
  }
  return builder.Build(args.ctx(), false, e);
}

// section 15.5.4.7 String.prototype.indexOf(searchString, position)
JSVal StringIndexOf(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.indexOf", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  JSString* str = val.ToString(args.ctx(), IV_LV5_ERROR(e));
  JSString* const search_str = args.At(0).ToString(args.ctx(), IV_LV5_ERROR(e));

  // undefined -> NaN -> 0
  uint32_t pos = 0;
  if (args.size() > 1) {
    const JSVal first = args[1];
    if (first.GetUInt32(&pos)) {
      pos = std::min<uint32_t>(pos, str->size());
    } else {
      const double position = first.ToInteger(args.ctx(), IV_LV5_ERROR(e));
      if (position > 0.0) {
        if (position > str->size()) {
          pos = str->size();
        } else {
          pos = static_cast<uint32_t>(position);
        }
      }
    }
  }

  const JSString::size_type loc = str->find(*search_str, pos);
  return (loc == JSString::npos) ? JSVal::Int32(-1) : JSVal(loc);
}

// section 15.5.4.8 String.prototype.lastIndexOf(searchString, position)
JSVal StringLastIndexOf(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.lastIndexOf", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  JSString* const str = val.ToString(args.ctx(), IV_LV5_ERROR(e));
  std::size_t target = str->size();
  JSString* const search_str = args.At(0).ToString(args.ctx(), IV_LV5_ERROR(e));
  if (args.size() > 1) {
    const double position = args[1].ToNumber(args.ctx(), IV_LV5_ERROR(e));
    if (!core::math::IsNaN(position)) {
      const double integer = core::DoubleToInteger(position);
      if (integer < 0) {
        target = 0;
      } else if (integer < target) {
        target = static_cast<std::size_t>(integer);
      }
    }
  }
  const JSString::size_type loc = str->rfind(*search_str, target);
  return (loc == JSString::npos) ? JSVal::Int32(-1) : JSVal(loc);
}

// section 15.5.4.9 String.prototype.localeCompare(that)
JSVal StringLocaleCompare(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.localeCompare", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  JSString* const str = val.ToString(args.ctx(), IV_LV5_ERROR(e));
  JSString* that = args.At(0).ToString(args.ctx(), IV_LV5_ERROR(e));
  return str->compare(*that);
}

// section 15.5.4.10 String.prototype.match(regexp)
JSVal StringMatch(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.match", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  Context* const ctx = args.ctx();
  JSString* const str = val.ToString(ctx, IV_LV5_ERROR(e));
  const uint32_t args_count = args.size();
  JSRegExp* regexp;
  if (args_count == 0 ||
      !args[0].IsObject() ||
      !args[0].object()->IsClass<Class::RegExp>()) {
    ScopedArguments a(ctx, 1, IV_LV5_ERROR(e));
    if (args_count == 0) {
      a[0] = JSUndefined;
    } else {
      a[0] = args[0];
    }
    const JSVal res = RegExpConstructor(a, IV_LV5_ERROR(e));
    assert(res.IsObject());
    regexp = static_cast<JSRegExp*>(res.object());
  } else {
    regexp = static_cast<JSRegExp*>(args[0].object());
  }
  const bool global = regexp->global();
  if (!global) {
    return regexp->Exec(ctx, str, e);
  }
  // step 8
  return regexp->ExecGlobal(ctx, str, e);
}

// section 15.5.4.11 String.prototype.replace(searchValue, replaceValue)
JSVal StringReplace(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.replace", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  Context* const ctx = args.ctx();
  JSString* const str = val.ToString(ctx, IV_LV5_ERROR(e));
  const uint32_t args_count = args.size();
  const bool search_value_is_regexp =
      args_count != 0 &&
      args[0].IsObject() && args[0].object()->IsClass<Class::RegExp>();

  if (search_value_is_regexp) {
    // searchValue is RegExp
    using std::get;
    const JSRegExp* reg = static_cast<JSRegExp*>(args[0].object());
    JSStringBuilder builder;
    if (args_count > 1 && args[1].IsCallable()) {
      JSFunction* const callable =
          static_cast<JSFunction*>(args[1].object());
      detail::FunctionReplacer replacer(ctx, str, *reg, callable);
      if (reg->global()) {
        replacer.ReplaceGlobal(&builder, IV_LV5_ERROR(e));
      } else {
        replacer.Replace(&builder, IV_LV5_ERROR(e));
      }
    } else {
      JSString* replace_value;
      if (args_count > 1) {
        replace_value = args[1].ToString(ctx, IV_LV5_ERROR(e));
      } else {
        replace_value = ctx->global_data()->string_undefined();
      }
      detail::StringReplacer replacer(ctx, str, *reg, replace_value);
      if (reg->global()) {
        replacer.ReplaceGlobal(&builder, IV_LV5_ERROR(e));
      } else {
        replacer.Replace(&builder, IV_LV5_ERROR(e));
      }
    }
    return builder.Build(ctx, false, e);
  } else {
    JSString* search_str = (args_count == 0) ?
        ctx->global_data()->string_undefined() :
        args[0].ToString(ctx, IV_LV5_ERROR(e));
    const JSString::size_type loc = str->find(*search_str, 0);
    if (loc == JSString::npos) {
      // not found
      return str;
    }
    // found pattern
    JSStringBuilder builder;
    builder.AppendJSString(*str, 0, loc);
    if (args_count > 1 && args[1].IsCallable()) {
      JSFunction* const callable =
          static_cast<JSFunction*>(args[1].object());
      ScopedArguments a(ctx, 3, IV_LV5_ERROR(e));
      a[0] = search_str;
      a[1] = static_cast<double>(loc);
      a[2] = str;
      const JSVal result = callable->Call(&a, JSUndefined, IV_LV5_ERROR(e));
      const JSString* const res = result.ToString(ctx, IV_LV5_ERROR(e));
      builder.AppendJSString(*res);
    } else {
      JSString* replace_value;
      if (args_count > 1) {
        replace_value = args[1].ToString(ctx, IV_LV5_ERROR(e));
      } else {
        replace_value = ctx->global_data()->string_undefined();
      }
      if (replace_value->Is8Bit()) {
        detail::ReplaceOnce(&builder, str, search_str,
                            loc, replace_value->Get8Bit());
      } else {
        detail::ReplaceOnce(&builder, str, search_str,
                            loc, replace_value->Get16Bit());
      }
    }
    builder.AppendJSString(
        *str,
        loc + search_str->size(), str->size());
    return builder.Build(ctx, false, e);
  }
}

// section 15.5.4.12 String.prototype.search(regexp)
JSVal StringSearch(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.search", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  Context* const ctx = args.ctx();
  JSString* const str = val.ToString(ctx, IV_LV5_ERROR(e));
  const uint32_t args_count = args.size();
  JSRegExp* regexp;
  if (args_count == 0) {
    regexp = JSRegExp::New(ctx);
  } else if (args[0].IsObject() &&
             args[0].object()->IsClass<Class::RegExp>()) {
    regexp = JSRegExp::New(
        ctx, static_cast<JSRegExp*>(args[0].object()));
  } else {
    ScopedArguments a(ctx, 1, IV_LV5_ERROR(e));
    a[0] = args[0];
    const JSVal res = RegExpConstructor(a, IV_LV5_ERROR(e));
    assert(res.IsObject());
    regexp = static_cast<JSRegExp*>(res.object());
  }
  const int last_index = regexp->LastIndex(ctx, IV_LV5_ERROR(e));
  regexp->SetLastIndex(ctx, 0, IV_LV5_ERROR(e));
  const JSVal result = regexp->Exec(ctx, str, e);
  regexp->SetLastIndex(ctx, last_index, IV_LV5_ERROR(e));
  if (result.IsNull()) {
    return JSVal::Int32(-1);
  } else {
    assert(result.IsObject());
    return result.object()->Get(ctx, symbol::index(), e);
  }
}

// section 15.5.4.13 String.prototype.slice(start, end)
JSVal StringSlice(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.slice", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  Context* const ctx = args.ctx();
  const JSString* const str = val.ToString(ctx, IV_LV5_ERROR(e));
  const uint32_t len = str->size();
  uint32_t start;
  if (!args.empty()) {
    const double relative_start = args.front().ToInteger(ctx, IV_LV5_ERROR(e));
    if (relative_start < 0) {
      start = core::DoubleToUInt32(std::max<double>(relative_start + len, 0.0));
    } else {
      start = core::DoubleToUInt32(std::min<double>(relative_start, len));
    }
  } else {
    start = 0;
  }
  uint32_t end;
  if (args.size() > 1) {
    if (args[1].IsUndefined()) {
      end = len;
    } else {
      const double relative_end = args[1].ToInteger(ctx, IV_LV5_ERROR(e));
      if (relative_end < 0) {
        end = core::DoubleToUInt32(std::max<double>(relative_end + len, 0.0));
      } else {
        end = core::DoubleToUInt32(std::min<double>(relative_end, len));
      }
    }
  } else {
    end = len;
  }
  const uint32_t span = (end < start) ? 0 : end - start;
  return str->Substring(ctx, start, start + span);
}

// section 15.5.4.14 String.prototype.split(separator, limit)
JSVal StringSplit(const Arguments& args, Error* e) {
  using std::get;
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.split", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  Context* const ctx = args.ctx();
  JSString* const str = val.ToString(ctx, IV_LV5_ERROR(e));
  const uint32_t args_count = args.size();
  uint32_t lim;
  if (args_count < 2 || args[1].IsUndefined()) {
    lim = 4294967295UL;  // (1 << 32) - 1
  } else {
    lim = args[1].ToUInt32(ctx, IV_LV5_ERROR(e));
  }

  bool regexp = false;
  JSVal target = JSEmpty;
  JSVal separator;
  if (args_count > 0) {
    separator = args[0];
    if (separator.IsObject() && separator.object()->IsClass<Class::RegExp>()) {
      target = separator;
      regexp = true;
    } else {
      target = separator.ToString(ctx, IV_LV5_ERROR(e));
    }
  } else {
    separator = JSUndefined;
  }

  if (lim == 0) {
    return JSArray::New(ctx);
  }

  if (separator.IsUndefined()) {
    JSArray* const a = JSArray::New(ctx);
    a->DefineOwnProperty(
      ctx,
      symbol::MakeSymbolFromIndex(0u),
      DataDescriptor(str, ATTR::W | ATTR::E | ATTR::C),
      false, IV_LV5_ERROR(e));
    return a;
  }

  if (!regexp) {
    assert(target.IsString());
    return detail::StringSplit(ctx, str, target.string(), lim, e);
  }

  assert(target.IsObject());
  JSRegExp* const reg = static_cast<JSRegExp*>(target.object());
  std::vector<int> cap(reg->num_of_captures() * 2);
  const uint32_t size = str->size();
  if (size == 0) {
    if (reg->Match(ctx, str, 0, &cap)) {
      return JSArray::New(ctx);
    }
    JSArray* const ary = JSArray::New(ctx);
    ary->JSArray::DefineOwnProperty(
        ctx,
        symbol::MakeSymbolFromIndex(0),
        DataDescriptor(str, ATTR::W | ATTR::E | ATTR::C),
        false, IV_LV5_ERROR(e));
    return ary;
  }

  uint32_t p = 0;
  uint32_t q = p;
  uint32_t start_match = 0;
  JSVector* vec = JSVector::New(ctx);
  vec->reserve(16);
  while (q != size) {
    if (!reg->Match(ctx, str, q, &cap) ||
        (start_match = static_cast<uint32_t>(cap[0])) == size) {
        break;
    }
    const uint32_t end = cap[1];
    if (q == end && end == p) {
      ++q;
    } else {
      vec->push_back(str->Substring(ctx, p, start_match));
      if (vec->size() == lim) {
        return vec->ToJSArray();
      }
      for (uint32_t i = 1, len = cap.size() >> 1; i < len; ++i) {
        if (cap[i * 2] != -1 && cap[i * 2 + 1] != -1) {
          vec->push_back(str->Substring(ctx, cap[i * 2], cap[i * 2 + 1]));
        } else {
          vec->push_back(JSUndefined);
        }
        if (vec->size() == lim) {
          return vec->ToJSArray();
        }
      }
      q = p = end;
    }
  }
  vec->push_back(str->Substring(ctx, p, size));
  return vec->ToJSArray();
}

// section 15.5.4.15 String.prototype.substring(start, end)
JSVal StringSubstring(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.substring", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  Context* const ctx = args.ctx();
  JSString* const str = val.ToString(ctx, IV_LV5_ERROR(e));
  const uint32_t len = str->size();

  const JSVal arg0 = args.At(0);
  uint32_t start = 0;
  if (arg0.GetUInt32(&start)) {
    start = std::min(start, len);
  } else {
    const double pos = arg0.ToInteger(ctx, IV_LV5_ERROR(e));
    if (pos > 0.0) {
      start = (pos > len) ? len : pos;
    }
  }

  const JSVal arg1 = args.At(1);
  uint32_t end = 0;
  if (arg1.IsUndefined()) {
    end = len;
  } else if (arg1.GetUInt32(&end)) {
    end = std::min(end, len);
  } else {
    const double pos = arg1.ToInteger(ctx, IV_LV5_ERROR(e));
    if (pos > 0.0) {
      end = (pos > len) ? len : pos;
    }
  }

  const uint32_t from = (std::min<uint32_t>)(start, end);
  const uint32_t to = (std::max<uint32_t>)(start, end);

  return str->Substring(ctx, from, to);
}

namespace detail {

template<typename Converter>
JSString* ConvertCase(Context* ctx,
                      JSString* str, Converter converter, Error* e) {
  if (str->Is8Bit()) {
    std::vector<char> builder;
    builder.resize(str->size());
    const Fiber8* fiber = str->Get8Bit();
    std::transform(fiber->begin(), fiber->end(), builder.begin(), converter);
    return JSString::New(ctx, builder.begin(), builder.end(), true, e);
  } else {
    // Special Casing is considered
    std::vector<char16_t> builder;
    builder.reserve(str->size());
    const Fiber16* fiber = str->Get16Bit();
    for (Fiber16::const_iterator it = fiber->begin(),
         last = fiber->end(); it != last; ++it) {
      const uint64_t ch = converter(*it);
      if (ch > 0xFFFF) {
        if (ch > 0xFFFFFFFF) {
          builder.push_back((ch >> 32) & 0xFFFF);
        }
        builder.push_back((ch >> 16) & 0xFFFF);
        builder.push_back(ch & 0xFFFF);
      } else {
        builder.push_back(ch);
      }
    }
    return JSString::New(ctx, builder.begin(), builder.end(), false, e);
  }
}

template<typename Iter, typename Converter>
JSString* ConvertCaseLocale(Context* ctx,
                            Iter it, Iter last, Converter converter, Error* e) {
  std::vector<char16_t> builder;
  builder.reserve(std::distance(it, last));
  int prev = core::character::code::DEFAULT;
  int next = core::character::code::DEFAULT;
  for (; it != last;) {
    const char16_t ch = *it;
    ++it;
    if (it != last) {
      next = *it;
    } else {
      next = core::character::code::DEFAULT;
    }
    prev = ch;
    // hard coding
    const uint64_t res = converter(core::character::locale::EN, ch, prev, next);
    if (res != core::character::code::REMOVE) {
      if (res > 0xFFFF) {
        if (res > 0xFFFFFFFF) {
          builder.push_back((res >> 32) & 0xFFFF);
        }
        builder.push_back((res >> 16) & 0xFFFF);
        builder.push_back(res & 0xFFFF);
      } else {
        builder.push_back(res);
      }
    }
  }
  return JSString::New(ctx, builder.begin(), builder.end(), false, e);
}

}  // namespace detail

// section 15.5.4.16 String.prototype.toLowerCase()
JSVal StringToLowerCase(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.toLowerCase", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  JSString* const str = val.ToString(args.ctx(), IV_LV5_ERROR(e));
  return detail::ConvertCase(args.ctx(), str, [](char16_t ch) {
    return core::character::ToLowerCase(ch);
  }, e);
}

// section 15.5.4.17 String.prototype.toLocaleLowerCase()
JSVal StringToLocaleLowerCase(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.toLocaleLowerCase", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  JSString* const str = val.ToString(args.ctx(), IV_LV5_ERROR(e));
  if (str->Is8Bit()) {
    const Fiber8* fiber = str->Get8Bit();
    return detail::ConvertCaseLocale(
        args.ctx(),
        fiber->begin(), fiber->end(),
        [](core::character::locale::Locale locale,
           char16_t c, int prev, int next) {
      return core::character::ToLocaleLowerCase(locale, c, prev, next);
    }, e);
  } else {
    const Fiber16* fiber = str->Get16Bit();
    return detail::ConvertCaseLocale(
        args.ctx(),
        fiber->begin(), fiber->end(),
        [](core::character::locale::Locale locale,
           char16_t c, int prev, int next) {
      return core::character::ToLocaleLowerCase(locale, c, prev, next);
    }, e);
  }
}

// section 15.5.4.18 String.prototype.toUpperCase()
JSVal StringToUpperCase(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.toUpperCase", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  JSString* const str = val.ToString(args.ctx(), IV_LV5_ERROR(e));
  return detail::ConvertCase(args.ctx(), str, [](char16_t ch) {
    return core::character::ToUpperCase(ch);
  }, e);
}

// section 15.5.4.19 String.prototype.toLocaleUpperCase()
JSVal StringToLocaleUpperCase(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.toLocaleUpperCase", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  JSString* const str = val.ToString(args.ctx(), IV_LV5_ERROR(e));
  if (str->Is8Bit()) {
    const Fiber8* fiber = str->Get8Bit();
    return detail::ConvertCaseLocale(
        args.ctx(),
        fiber->begin(), fiber->end(),
        [](core::character::locale::Locale locale,
           char16_t c, int prev, int next) {
      return core::character::ToLocaleUpperCase(locale, c, prev, next);
    }, e);
  } else {
    const Fiber16* fiber = str->Get16Bit();
    return detail::ConvertCaseLocale(
        args.ctx(),
        fiber->begin(), fiber->end(),
        [](core::character::locale::Locale locale,
           char16_t c, int prev, int next) {
      return core::character::ToLocaleUpperCase(locale, c, prev, next);
    }, e);
  }
}

template<typename FiberType>
JSVal detail::StringTrimHelper(Context* ctx, const FiberType* fiber, Error* e) {
  typename FiberType::const_iterator lit = fiber->begin();
  const typename FiberType::const_iterator last = fiber->end();
  // trim leading space
  bool empty = true;
  for (; lit != last; ++lit) {
    if (!detail::IsTrimmed(*lit)) {
      empty = false;
      break;
    }
  }
  if (empty) {
    return JSString::NewEmptyString(ctx);
  }
  // trim tailing space
  typename FiberType::const_reverse_iterator rit = fiber->rbegin();
  const typename FiberType::const_reverse_iterator rend(lit);
  for (; rit != rend; ++rit) {
    if (!detail::IsTrimmed(*rit)) {
      break;
    }
  }
  return JSString::New(ctx, lit, rit.base(), false, e);
}

// section 15.5.4.20 String.prototype.trim()
JSVal StringTrim(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.trim", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  JSString* const str = val.ToString(args.ctx(), IV_LV5_ERROR(e));
  if (str->Is8Bit()) {
    return detail::StringTrimHelper(args.ctx(), str->Get8Bit(), e);
  } else {
    return detail::StringTrimHelper(args.ctx(), str->Get16Bit(), e);
  }
}

// section 15.5.4.21 String.prototype.repeat(count)
JSVal StringRepeat(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.repeat", args, e);
  const JSVal val = args.this_binding();
  Context* ctx = args.ctx();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  JSString* const str = val.ToString(ctx, IV_LV5_ERROR(e));
  const int32_t count = args.At(0).ToInt32(ctx, IV_LV5_ERROR(e));
  if (count < 0) {
    return JSString::NewEmptyString(ctx);
  }
  return str->Repeat(ctx, count, e);
}

// section 15.5.4.22 String.prototype.startsWith(searchString, [position])
JSVal StringStartsWith(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.startsWith", args, e);
  const JSVal val = args.this_binding();
  Context* ctx = args.ctx();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  JSString* const str = val.ToString(ctx, IV_LV5_ERROR(e));
  JSString* const search_string = args.At(0).ToString(ctx, IV_LV5_ERROR(e));

  uint32_t start = 0;
  if (args.size() > 1) {
    const JSVal arg1 = args[1];
    if (arg1.GetUInt32(&start)) {
      start = std::min<uint32_t>(start, str->size());
    } else {
      const double pos = arg1.ToInteger(ctx, IV_LV5_ERROR(e));
      if (pos > 0.0) {
        if (pos > str->size()) {
          start = str->size();
        } else {
          start = static_cast<uint32_t>(pos);
        }
      }
    }
  }

  if (search_string->size() + start > str->size()) {
    return JSFalse;
  }
  JSString::const_iterator it = str->begin() + start;
  return JSVal::Bool(
      std::equal(search_string->begin(), search_string->end(), it));
}

// section 15.5.4.23 String.prototype.endsWith(searchString, [endPosition])
JSVal StringEndsWith(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.endsWith", args, e);
  const JSVal val = args.this_binding();
  Context* ctx = args.ctx();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  JSString* const str = val.ToString(ctx, IV_LV5_ERROR(e));
  JSString* const search_string = args.At(0).ToString(ctx, IV_LV5_ERROR(e));

  uint32_t end = str->size();
  if (args.size() > 1) {
    const JSVal arg1 = args[1];
    if (arg1.IsUndefined()) {
      end = str->size();
    } else if (arg1.GetUInt32(&end)) {
      end = std::min<uint32_t>(end, str->size());
    } else {
      const double pos = arg1.ToInteger(ctx, IV_LV5_ERROR(e));
      if (pos > 0.0) {
        if (pos > str->size()) {
          end = str->size();
        } else {
          end = static_cast<uint32_t>(pos);
        }
      }
    }
  }

  if (search_string->size() > end) {
    return JSFalse;
  }
  const std::size_t start = end - search_string->size();
  const JSString::const_iterator it = str->begin() + start;
  return JSVal::Bool(
      std::equal(search_string->begin(), search_string->end(), it));
}

// section 15.5.4.24 String.prototype.contains(searchString, [position])
JSVal StringContains(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.contains", args, e);
  const JSVal val = args.this_binding();
  Context* ctx = args.ctx();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  JSString* const str = val.ToString(ctx, IV_LV5_ERROR(e));
  JSString* const search_string = args.At(0).ToString(ctx, IV_LV5_ERROR(e));

  uint32_t start = 0;
  if (args.size() > 1) {
    const JSVal arg1 = args[1];
    if (arg1.GetUInt32(&start)) {
      start = std::min<uint32_t>(start, str->size());
    } else {
      const double pos = arg1.ToInteger(ctx, IV_LV5_ERROR(e));
      if (pos > 0.0) {
        if (pos > str->size()) {
          start = str->size();
        } else {
          start = static_cast<uint32_t>(pos);
        }
      }
    }
  }

  if ((search_string->size() + start) > str->size()) {
    return JSFalse;
  }
  return JSVal::Bool(str->find(*search_string, start) != JSString::npos);
}

// section 15.5.4.25 String.prototype.reverse()
JSVal StringReverse(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.reverse", args, e);
  const JSVal val = args.this_binding();
  Context* ctx = args.ctx();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  JSString* const str = val.ToString(ctx, IV_LV5_ERROR(e));
  return JSString::New(ctx, str->crbegin(), str->crend(), str->Is8Bit(), e);
}

// section B.2.3 String.prototype.substr(start, length)
// this method is deprecated.
JSVal StringSubstr(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype.substr", args, e);
  const JSVal val = args.this_binding();
  Context* const ctx = args.ctx();
  const JSString* const str = val.ToString(args.ctx(), IV_LV5_ERROR(e));
  const double len = str->size();

  double start;
  if (!args.empty()) {
    start = args.front().ToInteger(ctx, IV_LV5_ERROR(e));
  } else {
    start = 0.0;
  }

  double length;
  if (args.size() > 1) {
    if (args[1].IsUndefined()) {
      length = std::numeric_limits<double>::infinity();
    } else {
      length = args[1].ToInteger(ctx, IV_LV5_ERROR(e));
    }
  } else {
    length = std::numeric_limits<double>::infinity();
  }

  double result5;
  if (start > 0 || start == 0) {
    result5 = start;
  } else {
    result5 = std::max<double>(start + len, 0.0);
  }

  const double result6 = std::min<double>(std::max<double>(length, 0.0),
                                          len - result5);

  if (result6 <= 0) {
    return JSString::NewEmptyString(ctx);
  }

  const uint32_t capacity = core::DoubleToUInt32(result6);
  const uint32_t start_position = core::DoubleToUInt32(result5);
  return str->Substring(ctx, start_position, start_position + capacity);
}

// ES6
// section 21.1.3.27 String.prototype[@@iterator]()
JSVal StringIterator(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("String.prototype[@@iterator]", args, e);
  const JSVal val = args.this_binding();
  val.CheckObjectCoercible(IV_LV5_ERROR(e));
  JSString* const str = val.ToString(args.ctx(), IV_LV5_ERROR(e));
  return JSStringIterator::New(args.ctx(), str);
}

} } }  // namespace iv::lv5::runtime
