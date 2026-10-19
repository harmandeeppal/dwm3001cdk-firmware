set pagination off
set confirm off
target remote localhost:2331
break main.c:122
monitor reset
continue
printf "Custom BLE setup completed without an error.\n"
frame
bt
detach
quit
