#ifndef __API_HPP__
#define __API_HPP__

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_http_client.h>
#include "battery.h"

struct Weather {
    String time;
    String temp;
    String humidity;
    String windDir;
    String windScale;
    String windSpeed;
    String icon;
    String text;
    String updateTime;
    std::vector<String> futureTags;
};

// 定义单词结构体
struct Word {
    String ch;
    String en;
};

struct Bilibili {
    String userName;       // 昵称

    String follower;       // 粉丝数
    String allView;        // 总播放量
    String allLikes;       // 总点赞数

    String addFollower;   // 新增粉丝数
    String addAllView;    // 新增总播放量
    String addAllLikes;   // 新增总点赞数

    String comment;       // 新视频评论数
    String view;          // 新视频播放量
    String likes;         // 新视频点赞数

    String addComment;    // 新视频新增评论数
    String addView;       // 新视频新增播放量
    String addLikes;      // 新视频新增点赞数

    String videoDeltaDays; // 上次视频距今
    String liveDeltaDays;  // 上次直播距今

    std::vector<String> videoDateTags; // 视频发布日期tags
    std::vector<String> liveDateTags;  // 直播日期tags
};

struct Holiday {
    int year;
    int month;
    int holidays[16];
    int length;
};

struct OTAInfo {
    bool updateAvailable;
    String otaUrl;
};

struct SetValue {
    bool setValueAvailable;
    String api_host;
    String backup_host;
    String qweather_loc;
    int screen_index;
    int words_page;
    String wifi;
    String password;
};

// 定义返回数据结构体

struct ApiInfo {
    Weather weather;
    std::vector<Word> dailyWords;
    Bilibili bili;
    Holiday holiday;
    OTAInfo otaInfo;
    SetValue setValue;
};

template<uint8_t MAX_RETRY = 3>
class API {
    using callback = std::function<bool(JsonDocument&)>;
    using precall = std::function<void()>;

public:
    API() {
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        const char* encoding = "Content-Encoding";
        const char* headerKeys[1] = {};
        headerKeys[0] = encoding;
        http.collectHeaders(headerKeys, 1);
    }

    ~API() {}

    // 获取 HTTPClient
    HTTPClient& httpClient() {
        return http;
    }

