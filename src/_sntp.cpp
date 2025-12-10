#include "_sntp.h"
#include <WiFi.h>
#include "esp_sntp.h"
#include "Arduino.h"
#include <_preference.h>
#include <API.hpp>
#include "holiday.h"

TaskHandle_t* _handler;
int _status = SYNC_STATUS_IDLE;

int _sntp_status() {
    return _status;
}

// SNTP 校准时间的任务
void _sntp_task(void* pvParameter) {
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "ntp.aliyun.com");
    // 设置时区
    setenv("TZ", "CST-8", 1);
    sntp_init();

    unsigned long begin_millis = millis();
    int c = 1;
    int re;
    while (1) {
        if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            Serial.println("SNTP OK.");
            re = SYNC_STATUS_OK;
            break; // 获取时间成功，退出循环
        }

        if (millis() - begin_millis > 10 * 1000) { // 超时10s
            Serial.println("SNTP timeout.");
            re = SYNC_STATUS_NOK;
            break;
        }
        c++;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    time_t now = time(NULL);
    struct tm tmInfo = { 0 };
    localtime_r(&now, &tmInfo);
    Serial.printf("Now: %d-%02d-%02d %02d:%02d:%02d\r\n", tmInfo.tm_year + 1900, tmInfo.tm_mon + 1, tmInfo.tm_mday, tmInfo.tm_hour, tmInfo.tm_min, tmInfo.tm_sec);
    
    // 获取节假日信息并缓存
    Holiday _holiday;
        Preferences pref;
        pref.begin(PREF_NAMESPACE);
        size_t holiday_size = pref.getBytesLength(PREF_HOLIDAY);
        if (holiday_size > 0) {
            pref.getBytes(PREF_HOLIDAY, &_holiday, holiday_size);
        }
        pref.end();
    
        if (_holiday.year != tmInfo.tm_year + 1900 || _holiday.month != tmInfo.tm_mon + 1) {
            if (getHolidays(_holiday, tmInfo.tm_year + 1900, tmInfo.tm_mon + 1)) {
                pref.begin(PREF_NAMESPACE);
                pref.putBytes(PREF_HOLIDAY, &_holiday, sizeof(_holiday));
                pref.end();
            }
        }

    _status = re;
    vTaskDelete(NULL); // 删除任务
}

void _sntp_exec(int status) {
    _status = status;
    if (_status > 0) {
        return;
    }
    if (!WiFi.isConnected()) {
        _status = SYNC_STATUS_NOK;
    }

    _status = SYNC_STATUS_IN_PROGRESS;
    // 创建一个SNTP 校准时间的任务
    xTaskCreate(&_sntp_task, "_sntp_task", 1024 * 8, NULL, 6, _handler);
}