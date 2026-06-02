/*
 * Listing 2 — Deadlock Watchdog Timer (ESP-IDF)
 *
 * Two tasks acquire mutexes in opposite order, causing a deadlock.
 * app_main is subscribed to the TWDT but never feeds it, so the
 * watchdog fires once the timeout expires and resets the system.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h"
#include "esp_log.h"

#define WDT_TIMEOUT_MS 3000

static const char *TAG = "DeadLockWDT";

static SemaphoreHandle_t mutex_1;
static SemaphoreHandle_t mutex_2;

/* Task A: acquires mutex_1 first, then mutex_2 */
static void task_a(void *parameters)
{
    while (1) {
        xSemaphoreTake(mutex_1, portMAX_DELAY);
        ESP_LOGI(TAG, "Task A took mutex 1");
        vTaskDelay(pdMS_TO_TICKS(1));   // yield briefly to force interleaving

        xSemaphoreTake(mutex_2, portMAX_DELAY);
        ESP_LOGI(TAG, "Task A took mutex 2");

        ESP_LOGI(TAG, "Task A doing some work");
        vTaskDelay(pdMS_TO_TICKS(500));

        xSemaphoreGive(mutex_2);
        xSemaphoreGive(mutex_1);

        ESP_LOGI(TAG, "Task A going to sleep");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* Task B: acquires mutex_2 first, then mutex_1 — deadlock with Task A */
static void task_b(void *parameters)
{
    while (1) {
        xSemaphoreTake(mutex_2, portMAX_DELAY);
        ESP_LOGI(TAG, "Task B took mutex 2");
        vTaskDelay(pdMS_TO_TICKS(1));

        xSemaphoreTake(mutex_1, portMAX_DELAY);
        ESP_LOGI(TAG, "Task B took mutex 1");

        ESP_LOGI(TAG, "Task B doing some work");
        vTaskDelay(pdMS_TO_TICKS(500));

        xSemaphoreGive(mutex_1);
        xSemaphoreGive(mutex_2);

        ESP_LOGI(TAG, "Task B going to sleep");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Deadlock WDT example started.");

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
    /* Subscribe app_main — it will never call esp_task_wdt_reset(),
     * so the WDT fires once the deadlock prevents any recovery. */
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    mutex_1 = xSemaphoreCreateMutex();
    mutex_2 = xSemaphoreCreateMutex();

    xTaskCreate(task_a, "TaskA", 2048, NULL, 2, NULL);
    xTaskCreate(task_b, "TaskB", 2048, NULL, 1, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
