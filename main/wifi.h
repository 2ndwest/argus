#pragma once
#include <string>

// Simple WiFi helper - connects to WiFi and blocks until connected or fails
// Returns true on success, false on failure
bool wifi_connect(const char* ssid, const char* password, int max_retries = 5);

// HTTP GET request - returns response body (empty string on error)
std::string http_get(const char* url);

// HTTP POST request - returns HTTP status code (0 on connection error)
// Optional extra_header_name/value for custom headers (e.g., "x-webhook-secret")
int http_post(const char* url, const char* body, const char* content_type = "application/json",
              const char* extra_header_name = nullptr, const char* extra_header_value = nullptr);
