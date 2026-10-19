set pagination off
set confirm off
target remote localhost:2331
break fira_niq.c:139
break fira_niq.c:147
break fira_niq.c:155
break fira_niq.c:164
break fira_niq.c:174
monitor reset
continue
printf "Stopped at: "
frame
printf "QANI role: "
p fira_config.role
printf "Source address: "
p fira_config.SRC_ADDR
printf "Destination address: "
p fira_config.DST_ADDR
printf "Backtrace:\n"
bt
detach
quit
