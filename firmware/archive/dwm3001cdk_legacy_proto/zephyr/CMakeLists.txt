cmake_minimum_required(VERSION 3.13.1)
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(DIY-UWB)

# ── dwt_uwb_driver (Qorvo DW3110 low-level driver) ───────────────────────────
set(DWT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../lib/dwt_uwb_driver)

target_sources(app PRIVATE
    # Application sources
    ../src/main.c
    ../src/icm20948_stream.c
    ../src/ble_motion_transport.c

    # DW3xxx core driver
    ${DWT_DIR}/deca_interface.c
    ${DWT_DIR}/deca_compat.c
    ${DWT_DIR}/deca_rsl.c

    # DW3000/QM33110 chip-specific driver
    ${DWT_DIR}/dw3000/dw3000_device.c

    # Math library (used internally by the driver)
    ${DWT_DIR}/lib/qmath/src/qmath.c
)

target_include_directories(app PRIVATE
    ${DWT_DIR}
    ${DWT_DIR}/dw3000
    ${DWT_DIR}/lib/qmath/include
)

# Optional coordinator-only test mode. Leave undefined for normal full-mesh
# firmware; a value 0..14 limits the PELVIS coordinator to one mesh pair.
if(DEFINED COMP971_UWB_PAIR_ONLY_INDEX)
    target_compile_definitions(app PRIVATE
        COMP971_UWB_PAIR_ONLY_INDEX=${COMP971_UWB_PAIR_ONLY_INDEX}
    )
endif()

# Pair-only bench diagnostic: preserve pre-recovery DW3110 SYS_STATUS and
# SYS_STATE_LO values in the existing UWB failure event detail.
if(COMP971_UWB_TX_STATUS_DIAGNOSTICS)
    target_compile_definitions(app PRIVATE
        COMP971_UWB_TX_STATUS_DIAGNOSTICS=1
    )
endif()
