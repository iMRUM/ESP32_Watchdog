/*
 * Listing 5 — The Great Watchdog Timer (ESP-IDF)
 *
 * A hardware general-purpose timer fires every TIMER_ALARM_S seconds from an
 * ISR. Inside the ISR, sanity flags set by four worker tasks are checked:
 *   - All flags set  → clear flags, system continues (timer auto-reloads)
 *   - Any flag missing → esp_restart() to recover
 *
 * Arduino API mapping:
 *   timerBegin(0, 80, true)          → gptimer_new_timer() at 1 MHz
 *   timerAttachInterrupt(&onTimer)   → gptimer_register_event_callbacks()
 *   timerAlarmWrite(timer, N, true)  → gptimer_set_alarm_action() with auto-reload
 *   timerAlarmEnable(timer)          → gptimer_enable() + gptimer_start()
 *   timerWrite(timer, 0)             → handled by auto_reload_on_alarm = true
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_system.h"

#define TIMER_RESOLUTION_HZ  1000000UL   // 1 MHz → 1 tick = 1 µs
#define TIMER_ALARM_S        3
#define TIMER_ALARM_COUNT    (TIMER_ALARM_S * TIMER_RESOLUTION_HZ)  // 3 000 000 ticks
#define HANG_AFTER_MS        3000

static const char *TAG = "GreatWDT";

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile int flag[4] = {0, 0, 0, 0};

/* Hardware timer ISR — runs every TIMER_ALARM_S seconds.
 * auto_reload_on_alarm keeps the timer running without manual reset. */
static bool IRAM_ATTR on_timer_alarm(gptimer_handle_t timer,
                                     const gptimer_alarm_event_data_t *edata,
                                     void *user_ctx)
{
    portENTER_CRITICAL_ISR(&s_mux);
    int all_set = flag[0] && flag[1] && flag[2] && flag[3];
    if (all_set) {
        flag[0] = flag[1] = flag[2] = flag[3] = 0;
        // Sanity check passed — timer auto-reloads, system continues
    }
    portEXIT_CRITICAL_ISR(&s_mux);

    if (!all_set) {
        esp_restart();  // Sanity check failed — force system reset
    }
    return false;  // No higher-priority task woken
}

static void vTask1(void *pvParameters)
{
    TickType_t start = xTaskGetTickCount();
    for (;;) {
        ESP_LOGI(TAG, "Task1");
        if ((xTaskGetTickCount() - start) >= pdMS_TO_TICKS(HANG_AFTER_MS)) {
            ESP_LOGW(TAG, "Task1 simulating hang!");
            while (1) { ; }
        }
        portENTER_CRITICAL(&s_mux);
        flag[0] = 1;
        portEXIT_CRITICAL(&s_mux);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void vTask2(void *pvParameters)
{
    for (;;) {
        ESP_LOGI(TAG, "Task2");
        portENTER_CRITICAL(&s_mux);
        flag[1] = 1;
        portEXIT_CRITICAL(&s_mux);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void vTask3(void *pvParameters)
{
    for (;;) {
        ESP_LOGI(TAG, "Task3");
        portENTER_CRITICAL(&s_mux);
        flag[2] = 1;
        portEXIT_CRITICAL(&s_mux);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void vTask4(void *pvParameters)
{
    for (;;) {
        ESP_LOGI(TAG, "Task4");
        portENTER_CRITICAL(&s_mux);
        flag[3] = 1;
        portEXIT_CRITICAL(&s_mux);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    // Configure hardware timer at 1 MHz resolution
    gptimer_handle_t gptimer = NULL;
    gptimer_config_t timer_config = {
        .clk_src       = GPTIMER_CLK_SRC_DEFAULT,
        .direction     = GPTIMER_COUNT_UP,
        .resolution_hz = TIMER_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    // Register ISR callback
    gptimer_event_callbacks_t cbs = {
        .on_alarm = on_timer_alarm,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));

    // Alarm fires every TIMER_ALARM_S seconds and auto-reloads from 0
    gptimer_alarm_config_t alarm_config = {
        .alarm_count              = TIMER_ALARM_COUNT,
        .reload_count             = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    xTaskCreate(vTask1, "Task1", 2048, NULL, 1, NULL);
    xTaskCreate(vTask2, "Task2", 2048, NULL, 1, NULL);
    xTaskCreate(vTask3, "Task3", 2048, NULL, 2, NULL);
    xTaskCreate(vTask4, "Task4", 2048, NULL, 3, NULL);
}
