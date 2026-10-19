set pagination off
set confirm off
target remote localhost:2331
break comp971_ble_pilot.c:133
monitor reset
continue
printf "BLE enable returned to the application.\n"
frame
p result
bt
detach
quit
