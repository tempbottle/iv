// Stub functions implementations
#ifndef IV_LV5_BREAKER_STUB_H_
#define IV_LV5_BREAKER_STUB_H_
#include <iv/lv5/jsglobal.h>
#include <iv/lv5/breaker/fwd.h>
#include <iv/lv5/railgun/railgun.h>
#include <iv/lv5/breaker/templates.h>
#include <iv/lv5/breaker/assembler.h>
#include <iv/lv5/breaker/jsfunction.h>
#include <iv/lv5/breaker/context_fwd.h>
#include <iv/lv5/breaker/mono_ic.h>
#include <iv/lv5/breaker/poly_ic.h>
#include <iv/lv5/accessor.h>
namespace iv {
namespace lv5 {
namespace breaker {
namespace stub {

#define IV_LV5_BREAKER_RAISE_PAIR()\
  do {\
    void* pc = *stack->ret;\
    *stack->ret = Templates<>::dispatch_exception_handler();  /* NOLINT */\
    return Extract(core::BitCast<uint64_t>(pc), static_cast<uint64_t>(0));\
  } while (0)

#define IV_LV5_BREAKER_RAISE()\
  do {\
    void* pc = *stack->ret;\
    *stack->ret = Templates<>::dispatch_exception_handler();  /* NOLINT */\
    return core::BitCast<uint64_t>(pc);\
  } while (0)

#define IV_LV5_BREAKER_ERR\
  stack->error);\
  do {\
    if (*stack->error) {\
      IV_LV5_BREAKER_RAISE();\
    }\
  } while (0);\
((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY

#define IV_LV5_BREAKER_ERR_PAIR\
  stack->error);\
  do {\
    if (*stack->error) {\
      IV_LV5_BREAKER_RAISE_PAIR();\
    }\
  } while (0);\
((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY

inline void BUILD_ENV(Context* ctx,
                      railgun::Frame* frame,
                      uint32_t size, uint32_t mutable_start) {
  frame->variable_env_ = frame->lexical_env_ =
      JSDeclEnv::New(ctx,
                     frame->lexical_env(),
                     size,
                     frame->code()->names().begin(),
                     mutable_start);
}

inline Rep WITH_SETUP(Frame* stack, railgun::Frame* frame, JSVal src) {
  JSObject* const obj = src.ToObject(stack->ctx, IV_LV5_BREAKER_ERR);
  JSObjectEnv* const with_env =
      JSObjectEnv::New(stack->ctx, frame->lexical_env(), obj);
  with_env->set_provide_this(true);
  frame->set_lexical_env(with_env);
  return 0;
}

inline Rep FORIN_SETUP(Frame* stack, JSVal enumerable) {
  Context* ctx = stack->ctx;
  railgun::NativeIterator* it;
  if (enumerable.IsString()) {
    it = stack->ctx->GainNativeIterator(enumerable.string());
  } else {
    JSObject* const obj = enumerable.ToObject(ctx, IV_LV5_BREAKER_ERR);
    it = stack->ctx->GainNativeIterator(obj);
  }
  return Extract(JSVal::Cell(it));
}

inline Rep FORIN_ENUMERATE(Context* ctx, JSVal iterator) {
  railgun::NativeIterator* it =
      static_cast<railgun::NativeIterator*>(iterator.cell());
  const Symbol sym = it->Next();
  if (sym != symbol::kDummySymbol) {
    return Extract(JSString::New(ctx, sym));
  }
  return 0;
}

inline void FORIN_LEAVE(Context* ctx, JSVal iterator) {
  railgun::NativeIterator* it =
      static_cast<railgun::NativeIterator*>(iterator.cell());
  ctx->ReleaseNativeIterator(it);
}

inline Rep BINARY_ADD(Frame* stack, JSVal lhs, JSVal rhs) {
  Context* ctx = stack->ctx;
  if (lhs.IsString()) {
    if (rhs.IsString()) {
      JSString* const result =
          JSString::New(ctx, lhs.string(), rhs.string(), IV_LV5_BREAKER_ERR);
      return Extract(result);
    } else {
      const JSVal rp = rhs.ToPrimitive(ctx, Hint::NONE, IV_LV5_BREAKER_ERR);
      JSString* const rs = rp.ToString(ctx, IV_LV5_BREAKER_ERR);
      JSString* const result = JSString::New(ctx, lhs.string(), rs, IV_LV5_BREAKER_ERR);
      return Extract(result);
    }
  }

  const JSVal lprim = lhs.ToPrimitive(ctx, Hint::NONE, IV_LV5_BREAKER_ERR);
  const JSVal rprim = rhs.ToPrimitive(ctx, Hint::NONE, IV_LV5_BREAKER_ERR);
  if (lprim.IsString() || rprim.IsString()) {
    JSString* const lstr = lprim.ToString(ctx, IV_LV5_BREAKER_ERR);
    JSString* const rstr = rprim.ToString(ctx, IV_LV5_BREAKER_ERR);
    JSString* const result = JSString::New(ctx, lstr, rstr, IV_LV5_BREAKER_ERR);
    return Extract(result);
  }

  const double left = lprim.ToNumber(ctx, IV_LV5_BREAKER_ERR);
  const double right = rprim.ToNumber(ctx, IV_LV5_BREAKER_ERR);
  return Extract(left + right);
}

inline Rep BINARY_SUBTRACT(Frame* stack, JSVal lhs, JSVal rhs) {
  Context* ctx = stack->ctx;
  const double left = lhs.ToNumber(ctx, IV_LV5_BREAKER_ERR);
  const double res = left -  rhs.ToNumber(ctx, IV_LV5_BREAKER_ERR);
  return Extract(res);
}

inline Rep BINARY_MULTIPLY(Frame* stack, JSVal lhs, JSVal rhs) {
  Context* ctx = stack->ctx;
  const double left = lhs.ToNumber(ctx, IV_LV5_BREAKER_ERR);
  const double res = left *  rhs.ToNumber(ctx, IV_LV5_BREAKER_ERR);
  return Extract(res);
}

inline Rep BINARY_DIVIDE(Frame* stack, JSVal lhs, JSVal rhs) {
  Context* ctx = stack->ctx;
  const double left = lhs.ToNumber(ctx, IV_LV5_BREAKER_ERR);
  const double res = left / rhs.ToNumber(ctx, IV_LV5_BREAKER_ERR);
  return Extract(res);
}

inline Rep BINARY_MODULO(Frame* stack, JSVal lhs, JSVal rhs) {
  Context* ctx = stack->ctx;
  const double left = lhs.ToNumber(ctx, IV_LV5_BREAKER_ERR);
  const double right = rhs.ToNumber(ctx, IV_LV5_BREAKER_ERR);
  return Extract(core::math::Modulo(left, right));
}

inline Rep BINARY_LSHIFT(Frame* stack, JSVal lhs, JSVal rhs) {
  Context* ctx = stack->ctx;
  const int32_t left = lhs.ToInt32(ctx, IV_LV5_BREAKER_ERR);
  const int32_t right = rhs.ToInt32(ctx, IV_LV5_BREAKER_ERR);
  return Extract(JSVal::Int32(left << (right & 0x1f)));
}

inline Rep BINARY_RSHIFT(Frame* stack, JSVal lhs, JSVal rhs) {
  Context* ctx = stack->ctx;
  const int32_t left = lhs.ToInt32(ctx, IV_LV5_BREAKER_ERR);
  const int32_t right = rhs.ToInt32(ctx, IV_LV5_BREAKER_ERR);
  return Extract(JSVal::Int32(left >> (right & 0x1f)));
}

inline Rep BINARY_RSHIFT_LOGICAL(Frame* stack, JSVal lhs, JSVal rhs) {
  Context* ctx = stack->ctx;
  const uint32_t left = lhs.ToUInt32(ctx, IV_LV5_BREAKER_ERR);
  const int32_t right = rhs.ToInt32(ctx, IV_LV5_BREAKER_ERR);
  return Extract(JSVal::UInt32(left >> (right & 0x1f)));
}

inline Rep BINARY_LT(Frame* stack, JSVal lhs, JSVal rhs) {
  Context* ctx = stack->ctx;
  const CompareResult res = JSVal::Compare<true>(ctx, lhs, rhs, IV_LV5_BREAKER_ERR);
  return Extract(JSVal::Bool(res == CMP_TRUE));
}

inline Rep BINARY_LTE(Frame* stack, JSVal lhs, JSVal rhs) {
  Context* ctx = stack->ctx;
  const CompareResult res = JSVal::Compare<false>(ctx, rhs, lhs, IV_LV5_BREAKER_ERR);
  return Extract(JSVal::Bool(res == CMP_FALSE));
}

inline Rep BINARY_GT(Frame* stack, JSVal lhs, JSVal rhs) {
  Context* ctx = stack->ctx;
  const CompareResult res = JSVal::Compare<false>(ctx, rhs, lhs, IV_LV5_BREAKER_ERR);
  return Extract(JSVal::Bool(res == CMP_TRUE));
}

inline Rep BINARY_GTE(Frame* stack, JSVal lhs, JSVal rhs) {
  Context* ctx = stack->ctx;
  const CompareResult res = JSVal::Compare<true>(ctx, lhs, rhs, IV_LV5_BREAKER_ERR);
  return Extract(JSVal::Bool(res == CMP_FALSE));
}

inline Rep BINARY_INSTANCEOF(Frame* stack, JSVal lhs, JSVal rhs) {
  Context* ctx = stack->ctx;
  if (!rhs.IsObject()) {
    stack->error->Report(Error::Type, "instanceof requires object");
    IV_LV5_BREAKER_RAISE();
  }
  JSObject* const robj = rhs.object();
  if (!robj->IsCallable()) {
    stack->error->Report(Error::Type, "instanceof requires constructor");
    IV_LV5_BREAKER_RAISE();
  }
  const bool result =
      static_cast<JSFunction*>(robj)->HasInstance(ctx, lhs, IV_LV5_BREAKER_ERR);
  return Extract(JSVal::Bool(result));
}

inline Rep BINARY_IN(Frame* stack, JSVal lhs, JSVal rhs) {
  Context* ctx = stack->ctx;
  if (!rhs.IsObject()) {
    stack->error->Report(Error::Type, "in requires object");
    IV_LV5_BREAKER_RAISE();
  }
  const Symbol name = lhs.ToSymbol(ctx, IV_LV5_BREAKER_ERR);
  return Extract(JSVal::Bool(rhs.object()->HasProperty(ctx, name)));
}

inline Rep BINARY_EQ(Frame* stack, JSVal lhs, JSVal rhs) {
  Context* ctx = stack->ctx;
  const bool result = JSVal::AbstractEqual(ctx, lhs, rhs, IV_LV5_BREAKER_ERR);
  return Extract(JSVal::Bool(result));
}

inline Rep BINARY_STRICT_EQ(JSVal lhs, JSVal rhs) {
  return Extract(JSVal::Bool(JSVal::StrictEqual(lhs, rhs)));
}

inline Rep BINARY_NE(Frame* stack, JSVal lhs, JSVal rhs) {
  const bool result = JSVal::AbstractEqual(stack->ctx, lhs, rhs, IV_LV5_BREAKER_ERR);
  return Extract(JSVal::Bool(!result));
}

inline Rep BINARY_STRICT_NE(JSVal lhs, JSVal rhs) {
  return Extract(JSVal::Bool(!JSVal::StrictEqual(lhs, rhs)));
}

inline Rep BINARY_BIT_AND(Frame* stack, JSVal lhs, JSVal rhs) {
  Context* ctx = stack->ctx;
  const int32_t left = lhs.ToInt32(ctx, IV_LV5_BREAKER_ERR);
  const int32_t right = rhs.ToInt32(ctx, IV_LV5_BREAKER_ERR);
  return Extract(JSVal::Int32(left & right));
}

inline Rep BINARY_BIT_XOR(Frame* stack, JSVal lhs, JSVal rhs) {
  Context* ctx = stack->ctx;
  const int32_t left = lhs.ToInt32(ctx, IV_LV5_BREAKER_ERR);
  const int32_t right = rhs.ToInt32(ctx, IV_LV5_BREAKER_ERR);
  return Extract(JSVal::Int32(left ^ right));
}

inline Rep BINARY_BIT_OR(Frame* stack, JSVal lhs, JSVal rhs) {
  Context* ctx = stack->ctx;
  const int32_t left = lhs.ToInt32(ctx, IV_LV5_BREAKER_ERR);
  const int32_t right = rhs.ToInt32(ctx, IV_LV5_BREAKER_ERR);
  return Extract(JSVal::Int32(left | right));
}

inline Rep TO_OBJECT(Frame* stack, JSVal src) {
  return Extract(src.ToObject(stack->ctx));
}

inline Rep TO_NUMBER(Frame* stack, JSVal src) {
  const double x = src.ToNumber(stack->ctx, IV_LV5_BREAKER_ERR);
  return Extract(x);
}

inline Rep UNARY_NEGATIVE(Frame* stack, JSVal src) {
  const double x = src.ToNumber(stack->ctx, IV_LV5_BREAKER_ERR);
  return Extract(-x);
}

inline JSVal UNARY_NOT(JSVal src) {
  return JSVal::Bool(!src.ToBoolean());
}

inline Rep UNARY_BIT_NOT(Frame* stack, JSVal src) {
  const double value = src.ToNumber(stack->ctx, IV_LV5_BREAKER_ERR);
  return Extract(JSVal::Int32(~core::DoubleToInt32(value)));
}

inline Rep THROW(Frame* stack, JSVal src) {
  stack->error->Report(src);
  IV_LV5_BREAKER_RAISE();
}

inline Rep THROW_WITH_TYPE_AND_MESSAGE(
    Frame* stack, Error::Code type, const char* message) {
  stack->error->Report(type, message);
  IV_LV5_BREAKER_RAISE();
}

inline void RaiseReferenceError(Symbol name, Error* e) {
  core::UStringBuilder builder;
  builder.Append('"');
  builder.Append(symbol::GetSymbolString(name));
  builder.Append("\" not defined");
  e->Report(Error::Reference, builder.BuildPiece());
}

inline JSEnv* GetEnv(Context* ctx, JSEnv* env, Symbol name) {
  JSEnv* current = env;
  while (current) {
    if (current->HasBinding(ctx, name)) {
      return current;
    } else {
      current = current->outer();
    }
  }
  return nullptr;
}

inline Rep LOAD_GLOBAL(Frame* stack, Symbol name, GlobalIC* ic, Assembler* as) {
  // opcode | (dst | index) | nop | nop
  Context* ctx = stack->ctx;
  JSGlobal* global = ctx->global_obj();
  Slot slot;
  if (global->GetOwnPropertySlot(ctx, name, &slot)) {
    // now Own Property Pattern only implemented
    if (slot.IsLoadCacheable()) {
      ic->Repatch(as, global->map(), slot.offset() * sizeof(JSVal));
      return Extract(slot.value());
    }
    const JSVal ret = slot.Get(ctx, global, IV_LV5_BREAKER_ERR);
    return Extract(ret);
  }

  if (ctx->global_env()->HasBinding(ctx, name)) {
    const JSVal res =
       ctx->global_env()->GetBindingValue(ctx, name, ic->strict(), IV_LV5_BREAKER_ERR);
    return Extract(res);
  }
  RaiseReferenceError(name, stack->error);
  IV_LV5_BREAKER_RAISE();
  return 0;
}

inline Rep STORE_GLOBAL(Frame* stack, Symbol name,
                        GlobalIC* ic, Assembler* as, JSVal src) {
  // opcode | (src | name) | nop | nop
  Context* ctx = stack->ctx;
  JSGlobal* global = ctx->global_obj();
  Slot slot;
  if (global->GetOwnPropertySlot(ctx, name, &slot)) {
    if (slot.IsStoreCacheable()) {
      ic->Repatch(as, global->map(), slot.offset() * sizeof(JSVal));
      global->Direct(slot.offset()) = src;
    } else {
      global->Put(ctx, name, src, ic->strict(), IV_LV5_BREAKER_ERR);
    }
  } else {
    if (ctx->global_env()->HasBinding(ctx, name)) {
      ctx->global_env()->SetMutableBinding(ctx, name, src, ic->strict(), IV_LV5_BREAKER_ERR);
    } else {
      if (ic->strict()) {
        stack->error->Report(Error::Reference,
                  "putting to unresolvable reference "
                  "not allowed in strict reference");
        IV_LV5_BREAKER_RAISE();
      } else {
        ctx->global_obj()->Put(ctx, name, src, false, IV_LV5_BREAKER_ERR);
      }
    }
  }
  return 0;
}

inline Rep DELETE_GLOBAL(Frame* stack, Symbol name) {
  Context* ctx = stack->ctx;
  JSEnv* global = ctx->global_env();
  if (!global->HasBinding(ctx, name)) {
    // not found -> unresolvable reference
    return Extract(JSTrue);
  }
  const bool res = global->DeleteBinding(ctx, name);
  return Extract(JSVal::Bool(res));
}

template<bool Strict>
inline Rep TYPEOF_GLOBAL(Frame* stack, Symbol name) {
  Context* ctx = stack->ctx;
  JSEnv* global = ctx->global_env();
  if (!global->HasBinding(ctx, name)) {
    // not found -> unresolvable reference
    return Extract(ctx->global_data()->string_undefined());
  }
  const JSVal res = global->GetBindingValue(ctx, name, Strict, IV_LV5_BREAKER_ERR);
  return Extract(res.TypeOf(ctx));
}

template<int Target, std::size_t Returned, bool Strict>
inline Rep INCREMENT_HEAP(Frame* stack, JSEnv* env, uint32_t offset) {
  JSDeclEnv* decl = static_cast<JSDeclEnv*>(env);
  const JSVal w = decl->GetByOffset(offset, Strict, IV_LV5_BREAKER_ERR);
  if (w.IsInt32() &&
      railgun::detail::IsIncrementOverflowSafe<Target>(w.int32())) {
    std::tuple<JSVal, JSVal> results;
    const int32_t target = w.int32();
    std::get<0>(results) = w;
    std::get<1>(results) = JSVal::Int32(target + Target);
    decl->SetByOffset(offset, std::get<1>(results), Strict, IV_LV5_BREAKER_ERR);
    return Extract(std::get<Returned>(results));
  } else {
    Context* ctx = stack->ctx;
    std::tuple<double, double> results;
    std::get<0>(results) = w.ToNumber(ctx, IV_LV5_BREAKER_ERR);
    std::get<1>(results) = std::get<0>(results) + Target;
    decl->SetByOffset(offset, std::get<1>(results), Strict, IV_LV5_BREAKER_ERR);
    return Extract(JSVal(std::get<Returned>(results)));
  }
}

template<int Target, std::size_t Returned, bool Strict>
inline JSVal IncrementName(Context* ctx,
                           JSEnv* env, Symbol name, Error* e) {
  if (JSEnv* current = GetEnv(ctx, env, name)) {
    const JSVal w =
        current->GetBindingValue(ctx, name, Strict, IV_LV5_ERROR(e));
    if (w.IsInt32() &&
        railgun::detail::IsIncrementOverflowSafe<Target>(w.int32())) {
      std::tuple<JSVal, JSVal> results;
      const int32_t target = w.int32();
      std::get<0>(results) = w;
      std::get<1>(results) = JSVal::Int32(target + Target);
      current->SetMutableBinding(ctx, name, std::get<1>(results), Strict, e);
      return std::get<Returned>(results);
    } else {
      std::tuple<double, double> results;
      std::get<0>(results) = w.ToNumber(ctx, IV_LV5_ERROR(e));
      std::get<1>(results) = std::get<0>(results) + Target;
      current->SetMutableBinding(ctx, name, std::get<1>(results), Strict, e);
      return std::get<Returned>(results);
    }
  }
  RaiseReferenceError(name, e);
  return 0.0;
}

template<int Target, std::size_t Returned, bool Strict>
inline Rep INCREMENT_NAME(Frame* stack, JSEnv* env, Symbol name) {
  const JSVal res =
      IncrementName<Target, Returned, Strict>(stack->ctx, env, name, IV_LV5_BREAKER_ERR);
  return Extract(res);
}

inline RepPair CALL(Frame* stack,
                    JSVal callee,
                    JSVal* offset,
                    uint64_t argc_with_this,
                    railgun::Instruction* instr) {
  Context* ctx = stack->ctx;
  if (!callee.IsCallable()) {
    stack->error->Report(Error::Type, "not callable object");
    IV_LV5_BREAKER_RAISE_PAIR();
  }
  JSFunction* func =
      static_cast<JSFunction*>(callee.object());
  if (func->function_type() == JSFunction::FUNCTION_USER) {
    // inline call
    JSJITFunction* vm_func = static_cast<JSJITFunction*>(func);
    railgun::Code* code = vm_func->code();
    if (code->empty()) {
      return Extract(Extract(JSUndefined), static_cast<uint64_t>(0));
    }
    railgun::Frame* new_frame = ctx->vm()->stack()->NewCodeFrame(
        ctx,
        offset,
        code,
        vm_func->scope(),
        func,
        instr,
        argc_with_this, false);
    if (!new_frame) {
      stack->error->Report(Error::Range, "maximum call stack size exceeded");
      IV_LV5_BREAKER_RAISE_PAIR();
    }
    return Extract(code->executable(), new_frame);
  }

  // Native Function
  {
    railgun::detail::VMArguments args(ctx,
                                      offset + (argc_with_this - 1),
                                      argc_with_this - 1);
    const JSVal res = func->Call(&args, args.this_binding(), IV_LV5_BREAKER_ERR_PAIR);
    return Extract(Extract(res), static_cast<uint64_t>(0));
  }
}

inline RepPair EVAL(Frame* stack,
                    JSVal callee,
                    JSVal* offset,
                    uint64_t argc_with_this,
                    railgun::Instruction* instr,
                    railgun::Frame* from) {
  Context* ctx = stack->ctx;
  if (!callee.IsCallable()) {
    stack->error->Report(Error::Type, "not callable object");
    IV_LV5_BREAKER_RAISE_PAIR();
  }
  JSFunction* func =
      static_cast<JSFunction*>(callee.object());
  if (func->function_type() == JSFunction::FUNCTION_USER) {
    // inline call
    JSJITFunction* vm_func = static_cast<JSJITFunction*>(func);
    railgun::Code* code = vm_func->code();
    if (code->empty()) {
      return Extract(Extract(JSUndefined), static_cast<uint64_t>(0));
    }
    railgun::Frame* new_frame = ctx->vm()->stack()->NewCodeFrame(
        ctx,
        offset,
        code,
        vm_func->scope(),
        func,
        instr,
        argc_with_this, false);
    if (!new_frame) {
      stack->error->Report(Error::Range, "maximum call stack size exceeded");
      IV_LV5_BREAKER_RAISE_PAIR();
    }
    return Extract(code->executable(), new_frame);
  }

  // Native Function
  {
    railgun::detail::VMArguments args(ctx,
                                      offset + (argc_with_this - 1),
                                      argc_with_this - 1);
    const JSAPI native = func->NativeFunction();
    if (native && native == &GlobalEval) {
      // direct call to eval point
      args.set_this_binding(args.this_binding());
      const JSVal res = breaker::DirectCallToEval(args, from, IV_LV5_BREAKER_ERR_PAIR);
      return Extract(Extract(res), static_cast<uint64_t>(0));
    }
    const JSVal res = func->Call(&args, args.this_binding(), IV_LV5_BREAKER_ERR_PAIR);
    return Extract(Extract(res), static_cast<uint64_t>(0));
  }
}

inline RepPair CONSTRUCT(Frame* stack,
                         JSVal callee,
                         JSVal* offset,
                         uint64_t argc_with_this,
                         railgun::Instruction* instr) {
  Context* ctx = stack->ctx;
  if (!callee.IsCallable()) {
    stack->error->Report(Error::Type, "not callable object");
    IV_LV5_BREAKER_RAISE_PAIR();
  }
  JSFunction* func =
      static_cast<JSFunction*>(callee.object());
  if (func->function_type() == JSFunction::FUNCTION_USER) {
    // inline call
    JSJITFunction* vm_func = static_cast<JSJITFunction*>(func);
    railgun::Code* code = vm_func->code();
    railgun::Frame* new_frame = ctx->vm()->stack()->NewCodeFrame(
        ctx,
        offset,
        code,
        vm_func->scope(),
        func,
        instr,
        argc_with_this, false);
    if (!new_frame) {
      stack->error->Report(Error::Range, "maximum call stack size exceeded");
      IV_LV5_BREAKER_RAISE_PAIR();
    }
    Map* map = func->construct_map(ctx, IV_LV5_BREAKER_ERR_PAIR);
    JSObject* const obj = JSObject::New(ctx, map);
    new_frame->set_this_binding(obj);
    return Extract(code->executable(), new_frame);
  }

  // Native Function
  {
    railgun::detail::VMArguments args(ctx,
                                      offset + (argc_with_this - 1),
                                      argc_with_this - 1);
    args.set_constructor_call(true);
    const JSVal res = func->Construct(&args, IV_LV5_BREAKER_ERR_PAIR);
    return Extract(Extract(res), static_cast<uint64_t>(0));
  }
}

inline Rep CONCAT(Frame* stack, JSVal* src, uint32_t count) {
  JSString* result = JSString::New(stack->ctx, src, count, IV_LV5_BREAKER_ERR);
  return Extract(result);
}

inline Rep RAISE(Frame* stack, Error::Code code, JSString* str) {
  stack->error->Report(code, str->GetUString());
  IV_LV5_BREAKER_RAISE();
  return 0;
}

inline Rep TYPEOF(Context* ctx, JSVal src) {
  return Extract(src.TypeOf(ctx));
}

inline Rep TO_PRIMITIVE_AND_TO_STRING(Frame* stack, JSVal src) {
  Context* ctx = stack->ctx;
  const JSVal primitive = src.ToPrimitive(ctx, Hint::NONE, IV_LV5_BREAKER_ERR);
  JSString* str = primitive.ToString(ctx, IV_LV5_BREAKER_ERR);
  return Extract(str);
}

template<int Type>
inline void STORE_OBJECT_INDEXED(
     Context* ctx, JSVal target, JSVal item, uint32_t index) {
  JSObject* obj = target.object();
  Slot slot;
  Error::Dummy dummy;
  switch (Type) {
    case ObjectLiteral::DATA:
      obj->DefineOwnIndexedPropertySlot(
          ctx, index,
          DataDescriptor(
              item,
              ATTR::W | ATTR::E| ATTR::C),
          &slot, false, &dummy);
      break;
    case ObjectLiteral::GET:
      obj->DefineOwnIndexedPropertySlot(
          ctx, index,
          AccessorDescriptor(
              item.object(),
              nullptr,
              ATTR::E| ATTR::C|
              ATTR::UNDEF_SETTER),
          &slot, false, &dummy);
      break;
    case ObjectLiteral::SET:
      obj->DefineOwnIndexedPropertySlot(
          ctx, index,
          AccessorDescriptor(
              nullptr,
              item.object(),
              ATTR::E| ATTR::C|
              ATTR::UNDEF_GETTER),
          &slot, false, &dummy);
      break;
  }
}

inline void STORE_OBJECT_GET(
    Context* ctx, JSVal target, JSVal item, uint32_t offset) {
  JSObject* obj = target.object();
  obj->Direct(offset) = JSVal::Cell(Accessor::New(ctx, item.object(), nullptr));
}

inline void STORE_OBJECT_SET(
    Context* ctx, JSVal target, JSVal item, uint32_t offset) {
  JSObject* obj = target.object();
  obj->Direct(offset) = JSVal::Cell(Accessor::New(ctx, nullptr, item.object()));
}

inline void INIT_VECTOR_ARRAY_ELEMENT(
    JSVal target, const JSVal* reg, uint32_t index, uint32_t size) {
  JSArray* ary = static_cast<JSArray*>(target.object());
  ary->SetToVector(index, reg, reg + size);
}

inline void INIT_SPARSE_ARRAY_ELEMENT(
    JSVal target, const JSVal* reg, uint32_t index, uint32_t size) {
  JSArray* ary = static_cast<JSArray*>(target.object());
  ary->SetToMap(index, reg, reg + size);
}

template<bool CONFIGURABLE>
inline Rep INSTANTIATE_DECLARATION_BINDING(
    Frame* stack, JSEnv* env, Symbol name) {
  Context* ctx = stack->ctx;
  if (!env->HasBinding(ctx, name)) {
    env->CreateMutableBinding(ctx, name, CONFIGURABLE, IV_LV5_BREAKER_ERR);
  } else if (env == ctx->global_env()) {
    JSObject* const go = ctx->global_obj();
    const PropertyDescriptor existing_prop = go->GetProperty(ctx, name);
    if (existing_prop.IsConfigurable()) {
      go->DefineOwnProperty(
          ctx,
          name,
          DataDescriptor(
              JSUndefined,
              ATTR::W | ATTR::E |
              ((CONFIGURABLE) ? ATTR::C : ATTR::NONE)),
          true, IV_LV5_BREAKER_ERR);
    } else {
      if (existing_prop.IsAccessor()) {
        stack->error->Report(Error::Type,
                             "create mutable function binding failed");
        IV_LV5_BREAKER_RAISE();
      }
      const DataDescriptor* const data = existing_prop.AsDataDescriptor();
      if (!data->IsWritable() || !data->IsEnumerable()) {
        stack->error->Report(Error::Type,
                             "create mutable function binding failed");
        IV_LV5_BREAKER_RAISE();
      }
    }
  }
  return 0;
}

template<bool CONFIGURABLE, bool Strict>
inline Rep INSTANTIATE_VARIABLE_BINDING(Frame* stack, JSEnv* env, Symbol name) {
  Context* ctx = stack->ctx;
  if (!env->HasBinding(ctx, name)) {
    env->CreateMutableBinding(ctx, name, CONFIGURABLE, IV_LV5_BREAKER_ERR);
    env->SetMutableBinding(ctx, name, JSUndefined, Strict, IV_LV5_BREAKER_ERR);
  }
  return 0;
}

inline void INITIALIZE_HEAP_IMMUTABLE(JSEnv* env, JSVal src, uint32_t offset) {
  static_cast<JSDeclEnv*>(env)->InitializeImmutableBinding(offset, src);
}

inline Rep INCREMENT(Frame* stack, JSVal src) {
  const double res = src.ToNumber(stack->ctx, IV_LV5_BREAKER_ERR);
  return Extract(res + 1);
}


inline Rep DECREMENT(Frame* stack, JSVal src) {
  const double res = src.ToNumber(stack->ctx, IV_LV5_BREAKER_ERR);
  return Extract(res - 1);
}

inline Rep POSTFIX_INCREMENT(Frame* stack, JSVal val, JSVal* src) {
  const double res = val.ToNumber(stack->ctx, IV_LV5_BREAKER_ERR);
  *src = res;
  return Extract(res + 1);
}

inline Rep POSTFIX_DECREMENT(Frame* stack,
                             JSVal val, JSVal* src) {
  const double res = val.ToNumber(stack->ctx, IV_LV5_BREAKER_ERR);
  *src = res;
  return Extract(res - 1);
}

inline bool TO_BOOLEAN(JSVal src) {
  return src.ToBoolean();
}

template<bool Strict>
inline Rep LOAD_ARGUMENTS(Frame* stack, railgun::Frame* frame) {
  if (Strict) {
    JSObject* obj = JSStrictArguments::New(
        stack->ctx,
        static_cast<JSFunction*>(frame->callee().object()),
        frame->arguments_crbegin(),
        frame->arguments_crend(),
        IV_LV5_BREAKER_ERR);
    return Extract(obj);
  } else {
    JSObject* obj = JSNormalArguments::New(
        stack->ctx,
        static_cast<JSFunction*>(frame->callee().object()),
        frame->code()->params(),
        frame->arguments_crbegin(),
        frame->arguments_crend(),
        static_cast<JSDeclEnv*>(frame->variable_env()),
        IV_LV5_BREAKER_ERR);
    return Extract(obj);
  }
}

inline Rep PREPARE_DYNAMIC_CALL(Frame* stack,
                                JSEnv* env,
                                Symbol name,
                                JSVal* base) {
  Context* ctx = stack->ctx;
  if (JSEnv* target_env = GetEnv(ctx, env, name)) {
    const JSVal res = target_env->GetBindingValue(ctx, name, false, IV_LV5_BREAKER_ERR);
    *base = target_env->ImplicitThisValue();
    return Extract(res);
  }
  RaiseReferenceError(name, stack->error);
  IV_LV5_BREAKER_RAISE();
}

template<bool Strict>
inline Rep LOAD_NAME(Frame* stack, JSEnv* env, Symbol name) {
  Context* ctx = stack->ctx;
  if (JSEnv* current = GetEnv(ctx, env, name)) {
    const JSVal res = current->GetBindingValue(ctx, name, Strict, IV_LV5_BREAKER_ERR);
    return Extract(res);
  }
  RaiseReferenceError(name, stack->error);
  IV_LV5_BREAKER_RAISE();
}

template<bool Strict>
inline Rep STORE_NAME(Frame* stack, JSEnv* env, Symbol name, JSVal src) {
  Context* ctx = stack->ctx;
  if (JSEnv* current = GetEnv(ctx, env, name)) {
    current->SetMutableBinding(ctx, name, src, Strict, IV_LV5_BREAKER_ERR);
  } else {
    if (Strict) {
      stack->error->Report(Error::Reference,
                           "putting to unresolvable reference "
                           "not allowed in strict reference");
      IV_LV5_BREAKER_RAISE();
    } else {
      ctx->global_obj()->Put(ctx, name, src, Strict, IV_LV5_BREAKER_ERR);
    }
  }
  return 0;
}

template<bool Strict>
inline Rep DELETE_NAME(Frame* stack, JSEnv* env, Symbol name) {
  Context* ctx = stack->ctx;
  if (JSEnv* current = GetEnv(ctx, env, name)) {
    return Extract(JSVal::Bool(current->DeleteBinding(ctx, name)));
  }
  return Extract(JSTrue);
}

template<bool Strict>
inline Rep TYPEOF_NAME(Frame* stack, JSEnv* env, Symbol name) {
  Context* ctx = stack->ctx;
  if (JSEnv* current = GetEnv(ctx, env, name)) {
    const JSVal res = current->GetBindingValue(ctx, name, Strict, IV_LV5_BREAKER_ERR);
    return Extract(res.TypeOf(ctx));
  }
  return Extract(ctx->global_data()->string_undefined());
}

inline Rep LOAD_ELEMENT(Frame* stack, JSVal base, JSVal element) {
  Context* ctx = stack->ctx;
  Slot slot;
  if (!base.IsObject() || !element.IsInt32() || element.int32() < 0) {
    const Symbol name = element.ToSymbol(ctx, IV_LV5_BREAKER_ERR);
    const JSVal res = base.GetSlot(ctx, name, &slot, IV_LV5_BREAKER_ERR);
    return Extract(res);
  }

  const uint32_t value = element.int32();
  JSObject* obj = base.object();
  const JSVal res = obj->GetIndexedSlot(ctx, value, &slot, IV_LV5_BREAKER_ERR);
  return Extract(res);
}

template<bool Strict>
void StorePropPrimitive(Context* ctx,
                        JSVal base, Symbol name, JSVal stored, Error* e) {
  assert(base.IsPrimitive());
  Slot slot;
  JSObject* const o = base.ToObject(ctx, IV_LV5_ERROR_VOID(e));
  if (!o->CanPut(ctx, name, &slot)) {
    if (Strict) {
      e->Report(Error::Type, "cannot put value to object");
    }
    return;
  }

  if (slot.IsNotFound() || slot.attributes().IsData()) {
    if (Strict) {
      e->Report(Error::Type, "value to symbol in transient object");
    }
    return;
  }

  const Accessor* ac = slot.accessor();
  assert(ac->setter());
  ScopedArguments args(ctx, 1, IV_LV5_ERROR_VOID(e));
  args[0] = stored;
  static_cast<JSFunction*>(ac->setter())->Call(&args, base, e);
}

template<bool Strict>
inline void StorePropImpl(Context* ctx,
                          JSVal base, Symbol name, JSVal stored, Error* e) {
  if (base.IsPrimitive()) {
    StorePropPrimitive<Strict>(ctx, base, name, stored, e);
  } else {
    base.object()->Put(ctx, name, stored, Strict, e);
  }
}

inline Rep STORE_ELEMENT_GENERIC(
    Frame* stack, JSVal base, JSVal src, JSVal element, StoreElementIC* ic) {
  Context* ctx = stack->ctx;
  if (base.IsPrimitive()) {
    const Symbol name = element.ToSymbol(ctx, IV_LV5_BREAKER_ERR);
    Slot slot;
    JSObject* const o = base.ToObject(ctx, IV_LV5_BREAKER_ERR);
    if (!o->CanPut(ctx, name, &slot)) {
      if (ic->strict()) {
        stack->error->Report(Error::Type, "cannot put value to object");
        IV_LV5_BREAKER_RAISE();
      }
      return 0;
    }

    if (slot.IsNotFound() || slot.attributes().IsData()) {
      if (ic->strict()) {
        stack->error->Report(
            Error::Type, "value to symbol in transient object");
        IV_LV5_BREAKER_RAISE();
      }
      return 0;
    }

    const Accessor* ac = slot.accessor();
    assert(ac->setter());
    ScopedArguments args(ctx, 1, IV_LV5_BREAKER_ERR);
    args[0] = src;
    static_cast<JSFunction*>(ac->setter())->Call(&args, base, IV_LV5_BREAKER_ERR);
    return 0;
  }

  JSObject* obj = base.object();

  if (!element.IsInt32() || element.int32() < 0) {
    const Symbol name = element.ToSymbol(ctx, IV_LV5_BREAKER_ERR);
    obj->Put(ctx, name, src, ic->strict(), IV_LV5_BREAKER_ERR);
    return 0;
  }

  const uint32_t index = element.int32();
  Slot slot;
  obj->PutIndexedSlot(ctx, index, src, &slot, ic->strict(), IV_LV5_BREAKER_ERR);
  return 0;
}

inline Rep STORE_ELEMENT_INDEXED(
    Frame* stack, JSVal base, JSVal src, int32_t index, StoreElementIC* ic) {
  Context* ctx = stack->ctx;
  assert(base.IsObject());
  JSObject* obj = base.object();

  if (index < 0) {
    std::array<char, 15> buffer;
    char* end = core::Int32ToString(index, buffer.data());
    const Symbol name =
        ctx->Intern(core::StringPiece(
                buffer.data(), std::distance(buffer.data(), end)));
    obj->Put(ctx, name, src, ic->strict(), IV_LV5_BREAKER_ERR);
    return 0;
  }

  const bool indexed = obj->map()->IsIndexed();
  Slot slot;
  JSObject::PutIndexedSlotMethod(
      obj, ctx, index, src, &slot, ic->strict(), IV_LV5_BREAKER_ERR);
  if (indexed && slot.put_result_type() == Slot::PUT_INDEXED_OPTIMIZED) {
    // ic to hole path
    Chain* chain = Chain::New(obj, nullptr);
    ic->StoreNewElement(chain);
  }
  return 0;
}

inline Rep STORE_ELEMENT(
    Frame* stack, JSVal base, JSVal src, JSVal element, StoreElementIC* ic) {
  Context* ctx = stack->ctx;
  if (base.IsPrimitive()) {
    ic->Invalidate();
    const Symbol name = element.ToSymbol(ctx, IV_LV5_BREAKER_ERR);
    Slot slot;
    JSObject* const o = base.ToObject(ctx, IV_LV5_BREAKER_ERR);
    if (!o->CanPut(ctx, name, &slot)) {
      if (ic->strict()) {
        stack->error->Report(Error::Type, "cannot put value to object");
        IV_LV5_BREAKER_RAISE();
      }
      return 0;
    }

    if (slot.IsNotFound() || slot.attributes().IsData()) {
      if (ic->strict()) {
        stack->error->Report(
            Error::Type, "value to symbol in transient object");
        IV_LV5_BREAKER_RAISE();
      }
      return 0;
    }

    const Accessor* ac = slot.accessor();
    assert(ac->setter());
    ScopedArguments args(ctx, 1, IV_LV5_BREAKER_ERR);
    args[0] = src;
    static_cast<JSFunction*>(ac->setter())->Call(&args, base, IV_LV5_BREAKER_ERR);
    return 0;
  }

  JSObject* obj = base.object();

  if (!element.IsInt32() || element.int32() < 0) {
    const Symbol name = element.ToSymbol(ctx, IV_LV5_BREAKER_ERR);
    obj->Put(ctx, name, src, ic->strict(), IV_LV5_BREAKER_ERR);
    return 0;
  }

  const bool indexed = obj->map()->IsIndexed();
  const uint32_t index = element.int32();
  Slot slot;
  obj->PutIndexedSlot(ctx, index, src, &slot, ic->strict(), IV_LV5_BREAKER_ERR);
  if (indexed && slot.put_result_type() == Slot::PUT_INDEXED_OPTIMIZED &&
      obj->method()->DefineOwnIndexedPropertySlot == JSObject::DefineOwnIndexedPropertySlotMethod) {  // NOLINT
    // ic to hole path
    Chain* chain = Chain::New(obj, nullptr);
    ic->StoreNewElement(chain);
  }
  return 0;
}


template<bool Strict>
inline Rep DELETE_ELEMENT(Frame* stack, JSVal base, JSVal element) {
  Context* ctx = stack->ctx;
  if (element.IsInt32()) {
    int32_t index = element.int32();
    if (index >= 0) {
      JSObject* const obj = base.ToObject(ctx, IV_LV5_BREAKER_ERR);
      const bool result = obj->DeleteIndexed(ctx, index, Strict, IV_LV5_BREAKER_ERR);
      return Extract(JSVal::Bool(result));
    }
  }
  const Symbol name = element.ToSymbol(ctx, IV_LV5_BREAKER_ERR);
  JSObject* const obj = base.ToObject(ctx, IV_LV5_BREAKER_ERR);
  const bool result = obj->Delete(ctx, name, Strict, IV_LV5_BREAKER_ERR);
  return Extract(JSVal::Bool(result));
}

template<int Target, std::size_t Returned, bool Strict>
inline Rep INCREMENT_ELEMENT(Frame* stack, JSVal base, JSVal element) {
  Context* ctx = stack->ctx;
  const Symbol name = element.ToSymbol(ctx, IV_LV5_BREAKER_ERR);
  Slot slot;
  const JSVal w = base.GetSlot(ctx, name, &slot, IV_LV5_BREAKER_ERR);
  if (w.IsInt32() &&
      railgun::detail::IsIncrementOverflowSafe<Target>(w.int32())) {
    std::tuple<JSVal, JSVal> results;
    const int32_t target = w.int32();
    std::get<0>(results) = w;
    std::get<1>(results) = JSVal::Int32(target + Target);
    StorePropImpl<Strict>(ctx, base, name, std::get<1>(results), IV_LV5_BREAKER_ERR);
    return Extract(std::get<Returned>(results));
  } else {
    std::tuple<double, double> results;
    std::get<0>(results) = w.ToNumber(ctx, IV_LV5_BREAKER_ERR);
    std::get<1>(results) = std::get<0>(results) + Target;
    StorePropImpl<Strict>(ctx, base, name, std::get<1>(results), IV_LV5_BREAKER_ERR);
    return Extract(JSVal(std::get<Returned>(results)));
  }
}

inline Rep LOAD_REGEXP(Context* ctx, JSRegExp* reg) {
  return Extract(JSRegExp::New(ctx, reg));
}

inline Rep LOAD_OBJECT(Context* ctx, Map* map) {
  return Extract(JSObject::New(ctx, map));
}

inline Rep LOAD_ARRAY(Context* ctx, uint32_t len) {
  return Extract(JSArray::ReservedNew(ctx, len));
}

inline Rep DUP_ARRAY(Context* ctx, const JSVal constant) {
  return Extract(JSArray::New(ctx, static_cast<JSArray*>(constant.object())));
}

inline JSEnv* TRY_CATCH_SETUP(Context* ctx,
                              JSEnv* outer, Symbol sym, JSVal value) {
  return JSStaticEnv::New(ctx, outer, sym, value);
}

inline Rep STORE_PROP_GENERIC(
    Frame* stack, JSVal base, JSVal src, StorePropertyIC* ic) {
  if (ic->strict()) {
    StorePropImpl<true>(stack->ctx, base, ic->name(), src, IV_LV5_BREAKER_ERR);
  } else {
    StorePropImpl<false>(stack->ctx, base, ic->name(), src, IV_LV5_BREAKER_ERR);
  }
  return 0;
}

inline Rep LOAD_PROP_GENERIC(Frame* stack, JSVal base, LoadPropertyIC* ic) {  // NOLINT
  Slot slot;
  const JSVal res = base.GetSlot(stack->ctx, ic->name(), &slot, IV_LV5_BREAKER_ERR);
  return Extract(res);
}

inline Rep LOAD_PROP(Frame* stack, JSVal base, LoadPropertyIC* ic) {  // NOLINT
  assert(!symbol::IsArrayIndexSymbol(ic->name()));
  Context* ctx = stack->ctx;
  Slot slot;
  const Symbol name = ic->name();

  // String / Array length fast path
  if (name == symbol::length()) {
    if (base.IsString()) {
      ic->LoadStringLength(ctx);
      return Extract(JSVal::UInt32(base.string()->size()));
    } else if (base.IsObject() && base.object()->IsClass<Class::Array>()) {
      ic->LoadArrayLength(ctx);
      return Extract(
          JSVal::UInt32(static_cast<JSArray*>(base.object())->length()));
    }
  }

  const JSVal res = base.GetSlot(ctx, name, &slot, IV_LV5_BREAKER_ERR);

  if (slot.IsNotFound()) {
    return Extract(res);
  }

  // property found

  // uncacheable path
  if (!slot.IsLoadCacheable() || !base.IsCell()) {
    // bailout to generic
    return Extract(res);
  }

  // object, string or symbol
  JSCell* cell = static_cast<JSCell*>(base.cell());
  assert(cell);

  // cache phase
  // own property / proto property / chain lookup property
  if (slot.base() == cell) {
    // own property
    ic->LoadOwnProperty(ctx, cell->map(), slot.offset());
    return Extract(res);
  }

  if (slot.base() == cell->prototype()) {
    // proto property
    cell->FlattenMap();
    ic->LoadPrototypeProperty(
        ctx, cell->map(), slot.base()->map(), slot.offset());
    return Extract(res);
  }

  // chain property
  ic->LoadChainProperty(
      ctx, Chain::New(cell, slot.base()), slot.base()->map(), slot.offset());
  return Extract(res);
}

inline Rep STORE_PROP(
    Frame* stack, JSVal base, JSVal src, StorePropertyIC* ic) {
  assert(!symbol::IsArrayIndexSymbol(ic->name()));
  Context* ctx = stack->ctx;
  const Symbol name = ic->name();
  if (base.IsPrimitive()) {
    if (ic->strict()) {
      StorePropPrimitive<true>(ctx, base, name, src, IV_LV5_BREAKER_ERR);
    } else {
      StorePropPrimitive<false>(ctx, base, name, src, IV_LV5_BREAKER_ERR);
    }
    return 0;
  }

  JSObject* obj = base.object();
  Map* previous = obj->map();
  Slot slot;
  obj->PutSlot(ctx, name, src, &slot, ic->strict(), IV_LV5_BREAKER_ERR);
  const Slot::PutResultType put_result_type = slot.put_result_type();
  const bool unique = previous->IsUnique() || obj->map()->IsUnique();

  // uncacheable pattern
  if (!slot.IsPutCacheable()) {
    return 0;
  }

  assert(put_result_type != Slot::PUT_NONE);

  // cache it, replace
  if (put_result_type == Slot::PUT_REPLACE) {
    if (previous == obj->map()) {
      // we can cache even if map is unique
      ic->StoreReplaceProperty(obj->map(), slot.offset());
    } else if (!unique) {
      ic->StoreReplacePropertyWithMapTransition(
          previous, obj->map(), slot.offset());
    }
    return 0;
  }

  // uncacheable
  if (unique) {
    return 0;
  }

  // cache it, new
  assert(previous != obj->map());

  Chain* chain = Chain::New(obj, nullptr);  // list up all maps
  (*chain)[0] = previous;  // first is previous

  if (previous->StorageCapacity() == obj->map()->StorageCapacity()) {
    // reallocation is not necessary
    ic->StoreNewProperty(chain, obj->map(), slot.offset());
    return 0;
  }

  ic->StoreNewPropertyWithReallocation(chain, obj->map(), slot.offset());
  return 0;
}

template<bool Strict>
inline Rep DELETE_PROP(Frame* stack, JSVal base, Symbol name) {
  Context* ctx = stack->ctx;
  JSObject* const obj = base.ToObject(ctx, IV_LV5_BREAKER_ERR);
  const bool res = obj->Delete(ctx, name, Strict, IV_LV5_BREAKER_ERR);
  return Extract(JSVal::Bool(res));
}

template<int Target, std::size_t Returned, bool Strict>
inline Rep INCREMENT_PROP(Frame* stack, JSVal base, Symbol name) {
  Context* ctx = stack->ctx;
  Slot slot;
  const JSVal w = base.GetSlot(ctx, name, &slot, IV_LV5_BREAKER_ERR);
  if (w.IsInt32() &&
      railgun::detail::IsIncrementOverflowSafe<Target>(w.int32())) {
    std::tuple<JSVal, JSVal> results;
    const int32_t target = w.int32();
    std::get<0>(results) = w;
    std::get<1>(results) = JSVal::Int32(target + Target);
    StorePropImpl<Strict>(ctx, base, name, std::get<1>(results), IV_LV5_BREAKER_ERR);
    return Extract(std::get<Returned>(results));
  } else {
    std::tuple<double, double> results;
    std::get<0>(results) = w.ToNumber(ctx, IV_LV5_BREAKER_ERR);
    std::get<1>(results) = std::get<0>(results) + Target;
    StorePropImpl<Strict>(ctx, base, name, std::get<1>(results), IV_LV5_BREAKER_ERR);
    return Extract(JSVal(std::get<Returned>(results)));
  }
}

#undef IV_LV5_BREAKER_ERR
#undef IV_LV5_BREAKER_RAISE
#undef IV_LV5_BREAKER_RAISE_PAIR
} } } }  // namespace iv::lv5::breaker::stub
#endif  // IV_LV5_BREAKER_STUB_H_
