// Stub functions implementations
#ifndef IV_LV5_BREAKER_STUB_H_
#define IV_LV5_BREAKER_STUB_H_
#include <iv/lv5/breaker/fwd.h>
namespace iv {
namespace lv5 {
namespace breaker {
namespace stub {

#define ERR\
  ctx->PendingError());\
  if (*e) {\
    IV_LV5_BREAKER_RAISE();\
  }\
((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY

inline void BUILD_ENV(Context* ctx, railgun::Frame* framx,
                      uint32_t size, uint32_t mutable_start) {
  frame->variable_env_ = frame->lexical_env_ =
      JSDeclEnv::New(ctx,
                     frame->lexical_env(),
                     size,
                     frame->code()->names().begin(),
                     mutable_start);
}

inline Rep WITH_SETUP(Context* ctx, railgun::Frame* frame, JSVal src) {
  JSObject* const obj = src.ToObject(ctx, ERR);
  JSObjectEnv* const with_env =
      JSObjectEnv::New(ctx, frame->lexical_env(), obj);
  with_env->set_provide_this(true);
  frame->set_lexical_env(with_env);
  return 0;
}

inline Rep BINARY_ADD(Context* ctx, JSVal lhs, JSVal rhs) {
  assert(!lhs.IsNumber() || !rhs.IsNumber());
  if (lhs.IsString()) {
    if (rhs.IsString()) {
      return Extract(JSString::New(ctx, lhs.string(), rhs.string()));
    } else {
      const JSVal rp = rhs.ToPrimitive(ctx, Hint::NONE, ERR);
      JSString* const rs = rp.ToString(ctx, ERR);
      return Extract(JSString::New(ctx, lhs.string(), rs));
    }
  }

  const JSVal lprim = lhs.ToPrimitive(ctx, Hint::NONE, ERR);
  const JSVal rprim = rhs.ToPrimitive(ctx, Hint::NONE, ERR);
  if (lprim.IsString() || rprim.IsString()) {
    JSString* const lstr = lprim.ToString(ctx, ERR);
    JSString* const rstr = rprim.ToString(ctx, ERR);
    return Extract(JSString::New(ctx, lstr, rstr));
  }

  const double left = lprim.ToNumber(ctx_, ERR);
  const double right = rprim.ToNumber(ctx_, ERR);
  return Extract(left + right);
}

inline Rep BINARY_LT(Context* ctx, JSVal lhs, JSVal rhs) {
  const CompareResult res = JSVal::Compare<true>(ctx_, lhs, rhs, ERR);
  return Extract(JSVal::Bool(res == CMP_TRUE));
}

inline Rep BINARY_LTE(Context* ctx, JSVal lhs, JSVal rhs) {
  const CompareResult res = JSVal::Compare<false>(ctx_, rhs, lhs, ERR);
  return Extract(JSVal::Bool(res == CMP_FALSE));
}

inline Rep BINARY_GT(Context* ctx, JSVal lhs, JSVal rhs) {
  const CompareResult res = JSVal::Compare<false>(ctx_, rhs, lhs, ERR);
  return Extract(JSVal::Bool(res == CMP_TRUE));
}

inline Rep BINARY_GTE(Context* ctx, JSVal lhs, JSVal rhs) {
  const CompareResult res = JSVal::Compare<true>(ctx_, lhs, rhs, ERR);
  return Extract(JSVal::Bool(res == CMP_FALSE));
}

inline Rep TO_NUMBER(Context* ctx, JSVal src) {
  const double x = src.ToNumber(ctx, ERR);
  return Extract(x);
}

inline Rep UNARY_NEGATIVE(Context* ctx, JSVal src) {
  const double x = src.ToNumber(ctx, ERR);
  return Extract(-x);
}

inline JSVal UNARY_NOT(JSVal src) {
  return JSVal::Bool(!src.ToBoolean());
}

inline Rep UNARY_BIT_NOT(Context* ctx, JSVal src) {
  const double value = src.ToNumber(ctx, ERR);
  return Extract(JSVal::Int32(~core::DoubleToInt32(value)));
}

inline Rep THROW(Context* ctx, JSVal src) {
  ctx->PendingError()->Report(src);
  IV_LV5_BREAKER_RAISE();
}

inline void POP_ENV(railgun::Frame* frame) {
  frame->set_lexical_env(frame->lexical_env()->outer());
}

inline JSVal RETURN(JSVal val, railgun::Frame* frame) {
  if (frame->constructor_call_ && !val.IsObject()) {
    val = frame->GetThis();
  }
  // because of Frame is code frame,
  // first lexical_env is variable_env.
  // (if Eval / Global, this is not valid)
  assert(frame->lexical_env() == frame->variable_env());
  stack_.Unwind(frame);
  return val;
}

#undef ERR
} } } }  // namespace iv::lv5::breaker::stub
#endif  // IV_LV5_BREAKER_STUB_H_
