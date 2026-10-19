set pagination off
set confirm off
target remote :2331
info registers
info threads
thread apply all bt
detach
quit
