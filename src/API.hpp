#ifndef __API_HPP__
#define __API_HPP__

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_http_client.h>

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
};

// 定义单词结构体
struct Word {
    String ch;
    String en;
};

// 定义返回数据结构体

struct ApiInfo {
    Weather weather;
    std::vector<Word> dailyWords;
};

struct Bilibili {
    uint64_t follower;
    uint64_t view;
    uint64_t likes;
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
    bool getApiInfo(ApiInfo& apiInfo, const char* host, const char* locid) {
        return getRestfulAPI(
            "http://" + String(host) + "/apiInfo?location=" + String(locid), [&apiInfo](JsonDocument& json) {
                if (strcmp(json["code"], "200") != 0) {
                    Serial.print(F("Get weather failed, error: "));
                    Serial.println(json["code"].as<const char*>());
                    return false;
                }

                JsonObject weather = json["weather"];
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
                apiInfo.weather = weatherResult;

                JsonArray dailyWords = json["dailyWords"];
                std::vector<Word> dailyWordsResult;
                // 解析 words 数组
                for (JsonObject wordObj : dailyWords) {
                    Word w;
                    w.ch = wordObj["ch"].as<const char*>();
                    w.en = wordObj["en"].as<const char*>();
                    dailyWordsResult.push_back(w);
                }
                apiInfo.dailyWords = dailyWordsResult;

                return true;
            });
    }

    // B站粉丝
    // https://api.bilibili.com/x/relation/stat?vmid=4778211
    bool getFollower(Bilibili& result, uint32_t uid) {
        return getRestfulAPI("https://api.bilibili.com/x/relation/stat?vmid=" + String(uid), [&result](JsonDocument& json) {
            if (json["code"] != 0) {
                Serial.print(F("Get bilibili follower failed, error: "));
                Serial.println(json["message"].as<const char*>());
                return false;
            }
            result.follower = json["data"]["follower"];
            return true;
            });
    }

    // B站总播放量和点赞数
    // https://api.bilibili.com/x/space/upstat?mid=4778211
    bool getLikes(Bilibili& result, uint32_t uid, const char* cookie) {
        return getRestfulAPI(
            "https://api.bilibili.com/x/space/upstat?mid=" + String(uid), [&result](JsonDocument& json) {
                if (json["code"] != 0) {
                    Serial.print(F("Get bilibili likes failed, error: "));
                    Serial.println(json["message"].as<const char*>());
                    return false;
                }
                result.view = json["data"]["archive"]["view"];
                result.likes = json["data"]["likes"];
                return true;
            },
            [this, &cookie]() {
                http.addHeader("Cookie", String("SESSDATA=") + cookie + ";");
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
                        Serial.print(F("JSON Parse error: "));
                        Serial.println(error.c_str());
                        Serial.println(str);
                        shouldRetry = (error == DeserializationError::IncompleteInput);
                    } else {
                        http.end();
                        return cb(doc);
                    }

                } else {
                    Serial.print(F("HTTP Error: "));
                    Serial.println(http.errorToString(httpCode));
                    shouldRetry = (
                        httpCode == HTTPC_ERROR_CONNECTION_REFUSED ||
                        httpCode == HTTPC_ERROR_CONNECTION_LOST ||
                        httpCode == HTTPC_ERROR_READ_TIMEOUT
                    );
                }

                http.end();
            } else {
                Serial.println(F("HTTP begin failed"));
            }

            if (!shouldRetry) break;

            Serial.println(F("Retry after 5 seconds..."));
            delay(5000);
        }

        return false;
    }
};
#endif  // __API_HPP__
