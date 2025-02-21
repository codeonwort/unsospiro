#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NAN_BOXING            1

#define DEBUG_PRINT_CODE      0
#define DEBUG_TRACE_EXECUTION 0
#define DEBUG_STRESS_GC       0
#define DEBUG_LOG_GC          0

#define UINT8_COUNT           (UINT8_MAX + 1)

#ifdef __cplusplus
#define CPLUSPLUS_BEGIN extern "C" {
#define CPLUSPLUS_END   }
#else
#define CPLUSPLUS_BEGIN ;
#define CPLUSPLUS_END   ;
#endif
