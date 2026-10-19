# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.19)

enable_language(C ASM)

set(EXECUTABLE ${PROJECT_NAME}.elf)

message("Target is ${MY_TARGET}")

add_executable(${EXECUTABLE} ${PROJECT_COMMON}/src/main.c ${PROJECT_COMMON}/src/hooks.c)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Select the applications to use. This local composition is deliberately based
# on Qorvo's QANI FreeRTOS composition, but it does not include QANI or NIQ.
# COMP971 owns only the high-level fixed two-board adapter; Qorvo's FiRa,
# UWB-MAC and DW3110 driver remain unchanged.
set(USE_FIRA 1)
set(USE_SOFTDEVICE 1)
# The SDK's fira CMake uses this variable only to decide whether to compile
# its CLI-oriented fira_app.c wrapper. Keep it enabled so that wrapper is not
# linked: COMP971 uses the same lower FiRa/UWB-MAC APIs from its own adapter.
# No NIQ source, library, application task or start signal is included below.
set(USE_NIQ 1)
# Qorvo common_fira.h selects its lightweight QLOG implementation with this
# compile definition. This is only a logging-interface selection: the QANI
# application sources, task and start signal are not in this project.
add_definitions(-DQANI_BUILD)

set(USE_BOARD_LIB ON)
set(CONFIG_QM33 1)

# Disable RTT logs to reduce power consumption
set(CONFIG_LOG OFF)

# 0=NONE 1=ERROR 2=WARNING 3=INFO 4=DEBUG
set(CONFIG_QLOG_LEVEL 3)

set(CMAKE_C_FLAGS
    "-mcpu=cortex-${PROJECT_ARCH}\
        -mfpu=${PROJECT_FPU}\
        -mfloat-abi=${PROJECT_FP}\
        -mthumb\
        -specs=nano.specs\
        -specs=nosys.specs\
        -fdata-sections\
        -ffunction-sections\
        -std=c11\
        -Wall\
        -g3\
        -D${MY_CPU}\
        ${CMAKE_CUSTOM_C_FLAGS}"
)
set(CMAKE_ASM_FLAGS
    "-mcpu=cortex-${PROJECT_ARCH}\
        -mfpu=${PROJECT_FPU}\
        -mfloat-abi=${PROJECT_FP}\
        -mthumb\
        -fdata-sections\
        -ffunction-sections\
        -Wall\
        -g3\
        -D${MY_CPU}\
        -D__STACK_SIZE=2048\
        -D__HEAP_SIZE=0\
        ${CMAKE_CUSTOM_C_FLAGS}"
)

set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} -Wl,--wrap=dwt_isr -u _printf_float -Wl,--gc-sections,-T${CMAKE_SOURCE_DIR}/${MY_LD_FILE},-Map=${PROJECT_NAME}.map -Wl,--print-memory-usage"
)

# Flags for stack
set(CONFIG_QOSAL_IMPL_FREERTOS 1)

if(CONFIG_LOG)
  add_subdirectory(${PROJECT_BASE}/Src/Logger logger)
  list(APPEND LINK_LIB_LIST "logger")
endif()

# set l1 config needs (used on Src/UWB/CMakelists.txt)
set(CONFIG_L1_CONFIG_CUSTOM_DEFAULT_USE_DEDICATED_SECTION ON)
set(CONFIG_SECURE_PARTITIONS_UWB_L1_CONFIG_SHA256_SIZE 32)
set(CONFIG_SECURE_PARTITIONS_UWB_L1_CONFIG_SIZE 4096)
# Linker calib flags
set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} \
-Wl,--defsym=CONFIG_SECURE_PARTITIONS_UWB_L1_CONFIG_SHA256_SIZE=${CONFIG_SECURE_PARTITIONS_UWB_L1_CONFIG_SHA256_SIZE}"
)

set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} \
-Wl,--defsym=CONFIG_SECURE_PARTITIONS_UWB_L1_CONFIG_SIZE=${CONFIG_SECURE_PARTITIONS_UWB_L1_CONFIG_SIZE}"
)

# Set qhal needs
set(CONFIG_QHAL_IMPL_NRFX ON)
set(CONFIG_QPLATFORM_IMPL_QM33_QHAL_NON_ZEPHYR ON)
set(CONFIG_QHAL_MAX_GPIO_CALLBACKS 6)

list(APPEND LINK_LIB_LIST "qhal")
add_subdirectory(${PROJECT_BASE}/${LIBS_PATH}/uwb-stack/libs/qhal qhal)

# The vendor's NRFX qflash implementation erases and writes MCU flash when
# the prebuilt UWB stack persists L1 configuration. Qorvo's source-level
# CONFIG_L1_CONFIG_VOLATILE mode instead returns success after the RAM update.
# The SDK ships no volatile prebuilt stack variant, so exclude only qflash.c
# from this application composition and provide that documented RAM-only
# equivalent. Qorvo radio, UWB-MAC, FiRa, BLE and HAL sources remain unchanged.
get_target_property(COMP971_QHAL_SOURCES qhal SOURCES)
list(FILTER COMP971_QHAL_SOURCES EXCLUDE REGEX "(.*/)?src/nrfx/qflash\\.c$")
set_property(TARGET qhal PROPERTY SOURCES "${COMP971_QHAL_SOURCES}")
target_sources(qhal PRIVATE ${PROJECT_COMMON}/src/comp971_qflash_ram_guard.c)
target_include_directories(qhal PRIVATE ${PROJECT_COMMON}/include)

