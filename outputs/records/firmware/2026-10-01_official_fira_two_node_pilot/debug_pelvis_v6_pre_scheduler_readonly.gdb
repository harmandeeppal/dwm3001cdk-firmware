set pagination off
set confirm off
target remote localhost:2331
break main.c:133
monitor reset
continue
printf "All application tasks were created; scheduler has not started yet.\n"
frame
bt
detach
quit
