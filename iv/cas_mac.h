#ifndef IV_CAS_MAC_H_
#define IV_CAS_MAC_H_
#include <libkern/OSAtomic.h>
#include <iv/thread.h>
namespace iv {
namespace core {
namespace thread {

inline int CompareAndSwap(volatile int* target,
                          int new_value, int old_value) {
  int prev;
  do {
    if (OSAtomicCompareAndSwapInt(old_value, new_value, target)) {
     return old_value;
    }
    prev = *target;
  } while (prev == old_value);
  return prev;
}

} } }  // namespace iv::core::thread
#endif  // IV_CAS_MAC_H_
