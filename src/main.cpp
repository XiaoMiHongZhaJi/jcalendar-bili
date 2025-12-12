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
#include "_preference.h"
#include "version.h"
#include "OneButton.h"

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

void buttonClick(void* oneButton);
void buttonLongPressStop(void* oneButton);
void go_sleep(int sleep_seconds = 0);

unsigned long _idle_millis;
unsigned long TIME_TO_SLEEP = 180 * 1000;
int screen_index = 0;

bool _wifi_flag = false;
unsigned long _wifi_failed_millis;
unsigned long _screen_refersh_millis;

WiFiManager wm;
WiFiManagerParameter para_qweather_host("qweather_host", "天气服务器Host", "", 64);
WiFiManagerParameter para_qweather_location("qweather_loc", "位置ID", "", 64); //     城市code

void beepOnce(uint16_t beepFreq, uint16_t beepTime) {
  tone(BEEP_PIN, beepFreq);   // 发声
  delay(beepTime);
  noTone(BEEP_PIN);           // 停止
}


// 单击
void buttonClick(void* oneButton) {
    Serial.println("Button click.");
    beepOnce(2000, 100);

    if (wm.getConfigPortalActive()) {
        Serial.println("In config status, restart to apply new settings.");
        ESP.restart();
    } else {
        Serial.println("Refresh screen manually by click.");

        Preferences pref;
        pref.begin(PREF_NAMESPACE);
        int screen_index = pref.getInt(PREF_SI_TYPE);
        pref.end();
        
        ++screen_index;
        show_screen(screen_index);
        _screen_refersh_millis = millis();
    }
}

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

    led_init();
    led_on();
    delay(100);
    int voltage = readBatteryVoltage();
    Serial.printf("Battery: %d mV\r\n", voltage);
    if(voltage < 2500) {
        Serial.println("[INFO]电池损坏或无ADC电路。");
    } else if(voltage < 3000) {
        Serial.println("[WARN]电量低于3v，系统休眠。");
        go_sleep(7 * 24 * 60 * 60); // 一星期后唤醒
    } else if (voltage < 3300) {
        // 低于3.3v，电池电量用尽，屏幕给警告，然后关机。 
        Serial.println("[WARN]电量低于3.3v，警告并系统休眠。");
        si_warning("电量不足，请充电！");
        go_sleep();
    } else if (voltage > 4400) {
        Serial.println("[INFO]未接电池。");
    }

    button.setClickMs(300);
    button.setPressMs(3000); // 设置长按的时长
    button.attachClick(buttonClick, &button);
    button.attachLongPressStop(buttonLongPressStop, &button);
    attachInterrupt(digitalPinToInterrupt(KEY_M), checkTicks, CHANGE);

    Serial.println("Wm begin...");
    led_fast();
    beepOnce(2000, 100);
    wm.setHostname("J-Calendar");
    wm.setEnableConfigPortal(false);
    wm.setConnectTimeout(10);
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
 * 处理各个任务
 * 1. sntp同步
 *      前置条件：Wifi已连接
 * 2. 刷新日历
 *      前置条件：sntp同步完成（无论成功或失败）
 * 3. 刷新天气信息
 *      前置条件：wifi已连接
 * 4. 系统配置
 *      前置条件：无
 * 5. 休眠
 *      前置条件：所有任务都完成或失败，
 */
