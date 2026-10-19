#include "FreeRTOS.h"
#include "HAL_error.h"

__attribute__((weak)) void vApplicationIdleHook(void)
{
}

__attribute__((weak)) void vApplicationMallocFailedHook(void)
{
    error_handler(1, _ERR_Malloc_Failed);
}

__attribute__((weak)) void vPortSuppressTicksAndSleep(TickType_t expected_idle_time)
{
    (void)expected_idle_time;
}
