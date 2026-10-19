/*
 * COMP971 runtime persistence guard.
 *
 * The Qorvo SDK's source-level CONFIG_L1_CONFIG_VOLATILE mode keeps L1
 * configuration in RAM. The supplied UWB stack is prebuilt, so this local
 * guard supplies the equivalent qflash_write behaviour: successful RAM-only
 * operation and no MCU non-volatile-memory write.
 */
#pragma once

#include <stdint.h>

uint32_t comp971_flash_guard_blocked_write_count(void);
