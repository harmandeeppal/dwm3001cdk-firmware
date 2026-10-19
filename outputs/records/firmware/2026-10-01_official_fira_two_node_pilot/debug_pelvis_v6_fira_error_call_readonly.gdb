set pagination off
set confirm off
target remote localhost:2331
break fira_niq.c:139
break fira_niq.c:147
break fira_niq.c:155
break fira_niq.c:164
break fira_niq.c:174
break app_error_save_and_stop
monitor reset
continue
printf "Stopped at:\n"
frame
info registers pc r0 r1 r2 r3
bt
detach
quit