    // 和风天气 - 实时天气: https://dev.qweather.com/docs/api/weather/weather-now/
    bool getApiInfo(ApiInfo& apiInfo, String host, String locid, int screen_index) {
        return getRestfulAPI(
            "http://" + host + "/apiInfo?location=" + locid + "&battery=" + getBatteryVoltage() + "&screen_index=" + screen_index, [&apiInfo](JsonDocument& json) {
                if (strcmp(json["code"], "200") != 0) {
                    Serial.println("Get weather failed, error: ");
                    Serial.println(json["code"].as<const char*>());
                    return false;
                }

                if (json["ota_update"].as<bool>()) {
                    Serial.println("OTA update available, skipping data parsing.");
                    OTAInfo ota;
                    ota.updateAvailable = true;
                    ota.otaUrl = json["ota_url"].as<const char*>();
                    apiInfo.otaInfo = ota;
                    return false;
                }

                JsonObject setValue = json["setValue"];
                if (!setValue.isNull()) {
                    SetValue sv;
                    sv.setValueAvailable = true;
                    sv.api_host = setValue["api_host"].as<const char*>();
                    sv.backup_host = setValue["backup_host"].as<const char*>();
                    sv.qweather_loc = setValue["qweather_loc"].as<const char*>();
                    sv.wifi = setValue["wifi"].as<const char*>();
                    sv.password = setValue["password"].as<const char*>();
                    sv.screen_index = setValue["screen_index"].as<int>();
                    sv.words_page = setValue["words_page"].as<int>();
                    apiInfo.setValue = sv;
                    return false;
                }

                JsonObject weather = json["weather"];
                if (!weather.isNull()) {
                    Weather weatherResult;
                    weatherResult.updateTime = weather["updateTime"].as<const char*>();
                    weatherResult.time = weather["obsTime"].as<const char*>();
                    weatherResult.temp = weather["temp"].as<const char*>();
                    weatherResult.humidity = weather["humidity"].as<const char*>();
                    weatherResult.windDir = weather["windDir"].as<const char*>();
                    weatherResult.windScale = weather["windScale"].as<const char*>();
                    weatherResult.windSpeed = weather["windSpeed"].as<const char*>();
                    weatherResult.icon = weather["icon"].as<const char*>();
                    weatherResult.text = weather["text"].as<const char*>();

                    JsonArray weatherFuture = weather["future"];
                    std::vector<String> weatherFutureTags;
                    for (const char* tag : weatherFuture) {
                        weatherFutureTags.emplace_back(tag);
                    }
                    weatherResult.futureTags = weatherFutureTags;
                    
                    apiInfo.weather = weatherResult;
                }

                JsonArray dailyWords = json["dailyWords"];
                if (!dailyWords.isNull()) {
                    std::vector<Word> dailyWordsResult;
                    // 解析 words 数组
                    for (JsonObject wordObj : dailyWords) {
                        Word w;
                        w.ch = wordObj["ch"].as<const char*>();
                        w.en = wordObj["en"].as<const char*>();
                        dailyWordsResult.push_back(w);
                    }
                    apiInfo.dailyWords = dailyWordsResult;
                }

                JsonObject bili = json["biliInfo"];
                if (!bili.isNull()) {
                    Bilibili biliResult;
                    biliResult.userName = bili["userName"].as<const char*>();
                    biliResult.follower = bili["follower"].as<const char*>();
                    biliResult.allView = bili["allView"].as<const char*>();
                    biliResult.allLikes = bili["allLikes"].as<const char*>();
                    biliResult.addFollower = bili["addFollower"].as<const char*>();
                    biliResult.addAllView = bili["addAllView"].as<const char*>();
                    biliResult.addAllLikes = bili["addAllLikes"].as<const char*>();
                    biliResult.comment = bili["comment"].as<const char*>();
                    biliResult.view = bili["view"].as<const char*>();
                    biliResult.likes = bili["likes"].as<const char*>();
                    biliResult.addComment = bili["addComment"].as<const char*>();
                    biliResult.addView = bili["addView"].as<const char*>();
                    biliResult.addLikes = bili["addLikes"].as<const char*>();
                    biliResult.videoDeltaDays = bili["videoDeltaDays"].as<const char*>();
                    biliResult.liveDeltaDays = bili["liveDeltaDays"].as<const char*>();

                    JsonArray videoDateTags = bili["videoDateTags"];
                    std::vector<String> videoDateTagsResult;
                    for (const char* tag : videoDateTags) {
                        videoDateTagsResult.emplace_back(tag);
                    }
                    biliResult.videoDateTags = videoDateTagsResult;

                    JsonArray liveDateTags = bili["liveDateTags"];
                    std::vector<String> liveDateTagsResult;
                    for (const char* tag : liveDateTags) {
                        liveDateTagsResult.emplace_back(tag);
                    }
                    biliResult.liveDateTags = liveDateTagsResult;

                    apiInfo.bili = biliResult;
                }

                JsonObject holiday = json["holiday"];
                if (!holiday.isNull()) {
                    Holiday holidayResult;
                    holidayResult.year = holiday["year"].as<int>();
                    holidayResult.month = holiday["month"].as<int>();
                    holidayResult.length = holiday["length"].as<int>();
                    JsonArray array = holiday["holidays"].as<JsonArray>();
                    for (int i = 0; i < holidayResult.length; i++) {
                        holidayResult.holidays[i] = array[i].as<int>();
                    }
                    apiInfo.holiday = holidayResult;
                }

                return true;
            });
    }

private:
    HTTPClient http;
    WiFiClientSecure wiFiClientSecure;
    WiFiClient wiFiClient;

    
    bool getRestfulAPI(String url, callback cb, precall pre = precall()) {

        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("Wi-Fi 未连接!");
            return false;
        }

        HTTPClient http;

        // 自动判断 HTTP / HTTPS
        bool isHttps = url.startsWith("https://");

        // 创建对应的客户端
        WiFiClient client;
        WiFiClientSecure clientSecure;

        WiFiClient* httpClient = nullptr;

        if (isHttps) {
            // HTTPS 自动跳过证书验证
            clientSecure.setInsecure();
            httpClient = &clientSecure;
        } else {
            httpClient = &client;
        }

        JsonDocument doc;

        for (uint8_t i = 0; i < MAX_RETRY; i++) {
            bool shouldRetry = false;

            if (http.begin(*httpClient, url)) {

                if (pre) pre();

                int httpCode = http.GET();

                if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_NOT_MODIFIED) {

                    String str = http.getString();

                    DeserializationError error = deserializeJson(doc, str);

                    if (error) {
                        Serial.printf("JSON Parse error: %s\n", error.c_str());
                        Serial.printf("Received data: %s\n", str);
                        shouldRetry = (error == DeserializationError::IncompleteInput);
                    } else {
                        http.end();
                        return cb(doc);
                    }

                } else {
                    Serial.printf("HTTP Error: %d\n", httpCode);
                    shouldRetry = (
                        httpCode == HTTPC_ERROR_CONNECTION_REFUSED ||
                        httpCode == HTTPC_ERROR_CONNECTION_LOST ||
                        httpCode == HTTPC_ERROR_READ_TIMEOUT
                    );
                }

                http.end();
            } else {
                Serial.println("HTTP begin failed");
            }

            if (!shouldRetry) break;

            Serial.println("Retry after 5 seconds...");
            delay(5000);
        }

        return false;
    }
};
#endif  // __API_HPP__
