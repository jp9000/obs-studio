#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint64_t gettime_100ns(void);
extern bool sleepto_100ns(uint64_t time_target);

#ifdef __cplusplus
}
#endif
