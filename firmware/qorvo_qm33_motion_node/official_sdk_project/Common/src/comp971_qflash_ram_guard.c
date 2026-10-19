/*
 * COMP971-owned replacement for the SDK NRFX qflash implementation.
 *
 * Deliberately does not include or call NVMC, SoftDevice flash, UICR, OTP, or
 * erase APIs. It is linked only in the official-FiRa pilot composition.
 */
#include <stdint.h>

#include "comp971_flash_guard.h"
#include "qerr.h"

static volatile uint32_t m_blocked_write_count;

enum qerr qflash_write(uint32_t dst_addr, void *src_addr, uint32_t size)
{
    (void)dst_addr;
    (void)src_addr;
    (void)size;

    /* Match Qorvo's CONFIG_L1_CONFIG_VOLATILE success path: the in-RAM L1
     * structure remains valid for this boot, but it is never persisted. */
    ++m_blocked_write_count;
    return QERR_SUCCESS;
}

uint32_t comp971_flash_guard_blocked_write_count(void)
{
    return m_blocked_write_count;
}
