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

OneButton button(KEY_M, true);

void IRAM_ATTR checkTicks() {
    button.tick();
}

void print_wakeup_reason() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
        Serial.println("Wakeup caused by external signal using RTC_IO");
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
    {
        Serial.println("Wakeup caused by external signal using RTC_CNTL");
        uint64_t status = esp_sleep_get_ext1_wakeup_status();
        if (status == 0) {
            Serial.println(" *None of the configured pins woke us up");
        } else {
            Serial.print(" *Wakeup pin mask: ");
            Serial.printf("0x%016llX\r\n", status);
            for (int i = 0; i < 64; i++) {
                if ((status >> i) & 0x1) {
                    Serial.printf("  - GPIO%d\r\n", i);
                }
            }
        }
        break;
    }
    case ESP_SLEEP_WAKEUP_TIMER:
        Serial.println("Wakeup caused by timer");
        break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        Serial.println("Wakeup caused by touchpad");
        break;
    case ESP_SLEEP_WAKEUP_ULP:
        Serial.println("Wakeup caused by ULP program");
        break;
    default:
        Serial.printf("Wakeup was not caused by deep sleep.\r\n");
    }
}

/* ---------------- WiFiManager 参数 ---------------- */
WiFiManager wm;
WiFiManagerParameter para_api_host("api_host", "天气服务器Host", "", 64);
WiFiManagerParameter para_qweather_loc("qweather_loc", "位置ID", "", 64);

/* ---------------- 全局状态/变量 ---------------- */
ConfigManager cfg;
const Config& c = cfg.get();

bool _wifi_flag = false;
unsigned long portal_idle_millis = 0;
unsigned long _wifi_failed_millis = 0;
unsigned long _screen_refersh_millis = 0;

void beepOnce(uint16_t beepFreq, uint16_t beepTime) {
  tone(BEEP_PIN, beepFreq);   // 发声
  delay(beepTime);
  noTone(BEEP_PIN);           // 停止
}

// 声音模式函数保留
void playBeepPattern(uint8_t beepCount, uint16_t pauseTime, uint16_t beepFreq, uint16_t beepTime) {
  for (uint8_t i = 0; i < beepCount; i++) {
    beepOnce(beepFreq, beepTime);
    delay(pauseTime);
  }
}

/* ---------------- 按键回调原型 ---------------- */
void buttonClick(void* oneButton);
void buttonLongPressStop(void* oneButton);
void go_sleep(int sleep_seconds = 0);

/* ---------------- 按键回调实现 ---------------- */
void buttonClick(void* oneButton) {
    Serial.println("Button click.");
    beepOnce(2000, 100);

    if (wm.getConfigPortalActive()) {
        Serial.println("In config status, restart to apply new settings.");
        ESP.restart();
    } else {
        Serial.println("Refresh screen manually by click.");

        // 从 cfg 获取当前屏幕索引
        int screen_index = c.screen_index;
        int words_page = c.words_page;
        Serial.println("按键按下 screen_index: " + String(screen_index));
        screen_index ++;
        if (screen_index < 0 || screen_index > words_page) {
            screen_index = 0;
        }
        cfg.set_screen_index(screen_index);
        show_screen();
        _screen_refersh_millis = millis();
    }
}

/* ---------------- save params callback (配置页保存) ----------------
   说明：
   - 在这里我把通过配置页面保存视为“持久保存”（即写入 NVS 并立即生效）。
   - 如果你希望在配置界面里提供临时保存选项（仅 RAM），可以在 UI 上增加一个复选项并在此回调中按复选项调用 cfg.setTemp_qweather(...)。
*/
void saveParamsCallback() {
    String api_host = para_api_host.getValue();
    String qweather_loc = para_qweather_loc.getValue();

    cfg.set_api_host(api_host);
    cfg.set_qweather_loc(qweather_loc);
    Serial.println("Params saved persistently.");
    ESP.restart();
}

/* ---------------- 长按打开配置或清除配置 ---------------- */
void buttonLongPressStop(void* oneButton) {
    Serial.println("Button long press.");
    beepOnce(2000, 500);

    if (wm.getConfigPortalActive()) {
        // 删除持久配置
        cfg.clearPersistent();
        Serial.println("Persistent config cleared.");
        ESP.restart();
        return;
    }

    if (api_info_status() == 0) {
        api_info_stop();
    }

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

    // 控制配置超时，记录无操作时间点
    portal_idle_millis = millis();
}

/* ---------------- setup / loop ---------------- */
#define uS_TO_S_FACTOR 1000000
#define TIMEOUT_TO_SLEEP  10 // seconds
time_t blankTime = 0;