list(APPEND LINK_LIB_LIST "qplatform")
add_subdirectory(${PROJECT_BASE}/${LIBS_PATH}/uwb-stack/libs/qplatform qplatform)

target_compile_definitions(
  qhal PRIVATE CONFIG_QHAL_MAX_GPIO_CALLBACKS=${CONFIG_QHAL_MAX_GPIO_CALLBACKS}
)

target_compile_definitions(qplatform PRIVATE CONFIG_QPLATFORM_IMPL_NRF)

set(CONFIG_CFG_MANAGER_VOLATILE_MODE ON)
add_definitions(-DCONFIG_CFG_MANAGER_VOLATILE_MODE)

# Include external libraries
include(${COMMON_PATH}/uwbstack.cmake)
include(${COMMON_PATH}/niq.cmake)
include(${COMMON_PATH}/dwt_uwbdriver.cmake)

# Include internal libraries
add_subdirectory(ProjectDefinition)
add_subdirectory(${PROJECT_BASE}/SDK_BSP/${MY_BSP} ${MY_BSP})
add_subdirectory(${PROJECT_BASE}/Src/OS/${MY_OS} ${MY_OS})

add_subdirectory(${PROJECT_BASE}/Src/OSAL/Src/${MY_OS} OSAL)
list(APPEND LINK_LIB_LIST "OSAL")

add_subdirectory(${PROJECT_BASE}/Src/AppConfig AppConfig)

add_subdirectory(${PROJECT_BASE}/Src/UWB UWB)
list(APPEND LINK_LIB_LIST "UWB")

add_subdirectory(${PROJECT_BASE}/Src/Boards/Src/${MY_BOARD} platform_l1)
list(APPEND LINK_LIB_LIST "platform_l1")

add_subdirectory(${PROJECT_BASE}/Src/HAL/Src/${MY_HAL} HAL)
list(APPEND LINK_LIB_LIST "HAL")

add_subdirectory(${PROJECT_BASE}/Src/Apps/Src/common AppCommon)
list(APPEND LINK_LIB_LIST "app_common")

target_link_libraries(qhal PUBLIC HAL)

list(APPEND LINK_LIB_LIST "qosal")

add_subdirectory(${PROJECT_BASE}/Src/Comm/Src/BLE BLE)
list(APPEND LINK_LIB_LIST "BLE")

add_subdirectory(${PROJECT_BASE}/Src/Helpers Helpers)
list(APPEND LINK_LIB_LIST "Helpers")

add_subdirectory(${PROJECT_BASE}/Src/Comm Comm)
list(APPEND LINK_LIB_LIST "Interface")

if(USE_FIRA)
  add_subdirectory(${PROJECT_BASE}/Src/Apps/Src/fira AppFira)
  list(APPEND LINK_LIB_LIST "fira")
endif()

add_library(comp971_official_fira STATIC
  ${PROJECT_COMMON}/src/comp971_uwb_official_fira.c
)
target_include_directories(comp971_official_fira PUBLIC
  ${PROJECT_COMMON}/include
  ${PROJECT_COMMON}/../../include
)
set_source_files_properties(
  ${PROJECT_COMMON}/src/main.c
  ${PROJECT_COMMON}/src/hooks.c
  ${PROJECT_COMMON}/src/comp971_uwb_official_fira.c
  ${PROJECT_COMMON}/src/comp971_node_identity.c
  PROPERTIES COMPILE_OPTIONS "-Wundef"
)
target_link_libraries(comp971_official_fira PUBLIC fira Helpers)
list(APPEND LINK_LIB_LIST "comp971_official_fira")

list(APPEND STATIC_LINK_LIB_LIST "uwbstack_core")

foreach(lib ${LINK_LIB_LIST})
  target_link_libraries(${EXECUTABLE} ${lib})
endforeach(lib in)

foreach(lib ${STATIC_LINK_LIB_LIST})
  target_link_libraries(${EXECUTABLE} ${lib})
endforeach(lib in)

target_link_libraries(${EXECUTABLE} -Wl,--no-whole-archive c m nosys)

# SoftDeviceNrfx is used only to include the SoftDevice in generated hex. It is isolated from the
# BLE library to force the linker to include Soft Device table only.
add_subdirectory(${PROJECT_BASE}/Src/Comm/Src/BLE/nrfx/softdevice SoftDeviceNrfx)
target_link_libraries(${EXECUTABLE} -Wl,--whole-archive SoftDeviceNrfx -Wl,--no-whole-archive)

add_custom_command(
  TARGET ${PROJECT_NAME}.elf
  POST_BUILD
  # copy target image to other formats
  COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
  COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
)

# Disable APPROTECT
set(CONFIG_SOC_PROTECT 0)
