#include <stdio.h>
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/task.h"
#include "esp_timer.h"
#include "gpio_cxx.hpp"
#include "wifi.h"
#include "config.h"

#define DEBOUNCE_MS 1200

const auto DOOR_GPIO = idf::GPIONum(4);
const auto INTERNAL_LED_GPIO = idf::GPIONum(2);

enum class DoorState { OPEN, CLOSED };

void send_state_change(DoorState new_state) {
    printf("[!] State changed to %s, sending HTTP POST...\n", new_state == DoorState::OPEN ? "OPEN" : "CLOSED");

    // Build JSON payload
    char body[64];
    snprintf(body, sizeof(body),
             "{\"doorId\":\"%s\",\"isOpen\":%s}",
             DOOR_ID,
             new_state == DoorState::OPEN ? "true" : "false");

    int status = http_post(STATE_CHANGE_URL, body, "application/json", "x-webhook-secret", WEBHOOK_SECRET);
    if (status != 200) {
        printf("[!] HTTP POST failed with status %d!\n", status);
    } else {
        printf("[!] HTTP POST succeeded!\n");
    }
}

extern "C" void app_main() {
    printf("[!] Starting...\n");

    idf::GPIO_Output led(INTERNAL_LED_GPIO);

    if (!wifi_connect(WIFI_SSID, WIFI_PASS)) {
        printf("[!] Failed to connect to wifi!\n");
        led.set_high();
    } else {
        auto response = http_get("https://example.com");
        printf("[!] Test request response: %s\n", response.c_str());

        vTaskDelay(pdMS_TO_TICKS(1000));

        idf::GPIOInput door_sensor(DOOR_GPIO);
        door_sensor.set_pull_mode(idf::GPIOPullMode::PULLUP());

        // Read initial state
        bool is_high = door_sensor.get_level() == idf::GPIOLevel::HIGH;
        DoorState confirmed_state = is_high ? DoorState::OPEN : DoorState::CLOSED;
        DoorState pending_state = confirmed_state;
        int64_t pending_state_start_time = 0;

        printf("[!] Initial state: %s\n", confirmed_state == DoorState::OPEN ? "OPEN" : "CLOSED");

        while (true) {
            idf::GPIOLevel level = door_sensor.get_level();
            is_high = level == idf::GPIOLevel::HIGH;
            DoorState current_reading = is_high ? DoorState::OPEN : DoorState::CLOSED;

            // Update LED to match confirmed state
            if (confirmed_state == DoorState::OPEN) {
                led.set_high();
            } else {
                led.set_low();
            }

            // State change detection with debouncing
            if (current_reading != confirmed_state) {
                // We're seeing a different state than the confirmed one
                if (current_reading != pending_state) {
                    // This is a new potential state, start timing
                    pending_state = current_reading;
                    pending_state_start_time = esp_timer_get_time();
                    printf("[~] Potential state change detected: %s -> %s\n",
                           confirmed_state == DoorState::OPEN ? "OPEN" : "CLOSED",
                           pending_state == DoorState::OPEN ? "OPEN" : "CLOSED");
                } else {
                    // Same pending state, check if debounce time has elapsed
                    int64_t elapsed_ms = (esp_timer_get_time() - pending_state_start_time) / 1000;
                    if (elapsed_ms >= DEBOUNCE_MS) {
                        // State has been stable for long enough, confirm the transition
                        confirmed_state = pending_state;
                        send_state_change(confirmed_state);
                    }
                }
            } else {
                // Reading matches confirmed state, reset pending state
                if (pending_state != confirmed_state)
                    printf("[~] State change cancelled, back to %s\n",
                           confirmed_state == DoorState::OPEN ? "OPEN" : "CLOSED");
                pending_state = confirmed_state;
            }

            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}
