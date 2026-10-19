set pagination off
set confirm off
target remote localhost:2331
break fira_niq.c:133
break app_error_save_and_stop
monitor reset
continue
printf "Stopped at:\n"
frame
info registers pc r0 r1 r2 r3
bt
detach
quit
