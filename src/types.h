#ifndef _HAVE_TYPES_H
#define _HAVE_TYPES_H

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

#if defined(__x86_64__)
	typedef __int128  s128;
	typedef s128 flux_time;
#else
	typedef u64 flux_time;
#endif


#ifndef MIN
#  define MIN(_a,_b) ((_a) > (_b) ? (_b) : (_a))
#endif /* !MIN */

#ifndef MAX
#  define MAX(_a,_b) ((_a) > (_b) ? (_a) : (_b))
#endif /* !MAX */

#endif /* ^_HAVE_TYPES_H */