void setup() {
    delay(10);
    Serial.begin(115200);
    Serial.println(".");
    print_wakeup_reason();
    Serial.println("\r\n\r\n");
    delay(10);

    Serial.printf("***********************\r\n");
    Serial.printf("      J-Calendar\r\n");
    Serial.printf("    version: %s\r\n", J_VERSION);
    Serial.printf("***********************\r\n\r\n");
    Serial.printf("Copyright © 2022-2025 JADE Software Co., Ltd. All Rights Reserved.\r\n\r\n");

    // 初始化 ConfigManager，加载持久配置
    cfg.begin();
    led_init();
    led_on();
    delay(100);

    button.setClickMs(500);
    button.setPressMs(2000); // 设置长按的时长
    button.attachClick(buttonClick, &button);
    button.attachLongPressStop(buttonLongPressStop, &button);
    attachInterrupt(digitalPinToInterrupt(KEY_M), checkTicks, CHANGE);
    
    int voltage = readBatteryVoltage();
    Serial.printf("Battery: %d mV\r\n", voltage);
    if(voltage < 3000) {
        Serial.println("[INFO]电池损坏或无ADC电路。");
    } else if(voltage < 3550) {
        Serial.println("[WARN]电量低于3.5v，系统休眠。");
        go_sleep(7 * 24 * 60 * 60); // 7天后唤醒
    } else if (voltage < 3600) {
        Serial.println("[WARN]电量低于3.6v，警告并系统休眠。");
        go_sleep(24 * 60 * 60); // 1天后唤醒
    } else if (voltage > 4400) {
        Serial.println("[INFO]未接电池。");
    }

    Serial.println("Wm begin...");
    led_fast();
    beepOnce(2000, 100);
    wm.setHostname("J-Calendar");
    wm.setEnableConfigPortal(false);
    wm.setConnectTimeout(10);

    // 将配置中 qweather 值预填到 wm 参数（方便 config portal 展示）
    para_api_host.setValue(c.api_host.c_str(), 64);
    para_qweather_loc.setValue(c.qweather_loc.c_str(), 64);

    if (wm.autoConnect()) {
        Serial.println("Connect OK.");
        led_on();
        _wifi_flag = true;
    } else {
        Serial.println("Connect failed.");
        _wifi_flag = false;
        _wifi_failed_millis = millis();
        led_slow();
        sntp_exec(2);
        api_info_exec(2);
        WiFi.mode(WIFI_OFF); // 提前关闭WIFI，省电
        Serial.println("Wifi closed.");
    }
}

/**
 * loop() 任务说明已保留：sntp、天气、刷新日历、休眠管理、配置超时管理
 */
void loop() {
    button.tick(); // 单/双/长按逻辑
    wm.process();

    // sntp 同步（如果需要）
    if (sntp_status() == -1) {
        sntp_exec();
    }
    // 获取Weather信息
    if (api_info_status() == -1) {
        api_info_exec();
    }

    // 若 sntp & api 都完成，且屏幕待刷新，则刷新并计算下一步休眠逻辑
    // -1: 初始化 0: 显示中 1: 显示成功 2: 显示失败
    if (sntp_status() > 0 && api_info_status() > 0 && show_screen_status() == -1) {
        if (!wm.getConfigPortalActive()) {
            WiFi.mode(WIFI_OFF);
        }
        Serial.println("Wifi closed after data fetch.");

        int screen_index = c.screen_index;
        int words_page = c.words_page;
        Serial.println("get screen_index: " + String(screen_index));
        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        if (cause == ESP_SLEEP_WAKEUP_EXT0) {
            // 按键唤醒
            Serial.println("按键唤醒 ESP_SLEEP_WAKEUP_EXT0 get screen_index: " + String(screen_index));
            screen_index ++;
        } else if (cause == ESP_SLEEP_WAKEUP_TIMER) {
            // 定时器唤醒
            Serial.println("定时器唤醒 ESP_SLEEP_WAKEUP_TIMER get screen_index: " + String(screen_index));
            if (screen_index % 2 == 1) {
                // 如果是等待展示英文单词页面，则展示下一屏
                screen_index ++;
            } else {
                screen_index = 0;
            }
        }
        if (screen_index < 0 || screen_index > words_page) {
            screen_index = 0;
        }
        cfg.set_screen_index(screen_index);
        show_screen();
        _screen_refersh_millis = millis();
    }

    // 休眠决策：当屏幕刷新完成
    // -1: 初始化 0: 显示中 1: 显示成功 2: 显示失败
    if (!wm.getConfigPortalActive() && show_screen_status() > 0) {
        if (millis() - _screen_refersh_millis > IDLE_TO_SLEEP * 1000 || millis() - _wifi_failed_millis > IDLE_TO_SLEEP * 1000) {
            int screen_index = c.screen_index;
            if (screen_index % 2 == 1) {
                go_sleep(FLUSH_WORDS);
            } else if (screen_index > 0 && screen_index % 2 == 0) {
                go_sleep(FLUSH_CALENDAR);
            } else {
                go_sleep();
            }
        }
    }

    // 配置状态下超时休眠
    if (wm.getConfigPortalActive() && millis() - portal_idle_millis > PORTAL_TIMEOUT * 1000) {
        Serial.println("配置状态下超时 go_sleep");
        go_sleep();
    }

    delay(10);
}

/* ---------------- go_sleep 实现（未作根本改变，仅使用 cfg 中的 sleep 配置） ---------------- */
void go_sleep(int sleep_seconds) {
    time_t now;
    time(&now);
    struct tm local;
    localtime_r(&now, &local);

    int secondsToNextHour = sleep_seconds;
    if (sleep_seconds == 0) {
        // 未指定唤醒时间，则根据配置计算时间（保持原逻辑）
        secondsToNextHour = (60 - local.tm_min) * 60 - local.tm_sec;
        if (local.tm_hour == 23) {
            secondsToNextHour += 30;
        } else if (secondsToNextHour < 600) {
            // 50分之后，下个整点不再唤醒，下下个整点才唤醒
            secondsToNextHour += 3600;
        }
    }
    Serial.printf("Battery voltage: %d \n", readBatteryVoltage());
    Serial.printf("Seconds to next even hour: %d seconds.\n", secondsToNextHour);
    pinMode(PIN_LED_R, INPUT); // Off 
    delay(100);
    esp_sleep_enable_ext0_wakeup(KEY_M, LOW);
    esp_sleep_enable_timer_wakeup(secondsToNextHour * (uint64_t)uS_TO_S_FACTOR);
    Serial.println("Deep sleep now...");
    esp_deep_sleep_start();
}
