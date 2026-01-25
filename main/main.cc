#include <stdio.h>
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "gpio_cxx.hpp"
#include "wifi.h"
#include "config.h"
#include "esp_adc/adc_oneshot.h"

#define DEBOUNCE_MS 100
#define AUTO_RESTART_MINUTES 30

// Timer callback to restart the device
static void auto_restart_callback(void* arg) {
    printf("[!] Auto-restart timer fired, restarting...\n");
    esp_restart();
}

const auto INTERNAL_LED_GPIO = idf::GPIONum(2);

// Hall sensor on GPIO 33 = ADC1 channel 5
#define HALL_ADC_CHANNEL ADC_CHANNEL_5

enum class LockState { UNLOCKED, LOCKED };

void send_state_change(LockState new_state) {
    printf("[!] State changed to %s, sending HTTP POST...\n", new_state == LockState::LOCKED ? "LOCKED" : "UNLOCKED");

    // Build JSON payload
    char body[64];
    snprintf(body, sizeof(body),
             "{\"bathroomId\":\"%s\",\"isLocked\":%s}",
             BATHROOM_ID,
             new_state == LockState::LOCKED ? "true" : "false");

    int status = http_post(STATE_CHANGE_URL, body, "application/json", "x-webhook-secret", WEBHOOK_SECRET);
    if (status != 200) {
        printf("[!] HTTP POST failed with status %d!\n", status);
    } else {
        printf("[!] HTTP POST succeeded!\n");
    }
}

extern "C" void app_main() {
    printf("[!] Starting...\n");

    // Set up auto-restart timer
    const esp_timer_create_args_t restart_timer_args = {
        .callback = &auto_restart_callback,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "auto_restart",
        .skip_unhandled_events = false,
    };
    esp_timer_handle_t restart_timer;
    ESP_ERROR_CHECK(esp_timer_create(&restart_timer_args, &restart_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(restart_timer, AUTO_RESTART_MINUTES * 60 * 1000000ULL));
    printf("[!] Auto-restart scheduled in %d minutes\n", AUTO_RESTART_MINUTES);

    idf::GPIO_Output led(INTERNAL_LED_GPIO);

    if (!wifi_connect(WIFI_SSID, WIFI_PASS)) {
        printf("[!] Failed to connect to wifi!\n");
        led.set_high();
    } else {
        // Configure ADC1 for Hall sensor.
        adc_oneshot_unit_handle_t adc1_handle;
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
            .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

        adc_oneshot_chan_cfg_t channel_config = {
            .atten = ADC_ATTEN_DB_12,  // Full range 0-3.3V
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, HALL_ADC_CHANNEL, &channel_config));

        vTaskDelay(pdMS_TO_TICKS(1000));

        printf("[!] ADC configured, reading hall sensor...\n");

        // Read initial state
        int raw_value = 0;
        adc_oneshot_read(adc1_handle, HALL_ADC_CHANNEL, &raw_value);
        LockState confirmed_state = (raw_value > HALL_THRESHOLD) ? LockState::LOCKED : LockState::UNLOCKED;
        LockState pending_state = confirmed_state;
        int64_t pending_state_start_time = 0;

        printf("[!] Initial Hall reading: %d, state: %s\n", raw_value,
               confirmed_state == LockState::LOCKED ? "LOCKED" : "UNLOCKED");

        // Post initial state to server
        send_state_change(confirmed_state);

        while (true) {
            adc_oneshot_read(adc1_handle, HALL_ADC_CHANNEL, &raw_value);
            LockState current_reading = (raw_value > HALL_THRESHOLD) ? LockState::LOCKED : LockState::UNLOCKED;

            // Update LED to match confirmed state (LED on = locked, LED off = unlocked)
            if (confirmed_state == LockState::LOCKED) {
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
                    printf("[~] Potential state change detected: %s -> %s (raw: %d)\n",
                           confirmed_state == LockState::LOCKED ? "LOCKED" : "UNLOCKED",
                           pending_state == LockState::LOCKED ? "LOCKED" : "UNLOCKED",
                           raw_value);
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
                           confirmed_state == LockState::LOCKED ? "LOCKED" : "UNLOCKED");
                pending_state = confirmed_state;
            }

            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}
