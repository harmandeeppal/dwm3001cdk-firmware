set pagination off
set confirm off
target remote localhost:2331

# These are instruction addresses recovered from the exact PELVIS v7 HEX.
# Hardware breakpoints live in the Cortex-M debug unit. They do not alter
# application flash, UICR, OTP, antenna calibration, or RAM configuration.
hbreak *0x00022584
commands
  silent
  printf "TRACE CreateQaniTask reached\n"
  continue
end

hbreak *0x0002264c
commands
  silent
  printf "TRACE StartUWB reached\n"
  continue
end

hbreak *0x00022458
commands
  silent
  printf "TRACE fira_start_niq reached\n"
  continue
end

hbreak *0x00021e18
commands
  silent
  printf "TRACE fira_niq_app_process_init reached\n"
  continue
end

continue
