#include "api_info.h"
#include <_preference.h>

TaskHandle_t API_HANDLER;

String _qweather_host;
String _qweather_loc;

int8_t _api_info_status = -1;

ApiInfo _api_info = {};
std::vector<Word> _daily_words;
Weather _weather = {};

int8_t api_info_status() {
    return _api_info_status;
}

Weather* weather_data() {
    return &_weather;
}

std::vector<Word> daily_words() {
    return _daily_words;
}

void task_weather(void* param) {
    Serial.println("[Task] get weather begin...");

    API<> api;

    // 实时天气
    bool success = api.getApiInfo(_api_info, _qweather_host.c_str(), _qweather_loc.c_str());
    
    if (success) {
        _api_info_status = 1;
        _weather = _api_info.weather;
        _daily_words = _api_info.dailyWords;
        Serial.println("_daily_words size: " + String(_daily_words.size()));
    } else {
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

    // Preference 获取配置信息。
    Preferences pref;
    pref.begin(PREF_NAMESPACE);
    _qweather_host = pref.getString(PREF_QWEATHER_HOST, "default.host.com");
    _qweather_loc = pref.getString(PREF_QWEATHER_LOC, "");
    pref.end();

    if (_qweather_loc.length() == 0) {
        Serial.println("Qweather key/locationID invalid.");
        _api_info_status = 3;
        return;
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

