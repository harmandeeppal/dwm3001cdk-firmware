set pagination off
set confirm off
target remote :2331
p/x pxCurrentTCB
p *pxCurrentTCB
detach
quit
