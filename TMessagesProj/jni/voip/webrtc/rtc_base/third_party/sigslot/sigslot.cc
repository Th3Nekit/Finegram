#include "rtc_base/third_party/sigslot/sigslot.h"

namespace sigslot {

#ifdef _SIGSLOT_HAS_POSIX_THREADS

pthread_mutex_t* multi_threaded_global::get_mutex() {
  static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
  return &g_mutex;
}

#endif

}
