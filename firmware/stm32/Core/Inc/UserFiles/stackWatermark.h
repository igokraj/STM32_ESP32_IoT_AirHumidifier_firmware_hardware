#pragma once
#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Symbols from the linker script (STM32F446XX_FLASH.ld):
// _sstack = bottom of the reserved stack region, _estack = top of RAM / initial SP
extern uint32_t _sstack;
extern uint32_t _estack;

#define STACKWATERMARK_PATTERN       0xA5A5A5A5u
#define STACKWATERMARK_MARGIN_BYTES  32u   // never touch memory close to the current SP

// Call once, as early as possible in main() (before other function calls grow the stack)
static inline void StackWatermark_Init(void) {
    uint32_t *p = &_sstack;
    uint32_t *limit = (uint32_t *)(__get_MSP() - STACKWATERMARK_MARGIN_BYTES);

    while (p < limit) {
        *p++ = STACKWATERMARK_PATTERN;
    }
}

// Call any time later to read the deepest stack usage (in bytes) seen so far
static inline uint32_t StackWatermark_GetMaxUsedBytes(void) {
    uint32_t *p = &_sstack;
    uint32_t *end = &_estack;

    while (p < end && *p == STACKWATERMARK_PATTERN) {
        p++;
    }

    return (uint32_t)((uint8_t *)&_estack - (uint8_t *)p);
}

#ifdef __cplusplus
}
#endif
