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

/*
  重构说明：
  - 增加 ConfigManager：集中管理可配置项，支持临时（RAM）与持久（NVS）写入。
  - 配置读取优先级：运行时临时配置 > NVS 中持久配置 > 内置默认值
  - config 中包含：qweather_host, qweather_loc, screen_index, TIME_TO_SLEEP（单位 ms）等
*/

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

/* ---------- 全局 ConfigManager 实例 ---------- */
ConfigManager cfg;

/* ---------------- WiFiManager 参数（保持原来的参数对象） ---------------- */
WiFiManager wm;
WiFiManagerParameter para_qweather_host("qweather_host", "天气服务器Host", "", 64);
WiFiManagerParameter para_qweather_location("qweather_loc", "位置ID", "", 64); // 城市code

/* ---------------- 全局状态/变量 ---------------- */
unsigned long _idle_millis = 0;
unsigned long TIME_TO_SLEEP = 180 * 1000; // 默认值，会从 cfg 覆盖
int screen_index = 0;

bool _wifi_flag = false;
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

        // 从 cfg 获取当前屏幕索引（优先临时配置）
        Config c = cfg.get();
        int current = c.screen_index;
        current++;
        // 如果需要立即持久保存当前选择，可调用
        cfg.setPersistent_screen_index(current); // 示例：把手动切换的界面保存为持久（你也可以选择 setTemp_screen_index）
        show_screen(current);
        _screen_refersh_millis = millis();
    }
}

/* ---------------- save params callback (配置页保存) ----------------
   说明：
   - 在这里我把通过配置页面保存视为“持久保存”（即写入 NVS 并立即生效）。
   - 如果你希望在配置界面里提供临时保存选项（仅 RAM），可以在 UI 上增加一个复选项并在此回调中按复选项调用 cfg.setTemp_qweather(...)。
*/
void saveParamsCallback() {
    String host = para_qweather_host.getValue();
    String loc = para_qweather_location.getValue();

    // 持久保存并立即生效
    cfg.setPersistent_qweather(host, loc);

    Serial.println("Params saved persistently.");

    _idle_millis = millis(); // 刷新无操作时间点

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
    Config c = cfg.get(); // 优先临时 -> 否则持久值
    para_qweather_host.setValue(c.qweather_host.c_str(), 64);
    para_qweather_location.setValue(c.qweather_loc.c_str(), 64);

    wm.setTitle("电子墨水屏设置");
    wm.addParameter(&para_qweather_host);
    wm.addParameter(&para_qweather_location);
    std::vector<const char*> menu = {"wifi", "param", "update", "sep", "info", "restart", "exit"};
    wm.setMenu(menu); // custom menu, pass vector
    wm.setConfigPortalBlocking(false);
    wm.setBreakAfterConfig(true);
    wm.setSaveParamsCallback(saveParamsCallback);
    wm.setSaveConnect(false); // 保存完wifi信息后是否自动连接，设置为否，以便于用户继续配置param。
    wm.startConfigPortal("电子墨水屏设置");

    led_config(); // LED 进入三快闪状态

    // 控制配置超时，记录无操作时间点
    _idle_millis = millis();
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
    cfg.begin(PREF_NAMESPACE);
    // 从持久配置覆盖默认运行时 TIME_TO_SLEEP
    Config initial = cfg.get();
    TIME_TO_SLEEP = initial.time_to_sleep_ms;

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
    para_qweather_host.setValue(initial.qweather_host.c_str(), 64);
    para_qweather_location.setValue(initial.qweather_loc.c_str(), 64);

    if (wm.autoConnect()) {
        Serial.println("Connect OK.");
        led_on();
        _wifi_flag = true;
    } else {
        Serial.println("Connect failed.");
        _wifi_flag = false;
        _wifi_failed_millis = millis();
        led_slow();
        _sntp_exec(2);
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
    if (_sntp_status() == -1) {
        _sntp_exec();
    }
    // 获取Weather信息
    if (api_info_status() == -1) {
        api_info_exec();
    }

    // 若 sntp & api 都完成，且屏幕待刷新，则刷新并计算下一步休眠逻辑
    if (_sntp_status() > 0 && api_info_status() > 0 && show_screen_status() == -1) {
        if (!wm.getConfigPortalActive()) {
            WiFi.mode(WIFI_OFF);
        }
        Serial.println("Wifi closed after data fetch.");

        // get effective config (临时优先)
        Config c = cfg.get();
        screen_index = c.screen_index;
        Serial.println("get screen_index: " + String(screen_index));
        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        if (cause == ESP_SLEEP_WAKEUP_EXT0) {
            // 按键唤醒
            Serial.println("按键唤醒 ESP_SLEEP_WAKEUP_EXT0 get screen_index: " + String(screen_index));
            screen_index ++;
        } else if (cause == ESP_SLEEP_WAKEUP_TIMER) {
            Serial.println("定时器唤醒 ESP_SLEEP_WAKEUP_TIMER get screen_index: " + String(screen_index));
            if (screen_index == 1 || screen_index == 3 || screen_index == 5) {
                // 如果是等待展示英文单词页面，则展示下一屏
                screen_index ++;
            } else {
                screen_index = 0;
            }
        }
        show_screen(screen_index);
        _screen_refersh_millis = millis();
    }

    // 休眠决策：当屏幕刷新完成（show_screen_status() > 0）
    if (!wm.getConfigPortalActive() && show_screen_status() > 0) {
        if (millis() - _screen_refersh_millis > 20 * 1000 || millis() - _wifi_failed_millis > 20 * 1000) {
            Serial.println("go_sleep screen_index = " + String(screen_index));
            if (screen_index == 1 || screen_index == 3 || screen_index == 5) {
                go_sleep(20);
            } else if (screen_index == 2 || screen_index == 4 || screen_index == 6) {
                go_sleep(60 * 3);
            } else {
                // go_sleep(60 * 10);
                go_sleep();
            }
        }
    }

    // 配置状态下超时休眠
    if (wm.getConfigPortalActive() && millis() - _idle_millis > TIME_TO_SLEEP) {
        Serial.println("配置状态下 go_sleep");
        go_sleep();
    }

    delay(10);
}

/* ---------------- go_sleep 实现（未作根本改变，仅使用 cfg 中的 sleep 配置） ---------------- */
void go_sleep(int sleep_seconds) {
    uint64_t p;
    // 读取本次生效的配置
    Config c = cfg.get();

    time_t now;
    time(&now);
    struct tm local;
    localtime_r(&now, &local);

    int secondsToNextHour = sleep_seconds;
    if (sleep_seconds == 0) {
        // 未指定，则根据配置计算时间（保持原逻辑）
        secondsToNextHour = (60 - local.tm_min) * 60 - local.tm_sec + 20;
        if (local.tm_hour == 23) {
            secondsToNextHour += 20;
        } else if (secondsToNextHour < 600) {
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