void loop() {
    button.tick(); // 单击，刷新页面；双击，打开配置；长按，重启
    wm.process();
    // 前置任务：wifi已连接
    // sntp同步
    if (_sntp_status() == -1) {
        _sntp_exec();
    }
    // 前置任务：wifi已连接
    // 获取Weather信息
    if (api_info_status() == -1) {
        api_info_exec();
    }

    // 刷新日历
    // 前置任务：sntp、weather
    // 执行条件：屏幕状态为待处理
    if (_sntp_status() > 0 && api_info_status() > 0 && show_screen_status() == -1) {
        // 数据获取完毕后，关闭Wifi，省电
        if (!wm.getConfigPortalActive()) {
            WiFi.mode(WIFI_OFF);
        }
        Serial.println("Wifi closed after data fetch.");

        Preferences pref;
        pref.begin(PREF_NAMESPACE);
        screen_index = pref.getInt(PREF_SI_TYPE);
        pref.end();
        Serial.println("get screen_index: " + String(screen_index));
        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        if (cause == ESP_SLEEP_WAKEUP_EXT0) {
            // 按键唤醒
            screen_index ++;
        } else if (cause == ESP_SLEEP_WAKEUP_TIMER) {
            // 定时器唤醒
            if (screen_index == 1 || screen_index == 3 || screen_index == 5) {
                // 如果是等待展示英文单词页面，则展示下一屏
                screen_index ++;
            } else {
                // 否则展示首页
                screen_index = 0;
            }
        }
        show_screen(screen_index);
        _screen_refersh_millis = millis();
    }

    // 休眠
    // 前置条件：屏幕刷新完成（或成功）

    // 未在配置状态，且屏幕刷新完成，进入休眠
    if (!wm.getConfigPortalActive() && show_screen_status() > 0) {
        if (millis() - _screen_refersh_millis > 10 * 1000 || millis() - _wifi_failed_millis > 10 * 1000) {   
            if (screen_index == 1 || screen_index == 3 || screen_index == 5) {
                Serial.println("screen_index 1 go_sleep");
                // 如果是展示中文单词页面，则休眠时间很短
                go_sleep(20);
            } else if (screen_index == 2 || screen_index == 4 || screen_index == 6) {
                Serial.println("screen_index 2 go_sleep");
                // 如果是展示中英文单词页面，则休眠时间较短
                go_sleep(60 * 3);
            } else {
                Serial.println("screen_index else go_sleep");
                go_sleep();
            }
        }
    }
    // 配置状态下，
    if (wm.getConfigPortalActive() && millis() - _idle_millis > TIME_TO_SLEEP) {
        Serial.println("配置状态下 go_sleep");
        go_sleep();
    }

    delay(10);
}

// 蜂鸣器 提示音次数, 间隔(毫秒), 频率(Hz), 持续时间(毫秒)
void playBeepPattern(uint8_t beepCount, uint16_t pauseTime, uint16_t beepFreq, uint16_t beepTime) {
  for (uint8_t i = 0; i < beepCount; i++) {
    beepOnce(beepFreq, beepTime);
    delay(pauseTime);
  }
}

void saveParamsCallback() {
    Preferences pref;
    pref.begin(PREF_NAMESPACE);
    pref.putString(PREF_QWEATHER_HOST, para_qweather_host.getValue());
    pref.putString(PREF_QWEATHER_LOC, para_qweather_location.getValue());
    pref.end();

    Serial.println("Params saved.");

    _idle_millis = millis(); // 刷新无操作时间点

    ESP.restart();
}

// 长按打开配置页面，配置页面再次长按则清除配置并重启
void buttonLongPressStop(void* oneButton) {
    Serial.println("Button long press.");
    beepOnce(2000, 500);

    if (wm.getConfigPortalActive()) {
        // 删除Preferences，namespace下所有健值对。
        Preferences pref;
        pref.begin(PREF_NAMESPACE);
        pref.clear();
        pref.end();

        ESP.restart();
        return;
    }

    if (api_info_status() == 0) {
        api_info_stop();
    }

    // 设置配置页面
    // 根据配置信息设置默认值
    Preferences pref;
    pref.begin(PREF_NAMESPACE);
    String qHost = pref.getString(PREF_QWEATHER_HOST, "192.168.10.225:5000");
    String qLoc = pref.getString(PREF_QWEATHER_LOC, "101021600");
    pref.end();

    para_qweather_host.setValue(qHost.c_str(), 64);
    para_qweather_location.setValue(qLoc.c_str(), 64);

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

    // 控制配置超时180秒后休眠
    _idle_millis = millis();
}

#define uS_TO_S_FACTOR 1000000
#define TIMEOUT_TO_SLEEP  10 // seconds
time_t blankTime = 0;
void go_sleep(int sleep_seconds) {
    uint64_t p;
    // 根据配置情况来刷新，如果未配置qweather信息，则24小时刷新，否则每2小时刷新

    time_t now;
    time(&now);
    struct tm local;
    localtime_r(&now, &local);
    // 指定唤醒时间
    int secondsToNextHour = sleep_seconds;
    if (sleep_seconds == 0) {
        // 未指定，则根据配置计算时间
        secondsToNextHour = (60 - local.tm_min) * 60 - local.tm_sec + 40;
        if (local.tm_hour == 23) {
            secondsToNextHour += 20;
        } else if (secondsToNextHour < 600) {
            secondsToNextHour += 3600;
        }
    }
    Serial.printf("Seconds to next even hour: %d seconds.\n", secondsToNextHour);
    pinMode(PIN_LED_R, INPUT); // Off 
    delay(100);
    Serial.println("Deep sleep...");
    Serial.flush();
    esp_sleep_enable_ext0_wakeup(KEY_M, LOW);
    esp_sleep_enable_timer_wakeup(secondsToNextHour * (uint64_t)uS_TO_S_FACTOR);
    Serial.println("Deep sleep now...");
    esp_deep_sleep_start();
}