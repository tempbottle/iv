#include <iv/lv5/error_check.h>
#include <iv/lv5/constructor_check.h>
#include <iv/lv5/arguments.h>
#include <iv/lv5/jsval.h>
#include <iv/lv5/context.h>
#include <iv/lv5/jsobject.h>
#include <iv/lv5/error.h>
#include <iv/lv5/jsweak_map.h>
#include <iv/lv5/runtime/weak_map.h>
namespace iv {
namespace lv5 {
namespace runtime {

// 15.15.2.1 WeakMap(iterable = [])
// 15.15.3.1 new WeakMap (iterable = [])
JSVal WeakMapConstructor(const Arguments& args, Error* e) {
  Context* ctx = args.ctx();
  const JSVal first = args.At(0);
  const JSVal this_binding = args.this_binding();
  JSObject* map = nullptr;
  if (this_binding.IsUndefined() ||
      JSVal::StrictEqual(this_binding, ctx->global_data()->weak_map_prototype())) {
    map = JSObject::New(
        ctx,
        ctx->global_data()->weak_map_map());
  } else {
    map = this_binding.ToObject(ctx, IV_LV5_ERROR(e));
  }
  return JSWeakMap::Initialize(ctx, map, first, IV_LV5_ERROR(e));
}

// 15.15.5.2 WeakMap.prototype.clear()
JSVal WeakMapClear(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("WeakMap.prototype.clear", args, e);
  Context* ctx = args.ctx();
  JSObject* map = args.this_binding().ToObject(ctx, IV_LV5_ERROR(e));
  Slot slot;
  if (!map->GetOwnPropertySlot(ctx, JSWeakMap::symbol(), &slot)) {
    e->Report(Error::Type, "WeakMap.prototype.clear is not generic function");
    return JSEmpty;
  }
  JSWeakMap::Data* entries = static_cast<JSWeakMap::Data*>(slot.value().cell());
  entries->Clear();
  return JSUndefined;
}

// 15.15.5.3 WeakMap.prototype.delete(key)
JSVal WeakMapDelete(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("WeakMap.prototype.delete", args, e);
  Context* ctx = args.ctx();
  JSObject* map = args.this_binding().ToObject(ctx, IV_LV5_ERROR(e));
  Slot slot;
  if (!map->GetOwnPropertySlot(ctx, JSWeakMap::symbol(), &slot)) {
    e->Report(Error::Type, "WeakMap.prototype.delete is not generic function");
    return JSEmpty;
  }
  JSWeakMap::Data* entries = static_cast<JSWeakMap::Data*>(slot.value().cell());
  JSObject* cell = args.At(0).ToObject(ctx, IV_LV5_ERROR(e));
  return JSVal::Bool(entries->Delete(cell));
}

// 15.15.5.4 WeakMap.prototype.get(key)
JSVal WeakMapGet(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("WeakMap.prototype.get", args, e);
  Context* ctx = args.ctx();
  JSObject* map = args.this_binding().ToObject(ctx, IV_LV5_ERROR(e));
  Slot slot;
  if (!map->GetOwnPropertySlot(ctx, JSWeakMap::symbol(), &slot)) {
    e->Report(Error::Type, "WeakMap.prototype.get is not generic function");
    return JSEmpty;
  }
  JSWeakMap::Data* entries = static_cast<JSWeakMap::Data*>(slot.value().cell());
  JSObject* cell = args.At(0).ToObject(ctx, IV_LV5_ERROR(e));
  return entries->Get(cell);
}

// 15.15.5.5 WeakMap.prototype.has(key)
JSVal WeakMapHas(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("WeakMap.prototype.has", args, e);
  Context* ctx = args.ctx();
  JSObject* map = args.this_binding().ToObject(ctx, IV_LV5_ERROR(e));
  Slot slot;
  if (!map->GetOwnPropertySlot(ctx, JSWeakMap::symbol(), &slot)) {
    e->Report(Error::Type, "WeakMap.prototype.has is not generic function");
    return JSEmpty;
  }
  JSWeakMap::Data* entries = static_cast<JSWeakMap::Data*>(slot.value().cell());
  JSObject* cell = args.At(0).ToObject(ctx, IV_LV5_ERROR(e));
  return JSVal::Bool(entries->Has(cell));
}

// 15.15.5.6 WeakMap.prototype.set(key, value)
JSVal WeakMapSet(const Arguments& args, Error* e) {
  IV_LV5_CONSTRUCTOR_CHECK("WeakMap.prototype.set", args, e);
  Context* ctx = args.ctx();
  JSObject* map = args.this_binding().ToObject(ctx, IV_LV5_ERROR(e));
  Slot slot;
  if (!map->GetOwnPropertySlot(ctx, JSWeakMap::symbol(), &slot)) {
    e->Report(Error::Type, "WeakMap.prototype.set is not generic function");
    return JSEmpty;
  }
  JSWeakMap::Data* entries = static_cast<JSWeakMap::Data*>(slot.value().cell());
  JSObject* cell = args.At(0).ToObject(ctx, IV_LV5_ERROR(e));
  entries->Set(cell, args.At(1));
  return JSUndefined;
}

} } }  // namespace iv::lv5::runtime
