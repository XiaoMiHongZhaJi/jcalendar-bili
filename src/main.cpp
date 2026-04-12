#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include "esp_sleep.h"
#include <wiring.h>
#include "battery.h"
#include "led.h"
#include "_sntp.h"
#include "api_info.h"
#include "screen_ink.h"
#include "version.h"
#include "OneButton.h"
#include "ConfigManager.cpp"

/* ---------------- 对象 ---------------- */
OneButton button(KEY_M, true);
WiFiManager wm;
volatile bool needOpenPortal = false;

/* ---------------- 参数 ---------------- */
WiFiManagerParameter para_api_host("api_host", "天气服务器Host", "", 64);
WiFiManagerParameter para_qweather_loc("qweather_loc", "位置ID", "", 64);

/* ---------------- 配置 ---------------- */
ConfigManager cfg;
const Config& c = cfg.get();

/* ---------------- 状态 ---------------- */
bool _wifi_flag = false;
unsigned long portal_idle_millis = 0;
unsigned long _wifi_failed_millis = 0;
unsigned long _screen_refersh_millis = 0;

/* ---------------- FreeRTOS 任务句柄 ---------------- */
TaskHandle_t buttonTaskHandle;
TaskHandle_t displayTaskHandle;

/* ---------------- 蜂鸣器 ---------------- */
typedef struct {
    uint16_t freq;
    uint16_t time;
} BeepCmd_t;

QueueHandle_t beepQueue;

void beepOnce(uint16_t freq, uint16_t time)
{
    BeepCmd_t cmd = {freq, time};
    xQueueSend(beepQueue, &cmd, 0);
}


/* ---------------- 唤醒原因 ---------------- */
void print_wakeup_reason() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    Serial.printf("Wakeup reason: %d\n", wakeup_reason);
}

/* ---------------- 业务函数声明 ---------------- */
void buttonClick(void* oneButton);
void buttonLongPressStop(void* oneButton);
void go_sleep(int sleep_seconds = 0);

/* ---------------- 保存参数 ---------------- */
void saveParamsCallback() {
    cfg.set_api_host(para_api_host.getValue());
    cfg.set_qweather_loc(para_qweather_loc.getValue());
    ESP.restart();
}

/* ======================= Tasks ======================= */

