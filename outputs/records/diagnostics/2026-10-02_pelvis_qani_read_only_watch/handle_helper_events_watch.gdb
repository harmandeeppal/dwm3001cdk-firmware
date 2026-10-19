set pagination off
set confirm off
target remote :2331
hbreak handle_helper_events
commands 1
  silent
  printf "QANI_EVENT cb_type=%d\\n", cb_type
  continue
end
continue
