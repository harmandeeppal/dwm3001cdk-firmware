#ifndef COMP971_NODE_IDENTITY_H
#define COMP971_NODE_IDENTITY_H

#include <stdint.h>
#include <stdbool.h>
#include "comp971_config_shared.h"

/* Runtime active identity populated by comp971_identify_node() from FICR */
extern volatile uint8_t  g_comp971_node_index;
extern volatile uint8_t  g_comp971_uwb_role;
extern volatile uint16_t g_comp971_short_addr;
extern const char       *g_comp971_device_name;
extern volatile uint16_t g_comp971_restart_jitter_ms;
extern volatile bool     g_comp971_identity_valid;
extern volatile uint32_t g_comp971_config_hash;

/* Call early in main() before BLE and UWB initialization */
void comp971_identify_node(void);

#endif /* COMP971_NODE_IDENTITY_H */
