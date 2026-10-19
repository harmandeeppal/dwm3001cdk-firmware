set pagination off
set confirm off
target remote localhost:2331
interrupt
set $comp971_pc = $pc
printf "PC: 0x%08x\n", $comp971_pc
printf "m_qani_ready: "
p m_qani_ready
printf "m_start_failure_pending: "
p m_start_failure_pending
printf "m_start_failure_stage: "
p m_start_failure_stage
printf "m_start_failure_detail: "
p m_start_failure_detail
printf "fira_config.role: "
p fira_config.role
printf "Backtrace:\n"
bt
detach
quit
