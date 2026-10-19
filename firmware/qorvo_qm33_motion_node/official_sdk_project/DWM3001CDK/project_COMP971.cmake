set(MY_BOARD DWM3001CDK)
set(MY_OS FreeRTOS)
set(MY_SAMPLE COMP971)
set(MY_HAL nrfx)
set(MY_BSP Nordic)
set(MY_CPU NRF52833_XXAA)
set(MY_LD_FILE nRF52833.ld)
set(MY_TARGET ${MY_BOARD}-${MY_SAMPLE}-${MY_OS})

# Every wearable uses the same source. These two cache settings are the only
# identity difference in the IMU/BLE stage. UWB coordinator behaviour will be
# added later as a separate explicit role setting.
set(COMP971_NODE_INDEX "0" CACHE STRING "COMP971 node index fallback")
set(COMP971_DEVICE_NAME "COMP971-NODE" CACHE STRING "Bluetooth name fallback")
set(COMP971_UWB_ROLE "1" CACHE STRING "UWB role: 1 enables complete UWB stack in binary; actual role discovered via FICR")
set(COMP971_NUM_CONTROLEES "5" CACHE STRING "Number of controlees for full star")
set(COMP971_TEST_HOOKS "1" CACHE STRING "Compile test hooks: 1=enabled (bench/stress), 0=disabled (production)")
if (NOT COMP971_NODE_INDEX MATCHES "^[0-5]$")
  message(FATAL_ERROR "COMP971_NODE_INDEX must be an integer from 0 to 5")
endif()
if (NOT COMP971_UWB_ROLE MATCHES "^[0-2]$")
  message(FATAL_ERROR "COMP971_UWB_ROLE must be 0 (disabled), 1 (controller), or 2 (responder)")
endif()

set(PROJECT_ARCH m4)
set(PROJECT_FP hard)
set(PROJECT_FPU fpv4-sp-d16)

# These values are deliberately set before Qorvo's sdk_config.h defaults are
# included.  The existing gateway requires an ATT MTU of at least 209 bytes
# for a complete nine-sample IMU batch.
add_definitions(
  -DCOMP971_NODE_INDEX=${COMP971_NODE_INDEX}
  -DCOMP971_DEVICE_NAME=\"${COMP971_DEVICE_NAME}\"
  -DCOMP971_UWB_ROLE=${COMP971_UWB_ROLE}
  -DCOMP971_NUM_CONTROLEES=${COMP971_NUM_CONTROLEES}
  -DCOMP971_TEST_HOOKS=${COMP971_TEST_HOOKS}
  -DNRF_SDH_BLE_GATT_MAX_MTU_SIZE=247
  -DNRF_SDH_BLE_GAP_DATA_LENGTH=251
  -DNRFX_TWIM_ENABLED=1
  -DNRFX_TWIM0_ENABLED=0
  -DNRFX_TWIM1_ENABLED=1
  -DNRFX_TWIM_DEFAULT_CONFIG_IRQ_PRIORITY=6
  # Qorvo's SDK also maps its older TWI settings to the nrfx settings.
  # Keep that mapping consistent: use TWIM1 (DMA/I2C), not legacy TWI1.
  -DTWI_ENABLED=1
  -DTWI1_ENABLED=1
  -DTWI1_USE_EASY_DMA=1
  -DBOARD_CUSTOM
  -DBOARD_DWM3001_A1
  -DCONFIG_GPIO_AS_PINRESET
  -DNRF52833_XXAA
  -DNRF_SD_BLE_API_VERSION=7
  -DS113
  -DSOFTDEVICE_PRESENT
  -DINITIALIZE_USER_SECTIONS
  -DNO_VTOR_CONFIG
)

set(CMAKE_CUSTOM_C_FLAGS "-Werror -DDEBUG")
add_definitions(-Os)
set(CMAKE_ASM_FLAGS "-DS113 -DSOFTDEVICE_PRESENT")
set(USE_DRV_DW3000 1)
set(USE_DRV_DW3720 0)
set(USE_CRYPTO_BACKEND_MBEDTLS 1)
set(USE_CRYPTO_SPEED_OPTIMIZATION 0)

# The official FiRa startup imports the board's factory radio values from OTP
# into its L1 configuration.  For COMP971 these values must remain in RAM:
# never commit configuration, calibration, or antenna values to MCU flash.
# Both the CMake option and the compiler definition are required because the
# SDK uses the option to omit persistent-storage sources and the definition
# to make l1_config_store_key return after updating RAM.
set(CONFIG_L1_CONFIG_VOLATILE ON)
add_definitions(-DCONFIG_L1_CONFIG_VOLATILE)
