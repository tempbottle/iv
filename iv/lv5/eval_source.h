#ifndef IV_LV5_EVAL_SOURCE_H_
#define IV_LV5_EVAL_SOURCE_H_
#include <iv/noncopyable.h>
#include <iv/source_traits.h>
#include <iv/lv5/jsstring.h>
namespace iv {
namespace lv5 {
namespace detail {

static const std::string kEvalSource = "(eval)";

}  // namespace detail

class EvalSource : public core::Noncopyable<> {
 public:
  explicit EvalSource(const JSString& str)
    : source_() {
    source_.resize(str.size());
    str.Copy(source_.begin());
  }

  inline char16_t operator[](std::size_t pos) const {
    assert(pos < size());
    return source_[pos];
  }

  inline std::size_t size() const {
    return source_.size();
  }

  core::UStringPiece GetData() const {
    return source_;
  }

 private:
  core::UString source_;
};

}  // namespace lv5
namespace core {

template<>
struct SourceTraits<lv5::EvalSource> {
  static std::string GetFileName(const lv5::EvalSource& src) {
    return lv5::detail::kEvalSource;
  }

  static core::UStringPiece SubString(
      const lv5::EvalSource& src,
      std::size_t n, std::size_t len = std::string::npos) {
    return src.GetData().substr(n, len);
  }
};

} }  // namespace iv::core
#endif  // IV_LV5_EVAL_SOURCE_H_
