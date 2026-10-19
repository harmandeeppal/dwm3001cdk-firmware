# Qorvo's own QANI FiRa lifecycle owns UWB initialisation and its task. COMP971
# adds only a small identity/start adapter; it does not replace QANI internals.
add_library(fira_qani STATIC
  ${PROJECT_BASE}/Projects/FreeRTOS/QANI/Common/src/fira/fira_niq.c
  ../comp971_uwb_two_node.c
)
target_include_directories(fira_qani PUBLIC
  ${PROJECT_COMMON}/include
  ${CMAKE_CURRENT_LIST_DIR}/../../../../include
  ${PROJECT_BASE}/Projects/FreeRTOS/QANI/Common/src/fira
)
target_link_libraries(fira_qani PUBLIC fira niq Helpers)
