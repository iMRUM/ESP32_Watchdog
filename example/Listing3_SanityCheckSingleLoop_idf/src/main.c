/*
 * Listing 3 — Sanity Check (Single Loop) Watchdog Timer (ESP-IDF)
 *
 * Three "sub-tasks" run sequentially in a single loop, each setting a flag
 * when complete. The dog is only fed if all three flags were set during that
 * iteration. After HANG_AT_ITERATION loops the system deliberately hangs to
 * demonstrate WDT recovery.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_log.h"

#define WDT_TIMEOUT_MS    4000
#define HANG_AT_ITERATION 25

static const char *TAG = "SanityLoop";

void app_main(void)
{
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms    = WDT_TIMEOUT_MS,
        .idle_core_mask = 0,
        .trigger_panic  = true,
    };
    esp_err_t ret = esp_task_wdt_init(&wdt_config);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(esp_task_wdt_reconfigure(&wdt_config));
    } else {
        ESP_ERROR_CHECK(ret);
    }
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    int flag[3]  = {0, 0, 0};
    int iteration = 0;

    while (1) {
        iteration++;

        // Simulate an MCU hang after HANG_AT_ITERATION loops
        if (iteration >= HANG_AT_ITERATION) {
            ESP_LOGW(TAG, "Simulating MCU hang event!");
            while (1) { ; }
        }

        ESP_LOGI(TAG, "Task 1");
        flag[0] = 1;
        vTaskDelay(pdMS_TO_TICKS(100));

        ESP_LOGI(TAG, "Task 2");
        flag[1] = 1;
        vTaskDelay(pdMS_TO_TICKS(100));

        ESP_LOGI(TAG, "Task 3");
        flag[2] = 1;
        vTaskDelay(pdMS_TO_TICKS(100));

        // Feed the dog only when every sanity flag was raised
        if (flag[0] && flag[1] && flag[2]) {
            flag[0] = flag[1] = flag[2] = 0;
            esp_task_wdt_reset();
        }
    }
}
