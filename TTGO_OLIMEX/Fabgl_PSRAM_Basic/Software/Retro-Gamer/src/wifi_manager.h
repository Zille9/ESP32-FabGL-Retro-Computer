/*
 * WiFi Manager — WiFi connection + HTTP file server for Retro-Gamer
 * Reads wifi.json config from SD, connects, and runs a web file manager
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

// Initialize WiFi from /retro-go/config/wifi.json
// Returns true if config loaded (doesn't mean connected)
bool wifi_manager_init(void);

// Connect to the specified network index (0-3)
bool wifi_manager_connect(int network_index);

// Disconnect from WiFi
void wifi_manager_disconnect(void);

// Check if connected
bool wifi_manager_is_connected(void);

// Get IP address string (static buffer)
const char* wifi_manager_get_ip(void);

// Get SSID of current/configured network
const char* wifi_manager_get_ssid(int index);

// Get number of configured networks
int wifi_manager_get_network_count(void);

// Start HTTP file server on port 80
bool wifi_manager_start_server(void);

// Stop HTTP file server
void wifi_manager_stop_server(void);

// Process HTTP requests (call periodically from loop)
void wifi_manager_process(void);

// Sync time via NTP
bool wifi_manager_ntp_sync(const char *timezone);

#ifdef __cplusplus
}
#endif
