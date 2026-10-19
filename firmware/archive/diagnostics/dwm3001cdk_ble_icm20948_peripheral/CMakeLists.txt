# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dwm3001cdk_ble_icm20948_peripheral)

target_sources(app PRIVATE
    src/main.c
    ../../dwm3001cdk_motion_node/src/icm20948_stream.c
)
target_include_directories(app PRIVATE
    include
    ../../dwm3001cdk_motion_node/src
)
