/*
 * Listing 1 — Basic Watchdog Timer (ESP-IDF)
 *
 * Subscribes app_main to the Task WDT, feeds it for 10 iterations,
 * then deliberately hangs to trigger a WDT reset.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_log.h"

#define WDT_TIMEOUT_MS 3000

static const char *TAG = "BasicWDT";

void app_main(void)
{
    ESP_LOGI(TAG, "Setup started.");
    vTaskDelay(pdMS_TO_TICKS(2000));

    esp_task_wdt_config_t wdt_config = {
        .timeout_ms    = WDT_TIMEOUT_MS,
        .idle_core_mask = 0,
        .trigger_panic  = true,
    };
    /* If the TWDT was already initialized by IDF (CONFIG_ESP_TASK_WDT_INIT=y),
     * esp_task_wdt_init() returns ESP_ERR_INVALID_STATE — use reconfigure instead. */
    esp_err_t ret = esp_task_wdt_init(&wdt_config);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(esp_task_wdt_reconfigure(&wdt_config));
    } else {
        ESP_ERROR_CHECK(ret);
    }
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));  // Subscribe app_main to the TWDT

    ESP_LOGI(TAG, "LOOP started!");
    for (int i = 0; i <= 10; i++) {
        ESP_LOGI(TAG, "Task: %d", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_task_wdt_reset();  // Kick the dog
    }

    // Hang — WDT will fire after WDT_TIMEOUT_MS and trigger a panic/reset
    while (1) {
        ESP_LOGI(TAG, "MCU hang event!!!");
    }
}
