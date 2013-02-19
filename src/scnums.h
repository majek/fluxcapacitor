#if defined(__x86_64__)
#  include "scnums_amd64.h"
#elif defined(__i386__)
#  include "scnums_x86.h"
#elif defined(__ARMEL__)
#  include "scnums_arm.h"
#else
#  error "Unknown architecture!"
#endif
