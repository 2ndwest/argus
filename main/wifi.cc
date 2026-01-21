#include "wifi.hpp"
#include <cstdio>
#include <cstring>
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

static EventGroupHandle_t s_wifi_events;
static int s_max_retries;
static int s_retry_count;

#define CONNECTED BIT0
#define FAILED    BIT1

static void event_handler(void*, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < s_max_retries) {
            esp_wifi_connect();
            printf("[~] WiFi retry %d/%d...\n", ++s_retry_count, s_max_retries);
        } else {
            xEventGroupSetBits(s_wifi_events, FAILED);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto* e = (ip_event_got_ip_t*)data;
        printf("[+] WiFi connected! IP: " IPSTR "\n", IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_wifi_events, CONNECTED);
    }
}

bool wifi_connect(const char* ssid, const char* password, int max_retries) {
    s_max_retries = max_retries;
    s_retry_count = 0;
    s_wifi_events = xEventGroupCreate();

    // Init NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) return false;

    // Init network stack
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    // Init WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, nullptr);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, nullptr);

    // Configure & start
    wifi_config_t wifi_cfg = {};
    strncpy((char*)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid));
    strncpy((char*)wifi_cfg.sta.password, password, sizeof(wifi_cfg.sta.password));

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();

    printf("[~] Connecting to %s...\n", ssid);

    // Wait for result
    EventBits_t bits = xEventGroupWaitBits(s_wifi_events, CONNECTED | FAILED, pdFALSE, pdFALSE, portMAX_DELAY);
    return (bits & CONNECTED) != 0;
}

// HTTP event handler to accumulate response data
static esp_err_t http_event_handler(esp_http_client_event_t* evt) {
    auto* response = (std::string*)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && response) {
        response->append((char*)evt->data, evt->data_len);
    }
    return ESP_OK;
}

std::string http_get(const char* url) {
    std::string response;

    esp_http_client_config_t config = {};
    config.url = url;
    config.event_handler = http_event_handler;
    config.user_data = &response;
    config.crt_bundle_attach = esp_crt_bundle_attach; // For HTTPS

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err != ESP_OK) {
        printf("[-] HTTP GET failed: %s\n", esp_err_to_name(err));
        response.clear();
    } else {
        int status = esp_http_client_get_status_code(client);
        printf("[+] HTTP GET %s -> %d (%d bytes)\n", url, status, (int)response.size());
    }

    esp_http_client_cleanup(client);
    return response;
}

int http_post(const char* url, const char* body, const char* content_type,
              const char* extra_header_name, const char* extra_header_value) {
    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_POST;
    config.crt_bundle_attach = esp_crt_bundle_attach; // For HTTPS

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", content_type);
    if (extra_header_name && extra_header_value) {
        esp_http_client_set_header(client, extra_header_name, extra_header_value);
    }
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    int status = 0;

    if (err != ESP_OK) {
        printf("[-] HTTP POST failed: %s\n", esp_err_to_name(err));
    } else {
        status = esp_http_client_get_status_code(client);
        printf("[+] HTTP POST %s -> %d\n", url, status);
    }

    esp_http_client_cleanup(client);
    return status;
}
