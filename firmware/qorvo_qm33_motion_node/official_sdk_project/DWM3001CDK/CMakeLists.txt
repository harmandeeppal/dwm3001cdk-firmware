cmake_minimum_required(VERSION 3.19)

include(${MY_TARGET_SCRIPT})
project(${MY_TARGET})

# Reuse a project-local, traceable copy of Qorvo's DWM3001CDK/FreeRTOS build
# composition. It retains Qorvo's FiRa/UWB-MAC/DW3110 layers but deliberately
# excludes the iPhone-oriented QANI/NIQ application path.
include(${CMAKE_CURRENT_LIST_DIR}/COMP971-OfficialFira-FreeRTOS.cmake)

# QANI's CMake file expects a linker script beside its own source directory.
# This project deliberately keeps Qorvo's unmodified DWM3001CDK linker script
# in the frozen SDK working copy instead of copying or editing it.
string(REPLACE
  "-T${CMAKE_SOURCE_DIR}/${MY_LD_FILE}"
  "-T${PROJECT_BASE}/Projects/FreeRTOS/QANI/DWM3001CDK/${MY_LD_FILE}"
  CMAKE_EXE_LINKER_FLAGS
  "${CMAKE_EXE_LINKER_FLAGS}"
)

target_sources(${EXECUTABLE} PRIVATE
  ${PROJECT_COMMON}/src/comp971_node_identity.c
  ${PROJECT_COMMON}/src/comp971_ble_pilot.c
  ${PROJECT_COMMON}/src/comp971_icm20948.c
  ${PROJECT_BASE}/SDK_BSP/Nordic/SDK_17_1_0/modules/nrfx/drivers/src/nrfx_twim.c
)
target_include_directories(${EXECUTABLE} PRIVATE
  ${PROJECT_COMMON}/include
  ${CMAKE_CURRENT_LIST_DIR}/../../include
  ${PROJECT_BASE}/SDK_BSP/Nordic/SDK_17_1_0/components/ble/ble_advertising
  ${PROJECT_BASE}/SDK_BSP/Nordic/SDK_17_1_0/components/ble/nrf_ble_gatt
  ${PROJECT_BASE}/SDK_BSP/Nordic/SDK_17_1_0/modules/nrfx/drivers/include
)
