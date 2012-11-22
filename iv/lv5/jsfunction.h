#ifndef IV_LV5_JSFUNCTION_H_
#define IV_LV5_JSFUNCTION_H_
#include <iv/lv5/context.h>
#include <iv/lv5/jsfunction_fwd.h>
namespace iv {
namespace lv5 {

inline JSFunction::JSFunction(Context* ctx)
  : JSObject(ctx->global_data()->function_map()) {
  set_callable(true);
}

inline void JSFunction::Initialize(Context* ctx) {
  set_cls(JSFunction::GetClass());
  set_prototype(ctx->global_data()->function_prototype());
}

inline JSBoundFunction::JSBoundFunction(
    Context* ctx, JSFunction* target, JSVal this_binding, const Arguments& args)
  : JSFunction(ctx),
    target_(target),
    this_binding_(this_binding),
    arguments_(args.empty() ? 0 : args.size() - 1) {
  Error::Dummy e;
  if (args.size() > 0) {
    std::copy(args.begin() + 1, args.end(), arguments_.begin());
  }
  const uint32_t bound_args_size = arguments_.size();
  set_cls(JSFunction::GetClass());
  set_prototype(ctx->global_data()->function_prototype());
  // step 15
  if (target_->IsClass<Class::Function>()) {
    // target [[Class]] is "Function"
    const JSVal length = target_->Get(ctx, symbol::length(), &e);
    assert(length.IsNumber());
    const uint32_t target_param_size = length.ToUInt32(ctx, &e);
    assert(target_param_size == length.number());
    const uint32_t len = (target_param_size >= bound_args_size) ?
        target_param_size - bound_args_size : 0;
    DefineOwnProperty(
        ctx, symbol::length(),
        DataDescriptor(JSVal::UInt32(len), ATTR::NONE), false, &e);
  } else {
    DefineOwnProperty(
        ctx, symbol::length(),
        DataDescriptor(JSVal::UInt32(0u), ATTR::NONE), false, &e);
  }
  JSFunction* const throw_type_error = ctx->throw_type_error();
  DefineOwnProperty(ctx, symbol::caller(),
                    AccessorDescriptor(throw_type_error,
                                       throw_type_error,
                                       ATTR::NONE),
                    false, &e);
  DefineOwnProperty(ctx, symbol::arguments(),
                    AccessorDescriptor(throw_type_error,
                                       throw_type_error,
                                       ATTR::NONE),
                    false, &e);
}

inline bool JSFunction::DefineOwnPropertySlot(Context* ctx,
                                              Symbol name,
                                              const PropertyDescriptor& desc,
                                              Slot* slot,
                                              bool th,
                                              Error* e) {
  return JSObject::DefineOwnPropertySlot(ctx, name, desc, slot, th, e);
}


inline JSVal JSFunction::GetSlot(Context* ctx, Symbol name, Slot* slot, Error* e) {
  const JSVal val = JSObject::GetSlot(ctx, name, slot, IV_LV5_ERROR(e));
  if (name == symbol::caller()) {
    slot->MakeUnCacheable();
    if (val.IsCallable() &&
        static_cast<JSFunction*>(val.object())->IsStrict()) {
      e->Report(Error::Type,
                "\"caller\" property is not accessible in strict code");
      return JSFalse;
    }
  }
  return val;
}

} }  // namespace iv::lv5
#endif  // IV_LV5_JSFUNCTION_H_
