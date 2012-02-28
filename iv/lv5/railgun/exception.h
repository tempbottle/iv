#ifndef IV_LV5_RAILGUN_EXCEPTION_H_
#define IV_LV5_RAILGUN_EXCEPTION_H_
#include <iv/detail/cstdint.h>
#include <iv/detail/tuple.h>
#include <iv/lv5/gc_template.h>
namespace iv {
namespace lv5 {
namespace railgun {

class Handler {
 public:
  enum Type {
    CATCH,
    FINALLY,
    ITERATOR
  };

  Handler(Type type,
          uint32_t begin,
          uint32_t end,
          int16_t jmp,
          int16_t ret,
          int16_t flag,
          uint32_t dynamic_env_level)
    : type_(type),
      jmp_(jmp),
      ret_(ret),
      flag_(flag),
      begin_(begin),
      end_(end),
      dynamic_env_level_(dynamic_env_level) { }

  Type type() const { return type_; }

  uint32_t begin() const { return begin_; }

  uint32_t end() const { return end_; }

  int16_t jmp() const { return jmp_; }

  int16_t ret() const { return ret_; }

  int16_t flag() const { return flag_; }

  uint32_t dynamic_env_level() const { return dynamic_env_level_; }

 private:
  Type type_;
  int16_t jmp_;
  int16_t ret_;
  int16_t flag_;
  uint32_t begin_;
  uint32_t end_;
  uint32_t dynamic_env_level_;
};

typedef GCVector<Handler>::type ExceptionTable;


} } }  // namespace iv::lv5::railgun
#endif  // IV_LV5_RAILGUN_EXCEPTION_H_
