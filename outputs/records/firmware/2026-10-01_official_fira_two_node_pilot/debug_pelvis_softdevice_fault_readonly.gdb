set pagination off
set confirm off
target remote :2331
p m_error_data
p/x m_error_data.fault_id
p/x m_error_data.pc
p/x m_error_data.error_info
p/x m_error_data.err_code
p/x m_error_data.line_num
p m_error_data.p_file_name
detach
quit
