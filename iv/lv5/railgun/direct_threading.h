#ifndef IV_LV5_RAILGUN_DIRECT_THREADING_H_
#define IV_LV5_RAILGUN_DIRECT_THREADING_H_
#include <iv/platform.h>
namespace iv {
namespace lv5 {
namespace railgun {

#if defined(IV_USE_DIRECT_THREADED_CODE) && (defined(IV_COMPILER_GCC) || defined(IV_COMPILER_CLANG))  // NOLINT
#define IV_LV5_RAILGUN_USE_DIRECT_THREADED_CODE
#endif

enum DispatchTableTag { DIRECT_THREADED_DISPATCH_TABLE };

} } }  // namespace iv::lv5::railgun
#endif  // IV_LV5_RAILGUN_DIRECT_THREADING_H_
