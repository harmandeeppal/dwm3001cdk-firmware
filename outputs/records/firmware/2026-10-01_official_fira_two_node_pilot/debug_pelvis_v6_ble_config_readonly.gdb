set pagination off
set confirm off
target remote localhost:2331
break comp971_ble_pilot.c:132
monitor reset
continue
printf "BLE default configuration returned to the application.\n"
frame
p result
bt
detach
quit
