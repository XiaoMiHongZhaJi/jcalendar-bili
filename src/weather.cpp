#include "weather.h"
#include <_preference.h>

TaskHandle_t WEATHER_HANDLER;

String _qweather_host;
String _qweather_loc;

int8_t _weather_status = -1;
Weather _weather_now = {};

int8_t weather_status() {
    return _weather_status;
}
Weather* weather_data_now() {
    return &_weather_now;
}

void task_weather(void* param) {
    Serial.println("[Task] get weather begin...");

    API<> api;

    // 实时天气
    bool success = api.getWeatherNow(_weather_now, _qweather_host.c_str(), _qweather_loc.c_str());
    
    if (success) {
        _weather_status = 1;
    } else {
        _weather_status = 2;
    }

    Serial.println("[Task] get weather end...");
    WEATHER_HANDLER = NULL;
    vTaskDelete(NULL);
}

void weather_exec(int status) {
    _weather_status = status;
    if (status > 0) {
        return;
    }

    if (!WiFi.isConnected()) {
        _weather_status = 2;
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
        _weather_status = 3;
        return;
    }

    if (WEATHER_HANDLER != NULL) {
        vTaskDelete(WEATHER_HANDLER);
        WEATHER_HANDLER = NULL;
    }
    xTaskCreate(task_weather, "WeatherData", 1024 * 8, NULL, 2, &WEATHER_HANDLER);
}

void weather_stop() {
    if (WEATHER_HANDLER != NULL) {
        vTaskDelete(WEATHER_HANDLER);
        WEATHER_HANDLER = NULL;
    }
    _weather_status = 2;
}

