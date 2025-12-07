from flask import Flask, jsonify
import json
import random
import requests
from datetime import datetime

app = Flask(__name__)

# 简单内存缓存（如果希望持久化可改文件缓存）
_cache = {}

QWEATHER_URL = "https://your_qweather_api_host/v7/weather/now"
API_KEY = "your_api_key"
LOCATION = "101021600" #上海


@app.route("/weather")
def weather():
    weather = get_weather()
    return jsonify(weather)


def get_weather():
    params = {
        "key": API_KEY,
        "location": LOCATION
    }

    resp = requests.get(QWEATHER_URL, params=params, timeout=5)
    data = resp.json()
    print(data)

    now = data.get("now", {})

    icon_id = int(now.get("icon", 999))
    icon = get_icon(icon_id, now.get("obsTime"))

    result = {
        "time": now.get("obsTime", ""),
        "temp": now.get("temp", ""),
        "humidity": now.get("humidity", ""),
        "windDir": now.get("windDir", ""),
        "windScale": now.get("windScale", ""),
        "windSpeed": now.get("windSpeed", ""),
        "icon": icon,
        "text": now.get("text", ""),
        "updateTime": data.get("updateTime", ""),
        "code": data.get("code", "404")
    }

    return result


ICON_TABLE = {
    100: ("\uf1ac", "\uf101"),
    101: ("\uf1ad", "\uf102"),
    102: ("\uf1ae", "\uf103"),
    103: ("\uf1af", "\uf104"),
    104: ("\uf1b0", "\uf105"),
    150: ("\uf1b1", "\uf106"),
    151: ("\uf1b2", "\uf107"),
    152: ("\uf1b3", "\uf108"),
    153: ("\uf1b4", "\uf109"),
    300: ("\uf1b5", "\uf10a"),
    301: ("\uf1b6", "\uf10b"),
    302: ("\uf1b7", "\uf10c"),
    303: ("\uf1b8", "\uf10d"),
    304: ("\uf1b9", "\uf10e"),
    305: ("\uf1ba", "\uf10f"),
    306: ("\uf1bb", "\uf110"),
    307: ("\uf1bc", "\uf111"),
    308: ("\uf1bd", "\uf112"),
    309: ("\uf1be", "\uf113"),
    310: ("\uf1bf", "\uf114"),
    311: ("\uf1c0", "\uf115"),
    312: ("\uf1c1", "\uf116"),
    313: ("\uf1c2", "\uf117"),
    314: ("\uf1c3", "\uf118"),
    315: ("\uf1c4", "\uf119"),
    316: ("\uf1c5", "\uf11a"),
    317: ("\uf1c6", "\uf11b"),
    318: ("\uf1c7", "\uf11c"),
    350: ("\uf1c8", "\uf11d"),
    351: ("\uf1c9", "\uf11e"),
    399: ("\uf1ca", "\uf11f"),
    400: ("\uf1cb", "\uf120"),
    401: ("\uf1cc", "\uf121"),
    402: ("\uf1cd", "\uf122"),
    403: ("\uf1ce", "\uf123"),
    404: ("\uf1cf", "\uf124"),
    405: ("\uf1d0", "\uf125"),
    406: ("\uf1d1", "\uf126"),
    407: ("\uf1d2", "\uf127"),
    408: ("\uf1d3", "\uf128"),
    409: ("\uf1d4", "\uf129"),
    410: ("\uf1d5", "\uf12a"),
    456: ("\uf1d6", "\uf12b"),
    457: ("\uf1d7", "\uf12c"),
    499: ("\uf1d8", "\uf12d"),
    500: ("\uf1d9", "\uf12e"),
    501: ("\uf1da", "\uf12f"),
    502: ("\uf1db", "\uf130"),
    503: ("\uf1dc", "\uf131"),
    504: ("\uf1dd", "\uf132"),
    507: ("\uf1de", "\uf133"),
    508: ("\uf1df", "\uf134"),
    509: ("\uf1e0", "\uf135"),
    510: ("\uf1e1", "\uf136"),
    511: ("\uf1e2", "\uf137"),
    512: ("\uf1e3", "\uf138"),
    513: ("\uf1e4", "\uf139"),
    514: ("\uf1e5", "\uf13a"),
    515: ("\uf1e6", "\uf13b"),
    800: ("\uf13c", "\uf13c"),
    801: ("\uf13d", "\uf13d"),
    802: ("\uf13e", "\uf13e"),
    803: ("\uf13f", "\uf13f"),
    804: ("\uf140", "\uf140"),
    805: ("\uf141", "\uf141"),
    806: ("\uf142", "\uf142"),
    807: ("\uf143", "\uf143"),
    900: ("\uf1e7", "\uf144"),
    901: ("\uf1e8", "\uf145"),
    999: ("\uf1e9", "\uf146")
}


def is_night(timestr: str) -> bool:
    """判断是否夜间"""
    # 格式示例："2025-12-06T23:32+08:00"
    hour = int(timestr[11:13])
    return hour < 6 or hour >= 18


def get_icon(icon_id: int, time: str) -> str:
    night = is_night(time)
    day_icon, night_icon = ICON_TABLE.get(icon_id, ("\uf1e9", "\uf146"))
    return night_icon if night else day_icon


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)
