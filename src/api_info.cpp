#include "api_info.h"
#include <_preference.h>
#include <HTTPUpdate.h>
#include "_sntp.h"
#include "ConfigManager.cpp"

TaskHandle_t API_HANDLER;

String _api_host;
String _api_host_backup;
String _qweather_loc;
int _screen_index = 0;

int8_t _api_info_status = -1;

ApiInfo _api_info = {};
std::vector<Word> _daily_words;
Weather _weather = {};
Bilibili _bili = {};
Holiday _holiday = {};
SetValue _setValue = {};

extern ConfigManager cfg;

int8_t api_info_status() {
    return _api_info_status;
}

Weather* weather_data() {
    return &_weather;
}

Bilibili* bili_info() {
    return &_bili;
}

Holiday* holiday_info() {
    return &_holiday;
}

std::vector<Word> daily_words() {
    return _daily_words;
}

//当升级中，打印日志
void update_progress(int cur, int total) {
    Serial.printf("OTA:  HTTP update process at %d of %d bytes...\n", cur, total);
}

//当升级结束时，打印日志
void update_finished() {
    Serial.println("OTA:  HTTP update process finished");
    Serial.flush();  // ⭐ 强制输出
}

//当升级失败时，打印日志
void update_error(int err) {
    Serial.printf("OTA:  HTTP update fatal error code %d\n", err);
    Serial.flush();  // ⭐ 强制输出
}

void task_weather(void* param) {
    Serial.println("[Task] get weather begin...");

    API<> api;

    // 获取数据，根据当前小时重试不同的 host（每小时重试一次，优先主 host）
    bool success = false;
    Serial.println("Trying to fetch API info from: " + _api_host);
    success = api.getApiInfo(_api_info, _api_host, _qweather_loc, _screen_index);
    if (success) {
        Serial.println("Successfully fetched API info from: " + _api_host);
    } else if (_api_info.otaInfo.updateAvailable) {
        Serial.println("OTA update available, skipping data parsing.");
        String ota_url = _api_info.otaInfo.otaUrl;
        WiFiClient UpdateClient;
        httpUpdate.onEnd(update_finished);//当升级结束时
        httpUpdate.onProgress(update_progress);//当升级中
        httpUpdate.onError(update_error);//当升级失败时
        t_httpUpdate_return ret = httpUpdate.update(UpdateClient, ota_url);
        if (ret == HTTP_UPDATE_FAILED) {
            Serial.printf("OTA update failed. Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            Serial.flush();
        } else if (ret == HTTP_UPDATE_NO_UPDATES) {
            Serial.println("No OTA update available.");
            Serial.flush();
        } else if (ret == HTTP_UPDATE_OK) {
            Serial.println("OTA update successful, restarting...");
            Serial.flush();
        }
        _api_info_status = 2;
    } else if (_api_info.setValue.setValueAvailable) {
        Serial.println("SetValue received ...");
        _setValue = _api_info.setValue;
        if (_setValue.api_host.length() > 0) {
            _api_host = _setValue.api_host;
            cfg.set_api_host(_api_host);
            Serial.println("API host set to: " + _api_host);
        }
        if (_setValue.backup_host.length() > 0) {
            _api_host_backup = _setValue.backup_host;
            cfg.set_backup_host(_api_host_backup);
            Serial.println("Backup API host set to: " + _api_host_backup);
        }
        if (_setValue.qweather_loc.length() > 0) {
            _qweather_loc = _setValue.qweather_loc;
            cfg.set_qweather_loc(_qweather_loc);
            Serial.println("Qweather location set to: " + _qweather_loc);
        }
        if (_setValue.screen_index > 0) {
            cfg.set_screen_index(_setValue.screen_index);
            Serial.println("Screen index set to: " + String(_setValue.screen_index));
        }
        if (_setValue.wifi.length() > 0 && _setValue.password.length() > 0) {
            WiFi.begin(_setValue.wifi.c_str(), _setValue.password.c_str());
            Serial.println("Connecting to WiFi with new credentials from API..." + _setValue.wifi + " / " + _setValue.password);
        }
        _api_info_status = 2;
        Serial.println("SetValue processing completed, retrying API fetch with new settings...");
        success = api.getApiInfo(_api_info, _api_host, _qweather_loc, _screen_index);
    } else {
        Serial.println("Failed to fetch API info from: " + _api_host);
        success = api.getApiInfo(_api_info, _api_host_backup, _qweather_loc, _screen_index);
    }
    
    if (success) {
        _api_info_status = 1;
        _weather = _api_info.weather;
        _daily_words = _api_info.dailyWords;
        Serial.println("_daily_words size: " + String(_daily_words.size()));
        _bili = _api_info.bili;
        _holiday = _api_info.holiday;
    } else{
        _api_info_status = 2;
    }

    Serial.println("[Task] get weather end...");
    API_HANDLER = NULL;
    vTaskDelete(NULL);
}

void api_info_exec(int status) {
    _api_info_status = status;
    if (status > 0) {
        return;
    }

    if (!WiFi.isConnected()) {
        _api_info_status = 2;
        return;
    }

    Config c = cfg.get();

    _api_host = c.api_host;
    _api_host_backup = c.backup_host;
    _qweather_loc = c.qweather_loc;
    _screen_index = c.screen_index;

    if (_qweather_loc.length() == 0) {
        Serial.println("Qweather key/locationID invalid.");
        _api_info_status = 3;
    }

    if (API_HANDLER != NULL) {
        vTaskDelete(API_HANDLER);
        API_HANDLER = NULL;
    }
    xTaskCreate(task_weather, "WeatherData", 1024 * 8, NULL, 2, &API_HANDLER);
}

void api_info_stop() {
    if (API_HANDLER != NULL) {
        vTaskDelete(API_HANDLER);
        API_HANDLER = NULL;
    }
    _api_info_status = 2;
}

