set pagination off
set confirm off
target remote localhost:2331
break fira_niq.c:128
monitor reset
continue
printf "Reached Qorvo measurement-sequence preparation.\n"
frame
p controller
p fira_param->session.short_addr
p fira_param->session.destination_short_address[0]
p fira_param->session.channel_number
bt
detach
quit
