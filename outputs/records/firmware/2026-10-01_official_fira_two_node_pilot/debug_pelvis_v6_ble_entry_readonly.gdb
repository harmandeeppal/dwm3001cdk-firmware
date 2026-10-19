set pagination off
set confirm off
target remote localhost:2331
break comp971_ble_pilot.c:127
monitor reset
continue
printf "Reached BLE initialisation before the fault.\n"
frame
bt
detach
quit
