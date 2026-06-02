/*
 * Listing 4 — Sanity Check (Multi-Task RTOS) Watchdog Timer (ESP-IDF)
 *
 * Four worker tasks each set a flag when they complete an iteration.
 * A dedicated monitor task (highest priority) wakes every second, checks
 * all flags, and feeds the TWDT only when every task has checked in.
 * Task1 deliberately hangs after HANG_AFTER_MS to demonstrate WDT recovery.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_log.h"

#define WDT_TIMEOUT_MS  5000
#define HANG_AFTER_MS   3000

static const char *TAG = "SanityRTOS";

static volatile int flag[4] = {0, 0, 0, 0};

static void vTask1(void *pvParameters)
{
    TickType_t start = xTaskGetTickCount();
    for (;;) {
        ESP_LOGI(TAG, "Task1");
        if ((xTaskGetTickCount() - start) >= pdMS_TO_TICKS(HANG_AFTER_MS)) {
            ESP_LOGW(TAG, "Task1 simulating hang!");
            while (1) { ; }
        }
        flag[0] = 1;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void vTask2(void *pvParameters)
{
    for (;;) {
        ESP_LOGI(TAG, "Task2");
        flag[1] = 1;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void vTask3(void *pvParameters)
{
    for (;;) {
        ESP_LOGI(TAG, "Task3");
        flag[2] = 1;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void vTask4(void *pvParameters)
{
    for (;;) {
        ESP_LOGI(TAG, "Task4");
        flag[3] = 1;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* Monitor task — subscribes itself to the TWDT and feeds it only when
 * all worker tasks have set their flags within the check window. */
static void vCheckFlagTask(void *pvParameters)
{
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (flag[0] && flag[1] && flag[2] && flag[3]) {
            flag[0] = flag[1] = flag[2] = flag[3] = 0;
            esp_task_wdt_reset();
            ESP_LOGI(TAG, "All tasks healthy — WDT fed.");
        } else {
            ESP_LOGW(TAG, "Sanity check failed — WDT not fed.");
        }
    }
}

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

    xTaskCreate(vTask1,         "Task1",      2048, NULL, 1, NULL);
    xTaskCreate(vTask2,         "Task2",      2048, NULL, 1, NULL);
    xTaskCreate(vTask3,         "Task3",      2048, NULL, 2, NULL);
    xTaskCreate(vTask4,         "Task4",      2048, NULL, 3, NULL);
    xTaskCreate(vCheckFlagTask, "CheckFlags", 2048, NULL, 4, NULL);
}