/* 按键任务 */
void buttonTask(void *pvParameters) {
    while (1) {
        button.tick();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* WiFi任务 */
void wifiTask(void *pvParameters) {

    // 设置配置页面：用 NVS/默认值作为初始值（如果存在临时配置也可以选择预填临时值）
    para_api_host.setValue(c.api_host.c_str(), 64);
    para_qweather_loc.setValue(c.qweather_loc.c_str(), 64);

    wm.setTitle("电子墨水屏设置");
    wm.addParameter(&para_api_host);
    wm.addParameter(&para_qweather_loc);
    std::vector<const char*> menu = {"wifi", "param", "update", "sep", "info", "restart", "exit"};
    wm.setMenu(menu); // custom menu, pass vector
    wm.setConfigPortalBlocking(false);
    wm.setBreakAfterConfig(true);
    wm.setSaveParamsCallback(saveParamsCallback);
    wm.setSaveConnect(false); // 保存完wifi信息后是否自动连接，设置为否，以便于用户继续配置param。
    wm.startConfigPortal("电子墨水屏设置");

    led_config(); // LED 进入三快闪状态

    portal_idle_millis = millis();

    while (1) {
        wm.process();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* 网络任务 */
void netTask(void *pvParameters) {
    while (1) {
        if (sntp_status() == -1) sntp_exec();
        if (api_info_status() == -1) api_info_exec();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* 屏幕任务 */
void displayTask(void *pvParameters) {
    while (1) {
        
        if (sntp_status() > 0 && api_info_status() > 0 && show_screen_status() == -1) {
            _screen_refersh_millis = millis();

            int screen_index = c.screen_index;

            esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

            if (cause == ESP_SLEEP_WAKEUP_EXT0) {
                screen_index++;
            } else if (cause == ESP_SLEEP_WAKEUP_TIMER) {
                if (screen_index == 1 || screen_index == 3 || screen_index == 5)
                    screen_index++;
                else
                    screen_index = 0;
            }

            if (screen_index > 6) screen_index = 0;

            cfg.set_screen_index(screen_index);
            show_screen_task(screen_index);
            _screen_refersh_millis = millis();
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* 电源管理任务 */
void powerTask(void *pvParameters) {
    while (1) {

        if (!wm.getConfigPortalActive() && show_screen_status() > 0 && !needOpenPortal) {

            int screen_index = c.screen_index;

            if (millis() - _screen_refersh_millis > IDLE_TO_SLEEP * 1000 || millis() - _wifi_failed_millis > IDLE_TO_SLEEP * 1000) {

                if (screen_index % 2 == 1) {
                    go_sleep(FLUSH_WORDS);
                    Serial.println("Sleep for flush words..." + String(FLUSH_WORDS) + "s");
                }
                else if (screen_index > 0 && screen_index % 2 == 0) {
                    go_sleep(FLUSH_CALENDAR);
                    Serial.println("Sleep for flush calendar..." + String(FLUSH_CALENDAR) + "s");
                }
                else {
                    Serial.println("Sleep for next hour...");
                    go_sleep();
                }
            }
        }

        if (wm.getConfigPortalActive() && millis() - portal_idle_millis > PORTAL_TIMEOUT * 1000) {
            Serial.println("Sleep for portal timeout..." + String(PORTAL_TIMEOUT) + "s");
            go_sleep();
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void beepTask(void *param)
{
    BeepCmd_t cmd;

    while (1)
    {
        if (xQueueReceive(beepQueue, &cmd, portMAX_DELAY) == pdPASS)
        {
            tone(BEEP_PIN, cmd.freq);
            vTaskDelay(pdMS_TO_TICKS(cmd.time));
            noTone(BEEP_PIN);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ---------------- 按键回调 ---------------- */
void buttonClick(void* oneButton) {
    Serial.printf("buttonClick\n");
    beepOnce(2000, 100);

    _screen_refersh_millis = millis();   // 防休眠
    _wifi_failed_millis = millis();      // 防休眠

    if (wm.getConfigPortalActive()) {
        ESP.restart();
    } else {
        set_screen_status(-1); // 重置显示状态
    }
}

void buttonLongPressStop(void* oneButton) {
    Serial.printf("buttonLongPressStop\n");
    beepOnce(2000, 1000);

    // if (wm.getConfigPortalActive()) {
    //     cfg.clearPersistent();
    //     ESP.restart();
    //     return;
    // }

    _screen_refersh_millis = millis();   // 防休眠
    _wifi_failed_millis = millis();      // 防休眠

    xTaskCreate(wifiTask, "wifiTask", 4096, NULL, 2, NULL);
}

/* ======================= 休眠 ======================= */

#define uS_TO_S_FACTOR 1000000

void go_sleep(int sleep_seconds) {

    time_t now;
    time(&now);
    struct tm local;
    localtime_r(&now, &local);

    int secondsToNextHour = sleep_seconds;

    if (sleep_seconds == 0) {
        secondsToNextHour = (60 - local.tm_min) * 60 - local.tm_sec;
        if (secondsToNextHour < 600) secondsToNextHour += 3600;
    }

    led_off();
    WiFi.mode(WIFI_OFF);

    esp_sleep_enable_ext0_wakeup(KEY_M, LOW);
    esp_sleep_enable_timer_wakeup(secondsToNextHour * (uint64_t)uS_TO_S_FACTOR);

    esp_deep_sleep_start();
}

/* ======================= setup ======================= */

void setup() {

    Serial.begin(115200);
    print_wakeup_reason();

    cfg.begin(); // 加载配置（会从 NVS 读取，如果没有则使用默认值）

    led_init();
    led_fast();

    beepQueue = xQueueCreate(2, sizeof(BeepCmd_t));
    beepOnce(2000, 100);

    /* 按键 */
    button.setClickMs(500);
    button.setPressMs(2000);
    button.attachClick(buttonClick, &button);
    button.attachLongPressStop(buttonLongPressStop, &button);

    /* WiFi */
    wm.setHostname("J-Calendar");
    wm.setEnableConfigPortal(false);
    wm.setConnectTimeout(10);
    wm.setSaveParamsCallback(saveParamsCallback);

    if (wm.autoConnect()) {
        _wifi_flag = true;
        led_on();
    } else {
        _wifi_failed_millis = millis();
        WiFi.mode(WIFI_OFF);
        led_slow();
    }

    /* 创建任务 */
    xTaskCreate(buttonTask, "buttonTask", 2048, NULL, 3, &buttonTaskHandle);
    xTaskCreate(netTask, "netTask", 4096, NULL, 2, NULL);
    xTaskCreate(displayTask, "displayTask", 4096, NULL, 2, &displayTaskHandle);
    xTaskCreate(powerTask, "powerTask", 4096, NULL, 1, NULL);
    xTaskCreate(ledTask, "ledTask", 1024, NULL, 1, NULL);
    xTaskCreate(beepTask, "beepTask", 1024, NULL, 1, NULL);
}

/* ======================= loop ======================= */

void loop() {
}