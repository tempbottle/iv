#include <cmath>
#include <cstdio>
#include <cstring>
#include <iv/detail/array.h>
#include <iv/ustring.h>
#include <iv/dtoa.h>
#include <iv/canonicalized_nan.h>
#include <iv/concat.h>
#include <iv/lv5/jsmath.h>
#include <iv/lv5/jsenv.h>
#include <iv/lv5/jsjson.h>
#include <iv/lv5/jsmap.h>
#include <iv/lv5/jsweak_map.h>
#include <iv/lv5/jsset.h>
#include <iv/lv5/jsglobal.h>
#include <iv/lv5/jsfunction.h>
#include <iv/lv5/jsarguments.h>
#include <iv/lv5/jsdate.h>
#include <iv/lv5/jsnumberobject.h>
#include <iv/lv5/jsbooleanobject.h>
#include <iv/lv5/jsreflect.h>
#include <iv/lv5/jssymbol.h>
#include <iv/lv5/jsstring.h>
#include <iv/lv5/jsregexp.h>
#include <iv/lv5/jsi18n.h>
#include <iv/lv5/arguments.h>
#include <iv/lv5/context.h>
#include <iv/lv5/property.h>
#include <iv/lv5/class.h>
#include <iv/lv5/runtime/runtime.h>
#include <iv/lv5/jserror.h>
#include <iv/lv5/bind.h>
#include <iv/lv5/internal.h>
#include <iv/lv5/radio/radio.h>
#include <iv/lv5/jsarray_buffer.h>
#include <iv/lv5/jsdata_view.h>
#include <iv/lv5/jsobject.h>
namespace iv {
namespace lv5 {

Context::Context(JSAPI fc, JSAPI ge)
  : global_data_(this),
    throw_type_error_(
        JSInlinedFunction<&runtime::ThrowTypeError, 0>::NewPlain(
            this, Intern("ThrowError"),
            Map::NewUniqueMap(this, static_cast<JSObject*>(nullptr), false))),
    global_env_(JSObjectEnv::New(this, nullptr, global_obj())),
    regexp_allocator_(),
    regexp_vm_(),
    stack_(nullptr),
    function_constructor_(fc),
    global_eval_(ge),
    i18n_() {
  Initialize();
}

void Context::Initialize() {
  Error::Dummy dummy;
  // Object and Function
  JSObject* const obj_proto =
      JSObject::NewPlain(this,
                         Map::NewUniqueMap(
                             this, static_cast<JSObject*>(nullptr), false));

  // Object Map initializations
  global_data()->empty_object_map()->ChangePrototypeWithNoTransition(obj_proto);
  global_data()->InitNormalObjectMaps(this);

  global_obj()->ChangePrototype(this, obj_proto);

  JSFunction* const func_proto =
      JSInlinedFunction<&runtime::FunctionPrototype, 0>::NewPlain(
          this,
          Intern("Function"),
          Map::NewUniqueMap(this, obj_proto, false));
  global_data()->function_map()->ChangePrototypeWithNoTransition(func_proto);

  JSFunction* const obj_constructor =
      JSInlinedFunction<&runtime::ObjectConstructor, 1>::New(
          this,
          Intern("Object"));

  JSFunction* func_constructor =
      JSNativeFunction::NewPlain(
          this, function_constructor_, 1, Intern("Function"));
  JSFunction* eval_function =
      JSNativeFunction::NewPlain(this, global_eval_, 1, symbol::eval());

  // Function.prototype is used soon, so register it very early phase
  struct ClassSlot func_cls = {
    JSFunction::GetClass(),
    Intern("Function"),
    JSString::NewAsciiString(this, "Function", &dummy),
    func_constructor,
    func_proto
  };
  global_data()->RegisterClass<Class::Function>(func_cls);
  global_data()->set_function_prototype(func_proto);

  struct ClassSlot obj_cls = {
    JSObject::GetClass(),
    Intern("Object"),
    JSString::NewAsciiString(this, "Object", &dummy),
    obj_constructor,
    obj_proto
  };
  global_data_.RegisterClass<Class::Object>(obj_cls);

  bind::Object(this, func_constructor)
      .cls(func_cls.cls)
      // seciton 15.3.3.1 Function.prototype
      .def(symbol::prototype(), func_proto, ATTR::NONE);

  bind::Object(this, obj_constructor)
      .cls(func_cls.cls)
      // seciton 15.2.3.1 Object.prototype
      .def(symbol::prototype(), obj_proto, ATTR::NONE)
      // section 15.2.3.2 Object.getPrototypeOf(O)
      .def<&runtime::ObjectGetPrototypeOf, 1>("getPrototypeOf")
      // section 15.2.3.3 Object.getOwnPropertyDescriptor(O, P)
      .def<
        &runtime::ObjectGetOwnPropertyDescriptor, 2>("getOwnPropertyDescriptor")
      // section 15.2.3.4 Object.getOwnPropertyNames(O)
      .def<&runtime::ObjectGetOwnPropertyNames, 1>("getOwnPropertyNames")
      // section 15.2.3.5 Object.create(O[, Properties])
      .def<&runtime::ObjectCreate, 2>("create")
      // section 15.2.3.6 Object.defineProperty(O, P, Attributes)
      .def<&runtime::ObjectDefineProperty, 3>("defineProperty")
      // section 15.2.3.7 Object.defineProperties(O, Properties)
      .def<&runtime::ObjectDefineProperties, 2>("defineProperties")
      // section 15.2.3.8 Object.seal(O)
      .def<&runtime::ObjectSeal, 1>("seal")
      // section 15.2.3.9 Object.freeze(O)
      .def<&runtime::ObjectFreeze, 1>("freeze")
      // section 15.2.3.10 Object.preventExtensions(O)
      .def<&runtime::ObjectPreventExtensions, 1>("preventExtensions")
      // section 15.2.3.11 Object.isSealed(O)
      .def<&runtime::ObjectIsSealed, 1>("isSealed")
      // section 15.2.3.12 Object.isFrozen(O)
      .def<&runtime::ObjectIsFrozen, 1>("isFrozen")
      // section 15.2.3.13 Object.isExtensible(O)
      .def<&runtime::ObjectIsExtensible, 1>("isExtensible")
      // section 15.2.3.14 Object.keys(O)
      .def<&runtime::ObjectKeys, 1>("keys")
      // ES.next Object.is(x, y)
      .def<&runtime::ObjectIs, 2>("is");

  func_proto->ChangePrototype(this, obj_proto);
  bind::Object(this, func_proto)
      .cls(func_cls.cls)
      // section 15.3.4.1 Function.prototype.constructor
      .def(symbol::constructor(), func_constructor, ATTR::W | ATTR::C)
      // section 15.3.4.2 Function.prototype.toString()
      .def<&runtime::FunctionToString, 0>(symbol::toString())
      // section 15.3.4.3 Function.prototype.apply(thisArg, argArray)
      .def<&runtime::FunctionApply, 2>("apply")
      // section 15.3.4.4 Function.prototype.call(thisArg[, arg1[, arg2, ...]])
      .def<&runtime::FunctionCall, 1>("call")
      // section 15.3.4.5 Function.prototype.bind(thisArg[, arg1[, arg2, ...]])
      .def<&runtime::FunctionBind, 1>("bind");

  bind::Object(this, obj_proto)
      .cls(obj_cls.cls)
      // section 15.2.4.1 Object.prototype.constructor
      .def(symbol::constructor(), obj_constructor, ATTR::W | ATTR::C)
      // section 15.2.4.2 Object.prototype.toString()
      .def<&runtime::ObjectToString, 0>(symbol::toString())
      // section 15.2.4.3 Object.prototype.toLocaleString()
      .def<&runtime::ObjectToLocaleString, 0>("toLocaleString")
      // section 15.2.4.4 Object.prototype.valueOf()
      .def<&runtime::ObjectValueOf, 0>("valueOf")
      // section 15.2.4.5 Object.prototype.hasOwnProperty(V)
      .def<&runtime::ObjectHasOwnProperty, 1>("hasOwnProperty")
      // section 15.2.4.6 Object.prototype.isPrototypeOf(V)
      .def<&runtime::ObjectIsPrototypeOf, 1>("isPrototypeOf")
      // section 15.2.4.7 Object.prototype.propertyIsEnumerable(V)
      .def<&runtime::ObjectPropertyIsEnumerable, 1>("propertyIsEnumerable");
  global_data()->set_object_prototype(obj_proto);

  bind::Object global_binder(this, global_obj());
  global_binder.def(func_cls.name, func_constructor, ATTR::W | ATTR::C);
  global_binder.def(obj_cls.name, obj_constructor, ATTR::W | ATTR::C);

  // lazy update
  throw_type_error_->ChangePrototype(this, global_data()->function_prototype());

  // section 15.1 Global
  InitGlobal(func_cls, obj_proto, eval_function, &global_binder);
  // section 15.4 Array
  InitArray(func_cls, obj_proto, &global_binder);
  // section 15.5 String
  InitString(func_cls, obj_proto, &global_binder);
  // section 15.6 Boolean
  InitBoolean(func_cls, obj_proto, &global_binder);
  // section 15.7 Number
  InitNumber(func_cls, obj_proto, &global_binder);
  // section 15.8 Math
  InitMath(func_cls, obj_proto, &global_binder);
  // section 15.9 Date
  InitDate(func_cls, obj_proto, &global_binder);
  // section 15.10 RegExp
  InitRegExp(func_cls, obj_proto, &global_binder);
  // section 15.11 Error
  InitError(func_cls, obj_proto, &global_binder);
  // section 15.12 JSON
  InitJSON(func_cls, obj_proto, &global_binder);

  // ES.next
  InitIntl(func_cls, obj_proto, &global_binder);
  InitMap(func_cls, obj_proto, &global_binder);
  InitWeakMap(func_cls, obj_proto, &global_binder);
  InitSet(func_cls, obj_proto, &global_binder);
  InitBinaryBlocks(func_cls, obj_proto, &global_binder);
  InitReflect(func_cls, obj_proto, &global_binder);
  InitSymbol(func_cls, obj_proto, &global_binder);

  {
    // Arguments
    struct ClassSlot cls = {
      JSArguments::GetClass(),
      Intern("Arguments"),
      JSString::NewAsciiString(this, "Arguments", &dummy),
      nullptr,
      obj_proto
    };
    global_data_.RegisterClass<Class::Arguments>(cls);
    global_data()->InitArgumentsMap();
  }
}

void Context::InitGlobal(const ClassSlot& func_cls,
                         JSObject* obj_proto, JSFunction* eval_function,
                         bind::Object* global_binder) {
  // Global
  Error::Dummy dummy;
  struct ClassSlot cls = {
    JSGlobal::GetClass(),
    symbol::global(),
    JSString::NewAsciiString(this, "global", &dummy),
    nullptr,
    obj_proto
  };
  global_data_.RegisterClass<Class::global>(cls);

  global_binder->content()->ChangePrototype(this, cls.prototype);
  global_binder->cls(cls.cls)
      // section 15.1.1.1 NaN
      .def(symbol::NaN(), core::kNaN)
      // section 15.1.1.2 Infinity
      .def(symbol::Infinity(), std::numeric_limits<double>::infinity())
      // section 15.1.1.3 undefined
      .def("undefined", JSUndefined)
      // section 15.1.2.1 eval(x)
      .def("eval", eval_function, ATTR::W | ATTR::C)
      // section 15.1.2.3 parseIng(string, radix)
      .def<&runtime::GlobalParseInt, 2>("parseInt")
      // section 15.1.2.3 parseFloat(string)
      .def<&runtime::GlobalParseFloat, 1>("parseFloat")
      // section 15.1.2.4 isNaN(number)
      .def<&runtime::GlobalIsNaN, 1>("isNaN")
      // section 15.1.2.5 isFinite(number)
      .def<&runtime::GlobalIsFinite, 1>("isFinite")
      // section 15.1.3.1 decodeURI(encodedURI)
      .def<&runtime::GlobalDecodeURI, 1>("decodeURI")
      // section 15.1.3.2 decodeURIComponent(encodedURIComponent)
      .def<&runtime::GlobalDecodeURIComponent, 1>("decodeURIComponent")
      // section 15.1.3.3 encodeURI(uri)
      .def<&runtime::GlobalEncodeURI, 1>("encodeURI")
      // section 15.1.3.4 encodeURIComponent(uriComponent)
      .def<&runtime::GlobalEncodeURIComponent, 1>("encodeURIComponent")
      // section B.2.1 escape(string)
      // this method is deprecated.
      .def<&runtime::GlobalEscape, 1>("escape")
      // section B.2.2 unescape(string)
      // this method is deprecated.
      .def<&runtime::GlobalUnescape, 1>("unescape");
}

void Context::InitArray(const ClassSlot& func_cls,
                        JSObject* obj_proto, bind::Object* global_binder) {
  // section 15.4 Array
  Error::Dummy dummy;
  JSObject* const proto =
      JSArray::NewPlain(this, Map::NewUniqueMap(this, obj_proto, false));
  global_data()->array_map()->ChangePrototypeWithNoTransition(proto);
  // section 15.4.2 The Array Constructor
  JSFunction* const constructor =
      JSInlinedFunction<&runtime::ArrayConstructor, 1>::New(
          this, Intern("Array"));

  struct ClassSlot cls = {
    JSArray::GetClass(),
    Intern("Array"),
    JSString::NewAsciiString(this, "Array", &dummy),
    constructor,
    proto
  };
  global_data_.RegisterClass<Class::Array>(cls);
  global_binder->def(cls.name, constructor, ATTR::W | ATTR::C);

  bind::Object(this, constructor)
      // prototype
      .def(symbol::prototype(), proto, ATTR::NONE)
      // section 15.4.3.2 Array.isArray(arg)
      .def<&runtime::ArrayIsArray, 1>("isArray")
      // ES.next Array.from
      .def<&runtime::ArrayFrom, 1>("from")
      // ES.next Array.from
      .def<&runtime::ArrayOf, 0>("of");


  JSFunction* array_values =
     JSInlinedFunction<&runtime::ArrayValues, 0>::New(this, Intern("values"));

  JSVector* unscopables = JSVector::New(this, {
    JSString::NewAsciiString(this, "find", &dummy),
    JSString::NewAsciiString(this, "findIndex", &dummy),
    JSString::NewAsciiString(this, "fill", &dummy),
    JSString::NewAsciiString(this, "copyWithin", &dummy),
    JSString::NewAsciiString(this, "entries", &dummy),
    JSString::NewAsciiString(this, "keys", &dummy),
    JSString::NewAsciiString(this, "values", &dummy)
  });

  bind::Object(this, proto)
      .cls(cls.cls)
      // section 15.5.4.1 Array.prototype.constructor
      .def(symbol::constructor(), constructor, ATTR::W | ATTR::C)
      // section 15.4.4.2 Array.prototype.toString()
      .def<&runtime::ArrayToString, 0>("toString")
      // section 15.4.4.3 Array.prototype.toLocaleString()
      .def<&runtime::ArrayToLocaleString, 0>("toLocaleString")
      // section 15.4.4.4 Array.prototype.concat([item1[, item2[, ...]]])
      .def<&runtime::ArrayConcat, 1>("concat")
      // section 15.4.4.5 Array.prototype.join(separator)
      .def<&runtime::ArrayJoin, 1>("join")
      // section 15.4.4.6 Array.prototype.pop()
      .def<&runtime::ArrayPop, 0>("pop")
      // section 15.4.4.7 Array.prototype.push([item1[, item2[, ...]]])
      .def<&runtime::ArrayPush, 1>("push")
      // section 15.4.4.8 Array.prototype.reverse()
      .def<&runtime::ArrayReverse, 0>("reverse")
      // section 15.4.4.9 Array.prototype.shift()
      .def<&runtime::ArrayShift, 0>("shift")
      // section 15.4.4.10 Array.prototype.slice(start, end)
      .def<&runtime::ArraySlice, 2>("slice")
      // section 15.4.4.11 Array.prototype.sort(comparefn)
      .def<&runtime::ArraySort, 1>("sort")
      // section 15.4.4.12
      // Array.prototype.splice(start, deleteCount[, item1[, item2[, ...]]])
      .def<&runtime::ArraySplice, 2>("splice")
      // section 15.4.4.13 Array.prototype.unshift([item1[, item2[, ...]]])
      .def<&runtime::ArrayUnshift, 1>("unshift")
      // section 15.4.4.14 Array.prototype.indexOf(searchElement[, fromIndex])
      .def<&runtime::ArrayIndexOf, 1>("indexOf")
      // section 15.4.4.15
      // Array.prototype.lastIndexOf(searchElement[, fromIndex])
      .def<&runtime::ArrayLastIndexOf, 1>("lastIndexOf")
      // section 15.4.4.16 Array.prototype.every(callbackfn[, thisArg])
      .def<&runtime::ArrayEvery, 1>("every")
      // section 15.4.4.17 Array.prototype.some(callbackfn[, thisArg])
      .def<&runtime::ArraySome, 1>("some")
      // section 15.4.4.18 Array.prototype.forEach(callbackfn[, thisArg])
      .def<&runtime::ArrayForEach, 1>("forEach")
      // section 15.4.4.19 Array.prototype.map(callbackfn[, thisArg])
      .def<&runtime::ArrayMap, 1>("map")
      // section 15.4.4.20 Array.prototype.filter(callbackfn[, thisArg])
      .def<&runtime::ArrayFilter, 1>("filter")
      // section 15.4.4.21 Array.prototype.reduce(callbackfn[, initialValue])
      .def<&runtime::ArrayReduce, 1>("reduce")
      // section 15.4.4.22
      // Array.prototype.reduceRight(callbackfn[, initialValue])
      .def<&runtime::ArrayReduceRight, 1>("reduceRight")
      // ES6
      // section 22.1.3.4 Array.prototype.entries()
      .def<&runtime::ArrayEntries, 0>("entries")
      // ES6
      // section 22.1.3.13 Array.prototype.keys()
      .def<&runtime::ArrayKeys, 0>("keys")
      // ES6
      // section 22.1.3.29 Array.prototype.values()
      .def("values", array_values, ATTR::W | ATTR::C)
      // ES6
      // section 21.1.3.30 Array.prototype[@@iterator]()
      .def(global_data()->builtin_symbol_iterator(),
           array_values, ATTR::W | ATTR::C)
      // ES6
      // section 22.1.3.31 Array.prototype[@@unscopables]
      .def(global_data()->builtin_symbol_unscopables(),
           unscopables->ToJSArray(), ATTR::W | ATTR::C);
  global_data()->set_array_prototype(proto);

  // Init ArrayIterator
  JSObject* const array_iterator_proto = JSObject::New(this);
  global_data()->array_iterator_map()->ChangePrototypeWithNoTransition(
      array_iterator_proto);
  bind::Object(this, array_iterator_proto)
      // ES6
      // section 22.1.5.2.1 %ArrayIteratorPrototype%.next()
      .def<&runtime::ArrayIteratorNext, 0>("next")
      // ES6
      // section 22.1.5.2.2 %ArrayIteratorPrototype%[@@iterator]()
      .def(global_data()->builtin_symbol_toPrimitive(),
           JSInlinedFunction<
             &runtime::ArrayIteratorIterator, 0>::New(
                 this, Intern("[Symbol.iterator]")),
             ATTR::W | ATTR::C)
      // ES6
      // section 22.1.5.2.3 %ArrayIteratorPrototype%[@@toStringTag]
      .def(global_data()->builtin_symbol_toStringTag(),
           JSString::NewAsciiString(this, "Array Iterator", &dummy),
           ATTR::W | ATTR::C);
  global_data()->set_array_iterator_prototype(array_iterator_proto);
}

void Context::InitString(const ClassSlot& func_cls,
                         JSObject* obj_proto, bind::Object* global_binder) {
  // section 15.5 String
  Error::Dummy dummy;
  JSStringObject* const proto =
      JSStringObject::NewPlain(this, Map::NewUniqueMap(this, obj_proto, false));
  global_data()->string_map()->ChangePrototypeWithNoTransition(proto);
  global_data()->primitive_string_map()->ChangePrototypeWithNoTransition(proto);
  // section 15.5.2 The String Constructor
  JSFunction* const constructor =
      JSInlinedFunction<&runtime::StringConstructor, 1>::New(
          this, Intern("String"));

  struct ClassSlot cls = {
    JSStringObject::GetClass(),
    Intern("String"),
    JSString::NewAsciiString(this, "String", &dummy),
    constructor,
    proto
  };
  global_data_.RegisterClass<Class::String>(cls);
  global_binder->def(cls.name, constructor, ATTR::W | ATTR::C);

  bind::Object(this, constructor)
      // prototype
      .def(symbol::prototype(), proto, ATTR::NONE)
      // section 15.5.3.2 String.fromCharCode([char0 [, char1[, ...]]])
      .def<&runtime::StringFromCharCode, 1>("fromCharCode")
      // ES6 section 15.5.3.3 String.fromCodePoint(...codePoints)
      .def<&runtime::StringFromCodePoint, 1>("fromCodePoint")
      // ES6 section 15.5.3.4 String.raw(callSite, ...substitutions)
      .def<&runtime::StringRaw, 1>("raw");

  bind::Object(this, proto)
      .cls(cls.cls)
      // section 15.5.4.1 String.prototype.constructor
      .def(symbol::constructor(), constructor, ATTR::W | ATTR::C)
      // section 15.5.4.2 String.prototype.toString()
      .def<&runtime::StringToString, 0>("toString")
      // section 15.5.4.3 String.prototype.valueOf()
      .def<&runtime::StringValueOf, 0>("valueOf")
      // section 15.5.4.4 String.prototype.charAt(pos)
      .def<&runtime::StringCharAt, 1>("charAt")
      // section 15.5.4.5 String.prototype.charCodeAt(pos)
      .def<&runtime::StringCharCodeAt, 1>("charCodeAt")
      // ES6 section 15.5.4.6 String.prototype.codePointAt(pos)
      .def<&runtime::StringCodePointAt, 1>("codePointAt")
      // section 15.5.4.6 String.prototype.concat([string1[, string2[, ...]]])
      .def<&runtime::StringConcat, 1>("concat")
      // section 15.5.4.7 String.prototype.indexOf(searchString, position)
      .def<&runtime::StringIndexOf, 1>("indexOf")
      // section 15.5.4.8 String.prototype.lastIndexOf(searchString, position)
      .def<&runtime::StringLastIndexOf, 1>("lastIndexOf")
      // section 15.5.4.9 String.prototype.localeCompare(that)
      .def<&runtime::StringLocaleCompare, 1>("localeCompare")
      // section 15.5.4.10 String.prototype.match(regexp)
      .def<&runtime::StringMatch, 1>("match")
      // section 15.5.4.11 String.prototype.replace(searchValue, replaceValue)
      .def<&runtime::StringReplace, 2>("replace")
      // section 15.5.4.12 String.prototype.search(regexp)
      .def<&runtime::StringSearch, 1>("search")
      // section 15.5.4.13 String.prototype.slice(start, end)
      .def<&runtime::StringSlice, 2>("slice")
      // section 15.5.4.14 String.prototype.split(separator, limit)
      .def<&runtime::StringSplit, 2>("split")
      // section 15.5.4.15 String.prototype.substring(start, end)
      .def<&runtime::StringSubstring, 2>("substring")
      // section 15.5.4.16 String.prototype.toLowerCase()
      .def<&runtime::StringToLowerCase, 0>("toLowerCase")
      // section 15.5.4.17 String.prototype.toLocaleLowerCase()
      .def<&runtime::StringToLocaleLowerCase, 0>("toLocaleLowerCase")
      // section 15.5.4.18 String.prototype.toUpperCase()
      .def<&runtime::StringToUpperCase, 0>("toUpperCase")
      // section 15.5.4.19 String.prototype.toLocaleUpperCase()
      .def<&runtime::StringToLocaleUpperCase, 0>("toLocaleUpperCase")
      // section 15.5.4.20 String.prototype.trim()
      .def<&runtime::StringTrim, 0>("trim")
      // section 15.5.4.21 String.prototype.repeat(count)
      .def<&runtime::StringRepeat, 1>("repeat")
      // section 15.5.4.22 String.prototype.startsWith(searchString, [position])
      .def<&runtime::StringStartsWith, 1>("startsWith")
      // section 15.5.4.23
      // String.prototype.endsWith(searchString, [endPosition])
      .def<&runtime::StringEndsWith, 1>("endsWith")
      // section 15.5.4.24 String.prototype.contains(searchString, [position])
      .def<&runtime::StringContains, 1>("contains")
      // section 15.5.4.25 String.prototype.reverse()
      .def<&runtime::StringReverse, 0>("reverse")
      // section B.2.3 String.prototype.substr(start, length)
      // this method is deprecated.
      .def<&runtime::StringSubstr, 2>("substr")
      // ES6
      // section 21.1.3.27 String.prototype[@@iterator]()
      .def(global_data()->builtin_symbol_iterator(),
           JSInlinedFunction<
             &runtime::StringIterator, 0>::New(
                 this, Intern("[Symbol.iterator]")),
             ATTR::W | ATTR::C);
  global_data()->set_string_prototype(proto);

  // Init StringIterator
  JSObject* const string_iterator_proto = JSObject::New(this);
  global_data()->string_iterator_map()->ChangePrototypeWithNoTransition(
      string_iterator_proto);
  bind::Object(this, string_iterator_proto)
      // ES6
      // section 21.1.5.2.1 %StringIteratorPrototype%.next()
      .def<&runtime::StringIteratorNext, 0>("next")
      // ES6
      // section 21.1.5.2.2 %StringIteratorPrototype%[@@iterator]()
      .def(global_data()->builtin_symbol_toPrimitive(),
           JSInlinedFunction<
             &runtime::StringIteratorIterator, 0>::New(
                 this, Intern("[Symbol.iterator]")),
             ATTR::W | ATTR::C)
      // ES6
      // section 21.1.5.2.3 %StringIteratorPrototype%[@@toStringTag]
      .def(global_data()->builtin_symbol_toStringTag(),
           JSString::NewAsciiString(this, "String Iterator", &dummy),
           ATTR::W | ATTR::C);
  global_data()->set_string_iterator_prototype(string_iterator_proto);
}

void Context::InitBoolean(const ClassSlot& func_cls,
                          JSObject* obj_proto, bind::Object* global_binder) {
  // Boolean
  Error::Dummy dummy;
  JSBooleanObject* const proto =
      JSBooleanObject::NewPlain(
          this, Map::NewUniqueMap(this, obj_proto, false), false);
  global_data()->boolean_map()->ChangePrototypeWithNoTransition(proto);
  // section 15.6.2 The Boolean Constructor
  JSFunction* const constructor =
      JSInlinedFunction<&runtime::BooleanConstructor, 1>::New(
          this, Intern("Boolean"));

  struct ClassSlot cls = {
    JSBooleanObject::GetClass(),
    Intern("Boolean"),
    JSString::NewAsciiString(this, "Boolean", &dummy),
    constructor,
    proto
  };
  global_data_.RegisterClass<Class::Boolean>(cls);
  global_binder->def(cls.name, constructor, ATTR::W | ATTR::C);

  bind::Object(this, constructor)
      // section 15.6.3.1 Boolean.prototype
      .def(symbol::prototype(), proto, ATTR::NONE);

  bind::Object(this, proto)
      .cls(cls.cls)
      // section 15.6.4.1 Boolean.prototype.constructor
      .def(symbol::constructor(), constructor, ATTR::W | ATTR::C)
      // section 15.6.4.2 Boolean.prototype.toString()
      .def<&runtime::BooleanToString, 0>(symbol::toString())
      // section 15.6.4.3 Boolean.prototype.valueOf()
      .def<&runtime::BooleanValueOf, 0>(symbol::valueOf());
  global_data()->set_boolean_prototype(proto);
}

void Context::InitNumber(const ClassSlot& func_cls,
                         JSObject* obj_proto, bind::Object* global_binder) {
  // 15.7 Number
  Error::Dummy dummy;
  JSNumberObject* const proto =
      JSNumberObject::NewPlain(
          this, Map::NewUniqueMap(this, obj_proto, false), 0);
  global_data()->number_map()->ChangePrototypeWithNoTransition(proto);
  // section 15.7.3 The Number Constructor
  JSFunction* const constructor =
      JSInlinedFunction<&runtime::NumberConstructor, 1>::New(
          this, Intern("Number"));

  struct ClassSlot cls = {
    JSNumberObject::GetClass(),
    Intern("Number"),
    JSString::NewAsciiString(this, "Number", &dummy),
    constructor,
    proto
  };
  global_data_.RegisterClass<Class::Number>(cls);
  global_binder->def(cls.name, constructor, ATTR::W | ATTR::C);

  bind::Object(this, constructor)
      // section 15.7.3.1 Number.prototype
      .def(symbol::prototype(), proto, ATTR::NONE)
      // section 15.7.3.2 Number.MAX_VALUE
      .def("MAX_VALUE", 1.7976931348623157e+308)
      // section 15.7.3.3 Number.MIN_VALUE
      .def("MIN_VALUE", 5e-324)
      // section 15.7.3.4 Number.NaN
      .def(symbol::NaN(), JSNaN)
      // section 15.7.3.5 Number.NEGATIVE_INFINITY
      .def("NEGATIVE_INFINITY", -std::numeric_limits<double>::infinity())
      // section 15.7.3.6 Number.POSITIVE_INFINITY
      .def("POSITIVE_INFINITY", std::numeric_limits<double>::infinity())
      // section 15.7.3.7 Number.EPSILON
      .def("EPSILON", DBL_EPSILON)
      // section 15.7.3.8 Number.MAX_INTEGER
      .def("MAX_INTEGER", 9007199254740991.0)
      // section 15.7.3.9 Number.parseInt(string, radix)
      .def<&runtime::GlobalParseInt, 2>("parseInt")
      // section 15.7.3.10 Number.parseFloat(string)
      .def<&runtime::GlobalParseFloat, 1>("parseFloat")
      // section 15.7.3.11 Number.isNaN(number)
      .def<&runtime::NumberIsNaN, 1>("isNaN")
      // section 15.7.3.12 Number.isFinite(number)
      .def<&runtime::NumberIsFinite, 1>("isFinite")
      // section 15.7.3.13 Number.isInteger(number)
      .def<&runtime::NumberIsInteger, 1>("isInteger")
      // section 15.7.3.14 Number.toInteger(number)
      .def<&runtime::NumberToInt, 1>("toInteger");

  bind::Object(this, proto)
      .cls(cls.cls)
      // section 15.7.4.1 Number.prototype.constructor
      .def(symbol::constructor(), constructor, ATTR::W | ATTR::C)
      // section 15.7.4.2 Number.prototype.toString([radix])
      .def<&runtime::NumberToString, 1>(symbol::toString())
      // section 15.7.4.3 Number.prototype.toLocaleString()
      .def<&runtime::NumberToLocaleString, 0>("toLocaleString")
      // section 15.7.4.4 Number.prototype.valueOf()
      .def<&runtime::NumberValueOf, 0>(symbol::valueOf())
      // section 15.7.4.5 Number.prototype.toFixed(fractionDigits)
      .def<&runtime::NumberToFixed, 1>("toFixed")
      // section 15.7.4.6 Number.prototype.toExponential(fractionDigits)
      .def<&runtime::NumberToExponential, 1>("toExponential")
      // section 15.7.4.7 Number.prototype.toPrecision(precision)
      .def<&runtime::NumberToPrecision, 1>("toPrecision")
      // section 15.7.4.8 Number.prototype.clz()
      .def<&runtime::NumberCLZ, 0>("clz");
  global_data()->set_number_prototype(proto);
}

void Context::InitMath(const ClassSlot& func_cls,
                       JSObject* obj_proto, bind::Object* global_binder) {
  // section 15.8 Math
  Error::Dummy dummy;
  struct ClassSlot cls = {
    JSMath::GetClass(),
    Intern("Math"),
    JSString::NewAsciiString(this, "Math", &dummy),
    nullptr,
    obj_proto
  };
  global_data_.RegisterClass<Class::Math>(cls);
  JSObject* const math =
      JSMath::NewPlain(this, Map::NewUniqueMap(this, obj_proto, false));
  global_binder->def("Math", math, ATTR::W | ATTR::C);

  bind::Object(this, math)
      .cls(cls.cls)
      // section 15.8.1.1 E
      .def("E", std::exp(1.0))
      // section 15.8.1.2 LN10
      .def("LN10", std::log(10.0))
      // section 15.8.1.3 LN2
      .def("LN2", std::log(2.0))
      // section 15.8.1.4 LOG2E
      .def("LOG2E", 1.0 / std::log(2.0))
      // section 15.8.1.5 LOG10E
      .def("LOG10E", 1.0 / std::log(10.0))
      // section 15.8.1.6 PI
      .def("PI", std::acos(-1.0))
      // section 15.8.1.7 SQRT1_2
      .def("SQRT1_2", std::sqrt(0.5))
      // section 15.8.1.8 SQRT2
      .def("SQRT2", std::sqrt(2.0))
      // section 15.8.2.1 abs(x)
      .def<&runtime::MathAbs, 1>("abs")
      // section 15.8.2.2 acos(x)
      .def<&runtime::MathAcos, 1>("acos")
      // section 15.8.2.3 asin(x)
      .def<&runtime::MathAsin, 1>("asin")
      // section 15.8.2.4 atan(x)
      .def<&runtime::MathAtan, 1>("atan")
      // section 15.8.2.5 atan2(y, x)
      .def<&runtime::MathAtan2, 2>("atan2")
      // section 15.8.2.6 ceil(x)
      .def<&runtime::MathCeil, 1>("ceil")
      // section 15.8.2.7 cos(x)
      .def<&runtime::MathCos, 1>("cos")
      // section 15.8.2.8 exp(x)
      .def<&runtime::MathExp, 1>("exp")
      // section 15.8.2.9 floor(x)
      .def<&runtime::MathFloor, 1>("floor")
      // section 15.8.2.10 log(x)
      .def<&runtime::MathLog, 1>("log")
      // section 15.8.2.11 max([value1[, value2[, ... ]]])
      .def<&runtime::MathMax, 2>("max")
      // section 15.8.2.12 min([value1[, value2[, ... ]]])
      .def<&runtime::MathMin, 2>("min")
      // section 15.8.2.13 pow(x, y)
      .def<&runtime::MathPow, 2>("pow")
      // section 15.8.2.14 random()
      .def<&runtime::MathRandom, 0>("random")
      // section 15.8.2.15 round(x)
      .def<&runtime::MathRound, 1>("round")
      // section 15.8.2.16 sin(x)
      .def<&runtime::MathSin, 1>("sin")
      // section 15.8.2.17 sqrt(x)
      .def<&runtime::MathSqrt, 1>("sqrt")
      // section 15.8.2.18 tan(x)
      .def<&runtime::MathTan, 1>("tan")
      // section 15.8.2.19 log10(x)
      .def<&runtime::MathLog10, 1>("log10")
      // section 15.8.2.20 log2(x)
      .def<&runtime::MathLog2, 1>("log2")
      // section 15.8.2.21 log1p(x)
      .def<&runtime::MathLog1p, 1>("log1p")
      // section 15.8.2.22 expm1(x)
      .def<&runtime::MathExpm1, 1>("expm1")
      // section 15.8.2.23 cosh(x)
      .def<&runtime::MathCosh, 1>("cosh")
      // section 15.8.2.24 sinh(x)
      .def<&runtime::MathSinh, 1>("sinh")
      // section 15.8.2.25 tanh(x)
      .def<&runtime::MathTanh, 1>("tanh")
      // section 15.8.2.26 acosh(x)
      .def<&runtime::MathAcosh, 1>("acosh")
      // section 15.8.2.27 asinh(x)
      .def<&runtime::MathAsinh, 1>("asinh")
      // section 15.8.2.28 atanh(x)
      .def<&runtime::MathAtanh, 1>("atanh")
      // section 15.8.2.29 hypot(value1, value2[, value3])
      .def<&runtime::MathHypot, 2>("hypot")
      // section 15.8.2.30 trunc(x)
      .def<&runtime::MathTrunc, 1>("trunc")
      // section 15.8.2.31 sign(x)
      .def<&runtime::MathSign, 1>("sign")
      // section 15.8.2.32 cbrt(x)
      .def<&runtime::MathCbrt, 1>("cbrt")
      .def<&runtime::MathImul, 2>("imul");
}

void Context::InitDate(const ClassSlot& func_cls,
                       JSObject* obj_proto, bind::Object* global_binder) {
  // section 15.9 Date
  Error::Dummy dummy;
  JSObject* const proto =
      JSDate::NewPlain(
          this, Map::NewUniqueMap(this, obj_proto, false), core::kNaN);
  global_data()->date_map()->ChangePrototypeWithNoTransition(proto);
  // section 15.9.2.1 The Date Constructor
  JSFunction* const constructor =
      JSInlinedFunction<&runtime::DateConstructor, 7>::New(this, Intern("Date"));

  struct ClassSlot cls = {
    JSDate::GetClass(),
    Intern("Date"),
    JSString::NewAsciiString(this, "Date", &dummy),
    constructor,
    proto
  };
  global_data_.RegisterClass<Class::Date>(cls);
  global_binder->def(cls.name, constructor, ATTR::W | ATTR::C);

  bind::Object(this, constructor)
      // section 15.9.4.1 Date.prototype
      .def(symbol::prototype(), proto, ATTR::NONE)
      // section 15.9.4.2 Date.parse(string)
      .def<&runtime::DateParse, 1>("parse")
      // section 15.9.4.3 Date.UTC()
      .def<&runtime::DateUTC, 7>("UTC")
      // section 15.9.4.4 Date.now()
      .def<&runtime::DateNow, 0>("now");

  JSFunction* const toUTCString =
      JSInlinedFunction<&runtime::DateToUTCString, 0>::New(
          this, Intern("toUTCString"));

  bind::Object(this, proto)
      .cls(cls.cls)
      // section 15.9.5.1 Date.prototype.constructor
      .def(symbol::constructor(), constructor, ATTR::W | ATTR::C)
      // section 15.9.5.2 Date.prototype.toString()
      .def<&runtime::DateToString, 0>("toString")
      // section 15.9.5.3 Date.prototype.toDateString()
      .def<&runtime::DateToDateString, 0>("toDateString")
      // section 15.9.5.4 Date.prototype.toTimeString()
      .def<&runtime::DateToTimeString, 0>("toTimeString")
      // section 15.9.5.5 Date.prototype.toLocaleString()
      .def<&runtime::DateToLocaleString, 0>("toLocaleString")
      // section 15.9.5.6 Date.prototype.toLocaleDateString()
      .def<&runtime::DateToLocaleDateString, 0>("toLocaleDateString")
      // section 15.9.5.7 Date.prototype.toLocaleTimeString()
      .def<&runtime::DateToLocaleTimeString, 0>("toLocaleTimeString")
      // section 15.9.5.8 Date.prototype.valueOf()
      .def<&runtime::DateValueOf, 0>("valueOf")
      // section 15.9.5.9 Date.prototype.getTime()
      .def<&runtime::DateGetTime, 0>("getTime")
      // section 15.9.5.10 Date.prototype.getFullYear()
      .def<&runtime::DateGetFullYear, 0>("getFullYear")
      // section 15.9.5.11 Date.prototype.getUTCFullYear()
      .def<&runtime::DateGetUTCFullYear, 0>("getUTCFullYear")
      // section 15.9.5.12 Date.prototype.getMonth()
      .def<&runtime::DateGetMonth, 0>("getMonth")
      // section 15.9.5.13 Date.prototype.getUTCMonth()
      .def<&runtime::DateGetUTCMonth, 0>("getUTCMonth")
      // section 15.9.5.14 Date.prototype.getDate()
      .def<&runtime::DateGetDate, 0>("getDate")
      // section 15.9.5.15 Date.prototype.getUTCDate()
      .def<&runtime::DateGetUTCDate, 0>("getUTCDate")
      // section 15.9.5.16 Date.prototype.getDay()
      .def<&runtime::DateGetDay, 0>("getDay")
      // section 15.9.5.17 Date.prototype.getUTCDay()
      .def<&runtime::DateGetUTCDay, 0>("getUTCDay")
      // section 15.9.5.18 Date.prototype.getHours()
      .def<&runtime::DateGetHours, 0>("getHours")
      // section 15.9.5.19 Date.prototype.getUTCHours()
      .def<&runtime::DateGetUTCHours, 0>("getUTCHours")
      // section 15.9.5.20 Date.prototype.getMinutes()
      .def<&runtime::DateGetMinutes, 0>("getMinutes")
      // section 15.9.5.21 Date.prototype.getUTCMinutes()
      .def<&runtime::DateGetUTCMinutes, 0>("getUTCMinutes")
      // section 15.9.5.22 Date.prototype.getSeconds()
      .def<&runtime::DateGetSeconds, 0>("getSeconds")
      // section 15.9.5.23 Date.prototype.getUTCSeconds()
      .def<&runtime::DateGetUTCSeconds, 0>("getUTCSeconds")
      // section 15.9.5.24 Date.prototype.getMilliseconds()
      .def<&runtime::DateGetMilliseconds, 0>("getMilliseconds")
      // section 15.9.5.25 Date.prototype.getUTCMilliseconds()
      .def<&runtime::DateGetUTCMilliseconds, 0>("getUTCMilliseconds")
      // section 15.9.5.26 Date.prototype.getTimezoneOffset()
      .def<&runtime::DateGetTimezoneOffset, 0>("getTimezoneOffset")
      // section 15.9.5.27 Date.prototype.setTime(time)
      .def<&runtime::DateSetTime, 1>("setTime")
      // section 15.9.5.28 Date.prototype.setMilliseconds(ms)
      .def<&runtime::DateSetMilliseconds, 1>("setMilliseconds")
      // section 15.9.5.29 Date.prototype.setUTCMilliseconds(ms)
      .def<&runtime::DateSetUTCMilliseconds, 1>("setUTCMilliseconds")
      // section 15.9.5.30 Date.prototype.setSeconds(sec[, ms])
      .def<&runtime::DateSetSeconds, 2>("setSeconds")
      // section 15.9.5.31 Date.prototype.setUTCSeconds(sec[, ms])
      .def<&runtime::DateSetUTCSeconds, 2>("setUTCSeconds")
      // section 15.9.5.32 Date.prototype.setMinutes(min[, sec[, ms]])
      .def<&runtime::DateSetMinutes, 3>("setMinutes")
      // section 15.9.5.33 Date.prototype.setUTCMinutes(min[, sec[, ms]])
      .def<&runtime::DateSetUTCMinutes, 3>("setUTCMinutes")
      // section 15.9.5.34 Date.prototype.setHours(hour[, min[, sec[, ms]])
      .def<&runtime::DateSetHours, 4>("setHours")
      // section 15.9.5.35 Date.prototype.setUTCHours(hour[, min[, sec[, ms]])
      .def<&runtime::DateSetUTCHours, 4>("setUTCHours")
      // section 15.9.5.36 Date.prototype.setDate(date)
      .def<&runtime::DateSetDate, 1>("setDate")
      // section 15.9.5.37 Date.prototype.setUTCDate(date)
      .def<&runtime::DateSetUTCDate, 1>("setUTCDate")
      // section 15.9.5.38 Date.prototype.setMonth(month[, date])
      .def<&runtime::DateSetMonth, 2>("setMonth")
      // section 15.9.5.39 Date.prototype.setUTCMonth(month[, date])
      .def<&runtime::DateSetUTCMonth, 2>("setUTCMonth")
      // section 15.9.5.40 Date.prototype.setFullYear(year[, month[, date]])
      .def<&runtime::DateSetFullYear, 3>("setFullYear")
      // section 15.9.5.41 Date.prototype.setUTCFullYear(year[, month[, date]])
      .def<&runtime::DateSetUTCFullYear, 3>("setUTCFullYear")
      // section 15.9.5.42 Date.prototype.toUTCString()
      .def("toUTCString", toUTCString, ATTR::W | ATTR::C)
      // section 15.9.5.43 Date.prototype.toISOString()
      .def<&runtime::DateToISOString, 0>("toISOString")
      // section 15.9.5.44 Date.prototype.toJSON()
      .def<&runtime::DateToJSON, 1>("toJSON")
      // section B.2.4 Date.prototype.getYear()
      // this method is deprecated.
      .def<&runtime::DateGetYear, 0>("getYear")
      // section B.2.5 Date.prototype.setYear(year)
      // this method is deprecated.
      .def<&runtime::DateSetYear, 1>("setYear")
      // section B.2.6 Date.prototype.toGMTString()
      .def("toGMTString", toUTCString, ATTR::W | ATTR::C);
  global_data()->set_date_prototype(proto);
}

void Context::InitRegExp(const ClassSlot& func_cls,
                         JSObject* obj_proto, bind::Object* global_binder) {
  // section 15.10 RegExp
  Error::Dummy dummy;
  Map* proto_map =
      global_data()->regexp_map()->ChangePrototypeTransition(this, obj_proto);
  JSObject* const proto = JSRegExp::NewPlain(this, proto_map);
  global_data()->regexp_map()->ChangePrototypeWithNoTransition(proto);
  // section 15.10.4 The RegExp Constructor
  JSFunction* const constructor =
      JSInlinedFunction<&runtime::RegExpConstructor, 2>::New(
          this, Intern("RegExp"));

  struct ClassSlot cls = {
    JSRegExp::GetClass(),
    Intern("RegExp"),
    JSString::NewAsciiString(this, "RegExp", &dummy),
    constructor,
    proto
  };
  global_data_.RegisterClass<Class::RegExp>(cls);
  global_binder->def(cls.name, constructor, ATTR::W | ATTR::C);

  bind::Object(this, constructor)
      // section 15.10.5.1 RegExp.prototype
      .def(symbol::prototype(), proto, ATTR::NONE);

  bind::Object(this, proto)
      .cls(cls.cls)
      // section 15.10.6.1 RegExp.prototype.constructor
      .def(symbol::constructor(), constructor, ATTR::W | ATTR::C)
      // section 15.10.6.2 RegExp.prototype.exec(string)
      .def<&runtime::RegExpExec, 1>("exec")
      // section 15.10.6.3 RegExp.prototype.test(string)
      .def<&runtime::RegExpTest, 1>("test")
      // section 15.10.6.4 RegExp.prototype.toString()
      .def<&runtime::RegExpToString, 0>("toString")
      // Not Standard RegExp.prototype.compile(pattern, flags)
      .def<&runtime::RegExpCompile, 2>("compile");
  global_data()->set_regexp_prototype(proto);
}

void Context::InitError(const ClassSlot& func_cls,
                        JSObject* obj_proto, bind::Object* global_binder) {
  // Error
  Error::Dummy dummy;
  JSObject* const proto =
      JSObject::NewPlain(this, Map::NewUniqueMap(this, obj_proto, false));
  global_data()->error_map()->ChangePrototypeWithNoTransition(proto);
  // section 15.11.2 The Error Constructor
  JSFunction* const constructor =
      JSInlinedFunction<&runtime::ErrorConstructor, 1>::New(
          this, Intern("Error"));

  struct ClassSlot cls = {
    JSError::GetClass(),
    Intern("Error"),
    JSString::NewAsciiString(this, "Error", &dummy),
    constructor,
    proto
  };
  global_data_.RegisterClass<Class::Error>(cls);
  global_binder->def(cls.name, constructor, ATTR::W | ATTR::C);

  bind::Object(this, constructor)
      // section 15.11.3.1 Error.prototype
      .def(symbol::prototype(), proto, ATTR::NONE);

  bind::Object(this, proto)
      .cls(cls.cls)
      // section 15.11.4.1 Error.prototype.constructor
      .def(symbol::constructor(), constructor, ATTR::W | ATTR::C)
      // section 15.11.4.2 Error.prototype.name
      .def("name",
           JSString::NewAsciiString(this, "Error", &dummy), ATTR::W | ATTR::C)
      // section 15.11.4.3 Error.prototype.message
      .def("message", JSString::NewEmptyString(this), ATTR::W | ATTR::C)
      // section 15.11.4.4 Error.prototype.toString()
      .def<&runtime::ErrorToString, 0>(symbol::toString());
  global_data()->set_error_prototype(proto);

  {
    // section 15.11.6.1 EvalError
    JSObject* const sub_proto =
        JSObject::NewPlain(this, Map::NewUniqueMap(this, proto, false));
    global_data()->eval_error_map()->ChangePrototypeWithNoTransition(sub_proto);
    const Symbol sym = Intern("EvalError");
    JSFunction* const sub_constructor =
        JSInlinedFunction<&runtime::EvalErrorConstructor, 1>::New(this, sym);

    bind::Object(this, sub_constructor)
        .def(symbol::prototype(), sub_proto, ATTR::NONE);

    struct ClassSlot sub_cls = {
      JSEvalError::GetClass(),
      Intern("Error"),
      JSString::NewAsciiString(this, "EvalError", &dummy),
      sub_constructor,
      sub_proto
    };
    global_data_.RegisterClass<Class::EvalError>(sub_cls);
    global_binder->def(sym, sub_constructor, ATTR::W | ATTR::C);

    bind::Object(this, sub_proto)
        .cls(sub_cls.cls)
        .def(symbol::constructor(),
             sub_constructor, ATTR::W | ATTR::C)
        .def("name", sub_cls.name_string, ATTR::W | ATTR::C)
        .def("message", JSString::NewEmptyString(this), ATTR::W | ATTR::C);
    global_data()->set_eval_error_prototype(sub_proto);
  }
  {
    // section 15.11.6.2 RangeError
    JSObject* const sub_proto =
        JSObject::NewPlain(this, Map::NewUniqueMap(this, proto, false));
    global_data()->range_error_map()->ChangePrototypeWithNoTransition(sub_proto);
    const Symbol sym = Intern("RangeError");
    JSFunction* const sub_constructor =
        JSInlinedFunction<&runtime::RangeErrorConstructor, 1>::New(this, sym);

    bind::Object(this, sub_constructor)
        .def(symbol::prototype(), sub_proto, ATTR::NONE);

    struct ClassSlot sub_cls = {
      JSRangeError::GetClass(),
      Intern("Error"),
      JSString::NewAsciiString(this, "RangeError", &dummy),
      sub_constructor,
      sub_proto
    };
    global_data_.RegisterClass<Class::RangeError>(sub_cls);
    global_binder->def(sym, sub_constructor, ATTR::W | ATTR::C);

    bind::Object(this, sub_proto)
        .cls(sub_cls.cls)
        .def(symbol::constructor(),
             sub_constructor, ATTR::W | ATTR::C)
        .def("name", sub_cls.name_string, ATTR::W | ATTR::C)
        .def("message", JSString::NewEmptyString(this), ATTR::W | ATTR::C);
    global_data()->set_range_error_prototype(sub_proto);
  }
  {
    // section 15.11.6.3 ReferenceError
    JSObject* const sub_proto =
        JSObject::NewPlain(this, Map::NewUniqueMap(this, proto, false));
    global_data()->reference_error_map()->ChangePrototypeWithNoTransition(
        sub_proto);
    const Symbol sym = Intern("ReferenceError");
    JSFunction* const sub_constructor =
        JSInlinedFunction<
          &runtime::ReferenceErrorConstructor, 1>::New(this, sym);

    bind::Object(this, sub_constructor)
        .def(symbol::prototype(), sub_proto, ATTR::NONE);

    struct ClassSlot sub_cls = {
      JSReferenceError::GetClass(),
      Intern("Error"),
      JSString::NewAsciiString(this, "ReferenceError", &dummy),
      sub_constructor,
      sub_proto
    };
    global_data_.RegisterClass<Class::ReferenceError>(sub_cls);
    global_binder->def(sym, sub_constructor, ATTR::W | ATTR::C);

    bind::Object(this, sub_proto)
        .cls(sub_cls.cls)
        .def(symbol::constructor(),
             sub_constructor, ATTR::W | ATTR::C)
        .def("name", sub_cls.name_string, ATTR::W | ATTR::C)
        .def("message", JSString::NewEmptyString(this), ATTR::W | ATTR::C);
    global_data()->set_reference_error_prototype(sub_proto);
  }
  {
    // section 15.11.6.4 SyntaxError
    JSObject* const sub_proto =
        JSObject::NewPlain(this, Map::NewUniqueMap(this, proto, false));
    global_data()->syntax_error_map()->ChangePrototypeWithNoTransition(
        sub_proto);
    const Symbol sym = Intern("SyntaxError");
    JSFunction* const sub_constructor =
        JSInlinedFunction<&runtime::SyntaxErrorConstructor, 1>::New(this, sym);

    bind::Object(this, sub_constructor)
        .def(symbol::prototype(), sub_proto, ATTR::NONE);

    struct ClassSlot sub_cls = {
      JSSyntaxError::GetClass(),
      Intern("Error"),
      JSString::NewAsciiString(this, "SyntaxError", &dummy),
      sub_constructor,
      sub_proto
    };
    global_data_.RegisterClass<Class::SyntaxError>(sub_cls);
    global_binder->def(sym, sub_constructor, ATTR::W | ATTR::C);

    bind::Object(this, sub_proto)
        .cls(sub_cls.cls)
        .def(symbol::constructor(),
             sub_constructor, ATTR::W | ATTR::C)
        .def("name", sub_cls.name_string, ATTR::W | ATTR::C)
        .def("message", JSString::NewEmptyString(this), ATTR::W | ATTR::C);
    global_data()->set_syntax_error_prototype(sub_proto);
  }
  {
    // section 15.11.6.5 TypeError
    JSObject* const sub_proto =
        JSObject::NewPlain(this, Map::NewUniqueMap(this, proto, false));
    global_data()->type_error_map()->ChangePrototypeWithNoTransition(sub_proto);
    const Symbol sym = Intern("TypeError");
    JSFunction* const sub_constructor =
        JSInlinedFunction<&runtime::TypeErrorConstructor, 1>::New(this, sym);

    bind::Object(this, sub_constructor)
        .def(symbol::prototype(), sub_proto, ATTR::NONE);

    struct ClassSlot sub_cls = {
      JSTypeError::GetClass(),
      Intern("Error"),
      JSString::NewAsciiString(this, "TypeError", &dummy),
      sub_constructor,
      sub_proto
    };
    global_data_.RegisterClass<Class::TypeError>(sub_cls);
    global_binder->def(sym, sub_constructor, ATTR::W | ATTR::C);

    bind::Object(this, sub_proto)
        .cls(sub_cls.cls)
        .def(symbol::constructor(),
             sub_constructor, ATTR::W | ATTR::C)
        .def("name", sub_cls.name_string, ATTR::W | ATTR::C)
        .def("message", JSString::NewEmptyString(this), ATTR::W | ATTR::C);
    global_data()->set_type_error_prototype(sub_proto);
  }
  {
    // section 15.11.6.6 URIError
    JSObject* const sub_proto =
        JSObject::NewPlain(this, Map::NewUniqueMap(this, proto, false));
    global_data()->uri_error_map()->ChangePrototypeWithNoTransition(sub_proto);
    const Symbol sym = Intern("URIError");
    JSFunction* const sub_constructor =
        JSInlinedFunction<&runtime::URIErrorConstructor, 1>::New(this, sym);

    bind::Object(this, sub_constructor)
        .def(symbol::prototype(), sub_proto, ATTR::NONE);

    struct ClassSlot sub_cls = {
      JSURIError::GetClass(),
      Intern("Error"),
      JSString::NewAsciiString(this, "URIError", &dummy),
      sub_constructor,
      sub_proto
    };
    global_data_.RegisterClass<Class::URIError>(sub_cls);
    global_binder->def(sym, sub_constructor, ATTR::W | ATTR::C);

    bind::Object(this, sub_proto)
        .cls(sub_cls.cls)
        .def(symbol::constructor(),
             sub_constructor, ATTR::W | ATTR::C)
        .def("name", sub_cls.name_string, ATTR::W | ATTR::C)
        .def("message", JSString::NewEmptyString(this), ATTR::W | ATTR::C);
    global_data()->set_uri_error_prototype(sub_proto);
  }
}

void Context::InitJSON(const ClassSlot& func_cls,
                       JSObject* obj_proto, bind::Object* global_binder) {
  // section 15.12 JSON
  Error::Dummy dummy;
  struct ClassSlot cls = {
    JSJSON::GetClass(),
    Intern("JSON"),
    JSString::NewAsciiString(this, "JSON", &dummy),
    nullptr,
    obj_proto
  };
  global_data_.RegisterClass<Class::JSON>(cls);
  JSObject* const json =
      JSJSON::NewPlain(this, Map::NewUniqueMap(this, obj_proto, false));
  global_binder->def("JSON", json, ATTR::W | ATTR::C);
  bind::Object(this, json)
      .cls(cls.cls)
      // section 15.12.2 parse(text[, reviver])
      .def<&runtime::JSONParse, 2>("parse")
      // section 15.12.3 stringify(value[, replacer[, space]])
      .def<&runtime::JSONStringify, 3>("stringify");
}

void Context::InitMap(const ClassSlot& func_cls,
                      JSObject* obj_proto, bind::Object* global_binder) {
  // ES.next Map
  // http://wiki.ecmascript.org/doku.php?id=harmony:simple_maps_and_sets
  Error::Dummy dummy;
  JSObject* const proto =
      JSObject::New(this, Map::NewUniqueMap(this, obj_proto, false));
  global_data()->map_map()->ChangePrototypeWithNoTransition(proto);
  JSFunction* const constructor =
      JSInlinedFunction<&runtime::MapConstructor, 0>::New(this, Intern("Map"));

  global_binder->def("Map", constructor, ATTR::W | ATTR::C);

  bind::Object(this, constructor)
      .def(symbol::prototype(), proto, ATTR::NONE);

  JSFunction* map_entries =
     JSInlinedFunction<&runtime::MapEntries, 0>::New(this, Intern("entries"));

  bind::Object(this, proto)
      .def(symbol::constructor(), constructor, ATTR::W | ATTR::C)
      .def(global_data()->builtin_symbol_toStringTag(),
           JSString::NewAsciiString(this, "Map", &dummy), ATTR::C)
      .def<&runtime::MapClear, 0>("clear")
      .def<&runtime::MapDelete, 1>("delete")
      .def<&runtime::MapForEach, 1>("forEach")
      .def<&runtime::MapGet, 1>("get")
      .def<&runtime::MapHas, 1>("has")
      .def<&runtime::MapKeys, 0>("keys")
      .def<&runtime::MapSet, 2>("set")
      .def_getter<&runtime::MapSize, 0>("size")
      .def<&runtime::MapValues, 0>("values")
      .def("entries", map_entries, ATTR::W | ATTR::C)
      .def(global_data()->builtin_symbol_iterator(),
           map_entries, ATTR::W | ATTR::C);
  global_data()->set_map_prototype(proto);

  // Init MapIterator
  JSObject* const map_iterator_proto = JSObject::New(this);
  global_data()->map_iterator_map()->ChangePrototypeWithNoTransition(
      map_iterator_proto);
  bind::Object(this, map_iterator_proto)
      // ES6
      // section 23.1.5.2.1 %MapIteratorPrototype%.next()
      .def<&runtime::MapIteratorNext, 0>("next")
      // ES6
      // section 23.1.5.2.2 %MapIteratorPrototype%[@@iterator]()
      .def(global_data()->builtin_symbol_toPrimitive(),
           JSInlinedFunction<
             &runtime::MapIteratorIterator, 0>::New(
                 this, Intern("[Symbol.iterator]")),
             ATTR::W | ATTR::C)
      // ES6
      // section 23.1.5.2.3 %MapIteratorPrototype%[@@toStringTag]
      .def(global_data()->builtin_symbol_toStringTag(),
           JSString::NewAsciiString(this, "Map Iterator", &dummy),
           ATTR::W | ATTR::C);
  global_data()->set_map_iterator_prototype(map_iterator_proto);
}

void Context::InitWeakMap(const ClassSlot& func_cls,
                          JSObject* obj_proto, bind::Object* global_binder) {
  Error::Dummy dummy;
  JSObject* const proto =
      JSObject::New(this, Map::NewUniqueMap(this, obj_proto, false));
  global_data()->weak_map_map()->ChangePrototypeWithNoTransition(proto);
  JSFunction* const constructor =
      JSInlinedFunction<&runtime::WeakMapConstructor, 0>::New(
          this, Intern("WeakMap"));

  global_binder->def("WeakMap", constructor, ATTR::W | ATTR::C);

  bind::Object(this, constructor)
      .def(symbol::prototype(), proto, ATTR::NONE);

  bind::Object(this, proto)
      .def(symbol::constructor(), constructor, ATTR::W | ATTR::C)
      .def(global_data()->builtin_symbol_toStringTag(),
           JSString::NewAsciiString(this, "WeakMap", &dummy), ATTR::C)
      .def<&runtime::WeakMapClear, 0>("clear")
      .def<&runtime::WeakMapDelete, 1>("delete")
      .def<&runtime::WeakMapGet, 1>("get")
      .def<&runtime::WeakMapHas, 1>("has")
      .def<&runtime::WeakMapSet, 2>("set");
  global_data()->set_weak_map_prototype(proto);
}

void Context::InitSet(const ClassSlot& func_cls,
                      JSObject* obj_proto, bind::Object* global_binder) {
  // ES.next Set
  // http://wiki.ecmascript.org/doku.php?id=harmony:simple_maps_and_sets
  Error::Dummy dummy;
  JSObject* const proto =
      JSObject::New(this, Map::NewUniqueMap(this, obj_proto, false));
  global_data()->set_map()->ChangePrototypeWithNoTransition(proto);
  JSFunction* const constructor =
      JSInlinedFunction<&runtime::SetConstructor, 0>::New(this, Intern("Set"));

  global_binder->def("Set", constructor, ATTR::W | ATTR::C);

  bind::Object(this, constructor)
      .def(symbol::prototype(), proto, ATTR::NONE);

  JSFunction* set_values =
     JSInlinedFunction<&runtime::SetValues, 0>::New(this, Intern("values"));

  bind::Object(this, proto)
      .def(symbol::constructor(), constructor, ATTR::W | ATTR::C)
      .def(global_data()->builtin_symbol_toStringTag(),
           JSString::NewAsciiString(this, "Set", &dummy), ATTR::C)
      .def<&runtime::SetAdd, 1>("add")
      .def<&runtime::SetClear, 0>("clear")
      .def<&runtime::SetDelete, 1>("delete")
      .def<&runtime::SetEntries, 0>("entries")
      .def<&runtime::SetForEach, 1>("forEach")
      .def<&runtime::SetHas, 1>("has")
      .def<&runtime::SetKeys, 0>("keys")
      .def_getter<&runtime::SetSize, 0>("size")
      .def("values", set_values, ATTR::W | ATTR::C)
      .def(global_data()->builtin_symbol_iterator(),
           set_values, ATTR::W | ATTR::C);
  global_data()->set_set_prototype(proto);

  // Init SetIterator
  JSObject* const set_iterator_proto = JSObject::New(this);
  global_data()->set_iterator_map()->ChangePrototypeWithNoTransition(
      set_iterator_proto);
  bind::Object(this, set_iterator_proto)
      // ES6
      // section 23.2.5.2.1 %SetIteratorPrototype%.next()
      .def<&runtime::SetIteratorNext, 0>("next")
      // ES6
      // section 23.2.5.2.2 %SetIteratorPrototype%[@@iterator]()
      .def(global_data()->builtin_symbol_toPrimitive(),
           JSInlinedFunction<
             &runtime::SetIteratorIterator, 0>::New(
                 this, Intern("[Symbol.iterator]")),
             ATTR::W | ATTR::C)
      // ES6
      // section 23.2.5.2.3 %SetIteratorPrototype%[@@toStringTag]
      .def(global_data()->builtin_symbol_toStringTag(),
           JSString::NewAsciiString(this, "Set Iterator", &dummy),
           ATTR::W | ATTR::C);
  global_data()->set_set_iterator_prototype(set_iterator_proto);
}

void Context::InitIntl(const ClassSlot& func_cls,
                       JSObject* obj_proto, bind::Object* global_binder) {
  Error::Dummy dummy;
  intl_ = i18n::JSIntl::New(this);
  global_binder->def("Intl", intl_, ATTR::W | ATTR::C);

  bind::Object intl_binder = bind::Object(this, intl_);

#ifdef IV_ENABLE_I18N
  // Currently, Collator, NumberFormat, DateTimeFormat need ICU
  {
    // Collator
    JSObject* const proto =
        JSCollator::NewPlain(this, Map::NewUniqueMap(this, obj_proto));
    JSFunction* const constructor =
        JSInlinedFunction<&runtime::CollatorConstructor, 0>::New(
            this, Intern("Collator"));

    struct ClassSlot cls = {
      JSCollator::GetClass(),
      Intern("Collator"),
      JSString::NewAsciiString(this, "Collator", &dummy),
      constructor,
      proto
    };
    global_data_.RegisterClass<Class::Collator>(cls);
    intl_binder.def(cls.name, constructor, ATTR::W | ATTR::C);

    bind::Object(this, constructor)
        .def(symbol::prototype(), proto, ATTR::NONE)
        .def<&runtime::CollatorSupportedLocalesOf, 1>("supportedLocalesOf");

    bind::Object(this, proto)
        .cls(cls.cls)
        .def(symbol::constructor(), constructor, ATTR::W | ATTR::C)
        .def_getter<&runtime::CollatorCompareGetter, 0>(symbol::compare())
        .def<&runtime::CollatorResolvedOptions, 0>("resolvedOptions");
  }
#endif  // IV_ENABLE_I18N

  {
    // NumberFormat
    JSObject* const proto =
        JSObject::New(this, Map::NewUniqueMap(this, obj_proto, false));
    global_data()->number_format_map()->ChangePrototypeWithNoTransition(proto);
    JSFunction* const constructor =
        JSInlinedFunction<&runtime::NumberFormatConstructor, 0>::New(
            this, symbol::NumberFormat());

    intl_binder.def(symbol::NumberFormat(), constructor, ATTR::W | ATTR::C);

    bind::Object(this, constructor)
        .def(symbol::prototype(), proto, ATTR::NONE)
        .def<&runtime::NumberFormatSupportedLocalesOf, 1>("supportedLocalesOf");

    bind::Object(this, proto)
        .def(symbol::constructor(), constructor, ATTR::W | ATTR::C)
        .def_getter<&runtime::NumberFormatFormatGetter, 0>("format")
        .def<&runtime::NumberFormatResolvedOptions, 0>("resolvedOptions");
    Error::Dummy dummy;
    i18n::InitializeNumberFormat(
        this, proto, JSUndefined, JSUndefined, &dummy);
    global_data()->set_number_format_prototype(proto);
  }

#ifdef IV_ENABLE_I18N
  {
    // DateTimeFormat
    JSObject* const proto =
        JSDateTimeFormat::NewPlain(
            this, Map::NewUniqueMap(this, obj_proto, false));
    global_data()->date_time_format_map()->ChangePrototypeWithNoTransition(
        proto);
    JSFunction* const constructor =
        JSInlinedFunction<&runtime::DateTimeFormatConstructor, 0>::New(
            this, Intern("DateTimeFormat"));

    struct ClassSlot cls = {
      JSDateTimeFormat::GetClass(),
      Intern("DateTimeFormat"),
      JSString::NewAsciiString(this, "DateTimeFormat", &dummy),
      constructor,
      proto
    };
    global_data_.RegisterClass<Class::DateTimeFormat>(cls);
    intl_binder.def(cls.name, constructor, ATTR::W | ATTR::C);

    bind::Object(this, constructor)
        .def(symbol::prototype(), proto, ATTR::NONE)
        .def<&runtime::CollatorSupportedLocalesOf, 1>("supportedLocalesOf");

    bind::Object(this, proto)
        .cls(cls.cls)
        .def(symbol::constructor(), constructor, ATTR::W | ATTR::C)
        .def<&runtime::DateTimeFormatFormat, 1>("format")
        .def<&runtime::DateTimeFormatResolvedOptions, 0>("resolvedOptions");
  }
#endif  // IV_ENABLE_I18N
}

void Context::InitBinaryBlocks(const ClassSlot& func_cls,
                               JSObject* obj_proto,
                               bind::Object* global_binder) {
  Error::Dummy dummy;
  // ArrayBuffer
  {
    Map* proto_map =
        global_data()->array_buffer_map()->ChangePrototypeTransition(
            this, obj_proto);
    JSObject* const proto =
        JSArrayBuffer::NewPlain(this, 0, proto_map, &dummy);
    global_data()->array_buffer_map()->ChangePrototypeWithNoTransition(proto);
    JSFunction* const constructor =
        JSInlinedFunction<&runtime::ArrayBufferConstructor, 1>::New(
            this, Intern("ArrayBuffer"));

    struct ClassSlot cls = {
      JSArrayBuffer::GetClass(),
      Intern("ArrayBuffer"),
      JSString::NewAsciiString(this, "ArrayBuffer", &dummy),
      constructor,
      proto
    };
    global_data_.RegisterClass<Class::ArrayBuffer>(cls);
    global_binder->def(cls.name, constructor, ATTR::W | ATTR::C);
    global_data_.set_array_buffer_prototype(proto);

    bind::Object(this, constructor)
        .def(symbol::prototype(), proto, ATTR::NONE);

    bind::Object(this, proto)
        .cls(cls.cls)
        .def(symbol::constructor(), constructor, ATTR::W | ATTR::C);
  }

  // TypedArray
  InitTypedArray<JSInt8Array, Class::Int8Array>(func_cls, global_binder);
  InitTypedArray<JSUint8Array, Class::Uint8Array>(func_cls, global_binder);
  InitTypedArray<JSInt16Array, Class::Int16Array>(func_cls, global_binder);
  InitTypedArray<JSUint16Array, Class::Uint16Array>(func_cls, global_binder);
  InitTypedArray<JSInt32Array, Class::Int32Array>(func_cls, global_binder);
  InitTypedArray<JSUint32Array, Class::Uint32Array>(func_cls, global_binder);
  InitTypedArray<JSFloat32Array, Class::Float32Array>(func_cls, global_binder);
  InitTypedArray<JSFloat64Array, Class::Float64Array>(func_cls, global_binder);
  InitTypedArray<JSUint8ClampedArray, Class::Uint8ClampedArray>(func_cls,
                                                                global_binder);

  // DataView
  {
    Map* proto_map =
        global_data()->data_view_map()->ChangePrototypeTransition(this,
                                                                  obj_proto);
    JSObject* const proto = JSObject::New(this, proto_map);
    global_data()->data_view_map()->ChangePrototypeWithNoTransition(proto);
    JSFunction* const constructor =
        JSInlinedFunction<&runtime::DataViewConstructor, 1>::New(
            this, Intern("DataView"));

    struct ClassSlot cls = {
      JSDataView::GetClass(),
      Intern("DataView"),
      JSString::NewAsciiString(this, "DataView", &dummy),
      constructor,
      proto
    };
    global_data_.RegisterClass<Class::DataView>(cls);
    global_binder->def(cls.name, constructor, ATTR::W | ATTR::C);
    global_data_.set_data_view_prototype(proto);

    bind::Object(this, constructor)
        .def(symbol::prototype(), proto, ATTR::NONE);

    bind::Object(this, proto)
        .def(symbol::constructor(), constructor, ATTR::W | ATTR::C)
        .def<&runtime::DataViewGetInt8, 1>("getInt8")
        .def<&runtime::DataViewGetUint8, 1>("getUint8")
        .def<&runtime::DataViewGetInt16, 2>("getInt16")
        .def<&runtime::DataViewGetUint16, 2>("getUint16")
        .def<&runtime::DataViewGetInt32, 2>("getInt32")
        .def<&runtime::DataViewGetUint32, 2>("getUint32")
        .def<&runtime::DataViewGetFloat32, 2>("getFloat32")
        .def<&runtime::DataViewGetFloat64, 2>("getFloat64")
        .def<&runtime::DataViewSetInt8, 2>("setInt8")
        .def<&runtime::DataViewSetUint8, 2>("setUint8")
        .def<&runtime::DataViewSetInt16, 3>("setInt16")
        .def<&runtime::DataViewSetUint16, 3>("setUint16")
        .def<&runtime::DataViewSetInt32, 3>("setInt32")
        .def<&runtime::DataViewSetUint32, 3>("setUint32")
        .def<&runtime::DataViewSetFloat32, 3>("setFloat32")
        .def<&runtime::DataViewSetFloat64, 3>("setFloat64");
  }
}

template<typename TypedArray, Class::JSClassType CLS>
inline void Context::InitTypedArray(const ClassSlot& func_cls,
                                    bind::Object* global_binder) {
  Error::Dummy dummy;
  Map* proto_map =
      global_data()->typed_array_map(
          TypedArrayTraits<typename TypedArray::Element>::code)->
            ChangePrototypeTransition(this, global_data()->object_prototype());
  JSObject* const proto = JSObject::New(this, proto_map);
  global_data()->typed_array_map(
      TypedArrayTraits<typename TypedArray::Element>::code)->
        ChangePrototypeWithNoTransition(proto);
  const Class* cls = TypedArray::GetClass();
  JSFunction* const constructor =
      JSInlinedFunction<
        &runtime::TypedArrayConstructor<
          typename TypedArray::Element, TypedArray>, 1>::New(
            this,
            Intern(cls->name));
  struct ClassSlot slot = {
    TypedArray::GetClass(),
    Intern(cls->name),
    JSString::NewAsciiString(this, cls->name, &dummy),
    constructor,
    proto
  };
  global_data()->RegisterClass<CLS>(slot);
  global_binder->def(slot.name, constructor, ATTR::W | ATTR::C);
  global_data()->set_typed_array_prototype(
      TypedArrayTraits<typename TypedArray::Element>::code, proto);
  constructor->ChangePrototype(this, func_cls.prototype);
  bind::Object(this, constructor)
      .def(symbol::prototype(), proto, ATTR::NONE)
      .def("BYTES_PER_ELEMENT",
           JSVal::UnSigned(
               static_cast<uint32_t>(sizeof(typename TypedArray::Element))),
           ATTR::NONE);
  bind::Object proto_binder(this, proto);
  proto_binder.def(symbol::constructor(), constructor, ATTR::W | ATTR::C);
  proto_binder.def<
      &runtime::TypedArraySet<
        typename TypedArray::Element, TypedArray>, 1>("set");
  proto_binder.def<
      &runtime::TypedArraySubarray<
        typename TypedArray::Element, TypedArray>, 1>("subarray");
}

void Context::InitReflect(const ClassSlot& func_cls,
                          JSObject* obj_proto, bind::Object* global_binder) {
  // section 15.17 Reflect
  Error::Dummy dummy;
  struct ClassSlot cls = {
    JSReflect::GetClass(),
    Intern("Reflect"),
    JSString::NewAsciiString(this, "Reflect", &dummy),
    nullptr,
    obj_proto
  };
  global_data_.RegisterClass<Class::Reflect>(cls);
  JSObject* const ref =
      JSReflect::New(this, Map::NewUniqueMap(this, obj_proto, false));
  global_binder->def("Reflect", ref, ATTR::W | ATTR::C);

  bind::Object(this, ref)
      .cls(cls.cls)
      // 15.17.1.1 Reflect.getPrototypeOf(target)
      .def<&runtime::ReflectGetPrototypeOf, 1>("getPrototypeOf")
      // 15.17.1.2 Reflect.setPrototypeOf(target, proto)
      .def<&runtime::ReflectSetPrototypeOf, 2>("setPrototypeOf")
      // 15.17.1.3 Reflect.isExtensible(target)
      .def<&runtime::ReflectIsExtensible, 1>("isExtensible")
      // 15.17.1.4 Reflect.preventExtensions(target)
      .def<&runtime::ReflectPreventExtensions, 1>("preventExtensions")
      // 15.17.1.5 Reflect.hasOwn(target, propertyKey)
      .def<&runtime::ReflectHasOwn, 2>("hasOwn")
      // 15.17.1.6 Reflect.getOwnPropertyDescriptor(target, propertyKey)
      .def<&runtime::ReflectGetOwnPropertyDescriptor, 2>("getOwnPropertyDescriptor")
      // 15.17.1.7 Reflect.get(target, propertyKey, receiver=target)
      // .def<&runtime::ReflectGet, 3>("get")
      // 15.17.1.8 Reflect.set(target, propertyKey, V, receiver=target)
      // .def<&runtime::ReflectSet, 4>("set")
      // 15.17.1.9 Reflect.deleteProperty(target, propertyKey)
      .def<&runtime::ReflectDeleteProperty, 2>("deleteProperty")
      // 15.17.1.10 Reflect.defineProperty(target, propertyKey, attributes)
      .def<&runtime::ReflectDefineProperty, 3>("defineProperty")
      // 15.17.1.11 Reflect.enumerate(target)
      // .def<&runtime::ReflectEnumerate, 1>("enumerate")
      // 15.17.1.12 Reflect.keys(target)
      .def<&runtime::ReflectKeys, 1>("keys")
      // 15.17.1.13 Reflect.getOwnPropertyNames(target)
      .def<&runtime::ReflectGetOwnPropertyNames, 1>("getOwnPropertyNames")
      // 15.17.1.14 Reflect.freeze(target)
      .def<&runtime::ReflectFreeze, 1>("freeze")
      // 15.17.1.15 Reflect.seal(target)
      .def<&runtime::ReflectSeal, 1>("seal")
      // 15.17.1.16 Reflect.isFrozen(target)
      .def<&runtime::ReflectIsFrozen, 1>("isFrozen")
      // 15.17.1.17 Reflect.isSealed(target)
      .def<&runtime::ReflectIsSealed, 1>("isSealed")
      // 15.17.2.1 Reflect.has(target, propertyKey)
      .def<&runtime::ReflectHas, 2>("has")
      // 15.17.2.2 Reflect.instanceOf(target, O)
      .def<&runtime::ReflectInstanceOf, 2>("instanceOf");
}

void Context::InitSymbol(const ClassSlot& func_cls,
                         JSObject* obj_proto, bind::Object* global_binder) {
  Error::Dummy dummy;
  JSObject* const proto = JSObject::New(this);
  global_data()->symbol_map()->ChangePrototypeWithNoTransition(proto);
  global_data()->primitive_symbol_map()->ChangePrototypeWithNoTransition(proto);
  JSFunction* const constructor =
      JSInlinedFunction<&runtime::SymbolConstructor, 1>::New(
          this, Intern("Symbol"));

  struct ClassSlot cls = {
    JSSymbolObject::GetClass(),
    Intern("Symbol"),
    JSString::NewAsciiString(this, "Symbol", &dummy),
    constructor,
    proto
  };
  global_data_.RegisterClass<Class::Symbol>(cls);
  global_binder->def("Symbol", constructor, ATTR::W | ATTR::C);
  global_data_.set_symbol_prototype(proto);

  bind::Object(this, constructor)
      .def(symbol::prototype(), proto, ATTR::NONE)
#define V(name)\
      .def(symbol::name(),\
           global_data()->IV_CONCAT(builtin_symbol_, name)(), ATTR::NONE)
      IV_BUILTIN_SYMBOLS(V)
#undef V
      .def(global_data()->builtin_symbol_create(),
           JSInlinedFunction<
             &runtime::SymbolCreate, 0>::New(
                 this, Intern("[Symbol.create]")),
             ATTR::C);

  bind::Object(this, proto)
      .def(symbol::constructor(), constructor, ATTR::W | ATTR::C)
      .def<&runtime::SymbolToString, 0>("toString")
      .def<&runtime::SymbolValueOf, 0>("valueOf")
      .def(global_data()->builtin_symbol_toStringTag(),
           JSString::NewAsciiString(this, "Symbol", &dummy), ATTR::C)
      .def(global_data()->builtin_symbol_toPrimitive(),
           JSInlinedFunction<
             &runtime::SymbolToPrimitive, 1>::New(
                 this, Intern("[Symbol.toPrimitive]")),
             ATTR::C);
}

Symbol Context::Intern(const core::StringPiece& str) {
  return global_data()->Intern(str);
}

Symbol Context::Intern(const core::UStringPiece& str) {
  return global_data()->Intern(str);
}

Symbol Context::Intern(const JSString* str) {
  if (str->Is8Bit()) {
    return global_data()->Intern(*str->Get8Bit());
  } else {
    return global_data()->Intern(*str->Get16Bit());
  }
}

Symbol Context::Intern(uint32_t index) {
  return global_data()->InternUInt32(index);
}

Symbol Context::Intern(double number) {
  return global_data()->InternDouble(number);
}

Symbol Context::Intern64(uint64_t index) {
  return global_data()->Intern64(index);
}

} }  // namespace iv::lv5
