from flask import Flask, jsonify, request
import json
import random
import requests
from datetime import datetime, timedelta, timezone
import logging
import re

QWEATHER_URL = "https://pc487rtvyv.re.qweatherapi.com/v7/weather/now"
QWEATHER_URL_2 = "https://pc487rtvyv.re.qweatherapi.com/v7/weather/30d"
API_KEY = "xxx"
LOCATION = "101021600" # 上海市
CET6_WORDS_PATH = "/root/sh/cet6_words.json"
WORDS_HISTORY_DAYS = 3
BILI_SESSDATA = "xxx"

COOKIES = {
    "SESSDATA": BILI_SESSDATA
}

HEADERS = {
    "accept": "*/*",
    "accept-language": "zh-CN,zh;q=0.9",
    "origin": "https://space.bilibili.com",
    "referer": "https://space.bilibili.com/",
    "sec-ch-ua": '"Chromium";v="142", "Google Chrome";v="142", "Not_A Brand";v="99"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "sec-fetch-dest": "empty",
    "sec-fetch-mode": "cors",
    "sec-fetch-site": "same-site",
    "user-agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/142.0.0.0 Safari/537.36"
}

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger("weather_server")

app = Flask(__name__)
words_cache = {}
bili_cache = {}
api_info_cache = {}


@app.route("/apiInfo")
def apiInfo():
    battery = request.args.get('battery') or None
    location = request.args.get('location') or LOCATION
    logger.info(f"/apiInfo called with battery={battery}, location={location}")

    global api_info_cache
    need_update = False

    if not api_info_cache or location not in api_info_cache:
        need_update = True
    else:
        last_update = api_info_cache[location].get("updateTime", "")
        last_updatetime = datetime.strptime(last_update, "%Y-%m-%d %H:%M:%S")
        if not last_update or datetime.now() - last_updatetime > timedelta(minutes=50):
            need_update = True
            logger.info(f"apiInfo cache is outdated, last update: {last_update}, location: {location}")
    if need_update:
        logger.info("Updating apiInfo cache, location: " + location)
        api_info_cache[location] = {
            "code": "200",
            "weather": get_weather(location),
            "dailyWords": get_daily_words(),
            "biliInfo": get_bili_info(),
            "holiday": get_holiday(),
            "updateTime": datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        }
    else:
        logger.info(f"Using cached apiInfo, last update: {api_info_cache[location].get('updateTime','')}, location: {location}")

    return jsonify(api_info_cache[location])


baseFollower = 0
baseAllView = 0
baseAllLikes = 0
baseComment = 0
baseView = 0
baseLikes = 0
latest_bvid = 0


def get_bili_info():
    global bili_cache, baseFollower, baseAllView, baseAllLikes, baseComment, baseView, baseLikes, latest_bvid
    need_update = False
    last_updatetime = None

    if not bili_cache:
        need_update = True
    else:
        last_update = bili_cache.get("updateTime", "")
        last_updatetime = datetime.strptime(last_update, "%Y-%m-%d %H:%M:%S")
        if datetime.now() - last_updatetime > timedelta(minutes=1):
            need_update = True
            logger.info(f"biliInfo cache is outdated, last update: {last_update}")
        elif datetime.now().date() != last_updatetime.date():
            need_update = True
            logger.info(f"biliInfo cache is from previous day, last update: {last_update}")
    if need_update:
        logger.info("Updating biliInfo cache")
        bili_user_info = get_bili_user()

        if last_updatetime is None or datetime.now().date() != last_updatetime.date():
            logger.info("New day detected, base biliInfo: " + json.dumps(bili_user_info))

            baseFollower = bili_user_info.get("follower", 0)
            baseAllView = bili_user_info.get("allView", 0)
            baseAllLikes = bili_user_info.get("allLikes", 0)

            baseComment = bili_user_info.get("comment", 0)
            baseView = bili_user_info.get("view", 0)
            baseLikes = bili_user_info.get("likes", 0)

        if bili_user_info.get("latest_bvid") != latest_bvid:
            latest_bvid = bili_user_info.get("latest_bvid")
            baseComment = bili_user_info.get("comment", 0)
            baseView = bili_user_info.get("view", 0)
            baseLikes = bili_user_info.get("likes", 0)

        video_date_list = bili_user_info.get("videoDates", [])
        video_delta_days = bili_user_info.get("videoDeltaDays", 0)

        live_dates_info = get_live_dates()
        live_delta_days = live_dates_info.get("liveDeltaDays", 0)
        live_date_list = live_dates_info.get("liveDates", [])

        bili_cache = {
            "userName": str(bili_user_info.get("uid", "")),

            "follower": format_number(bili_user_info.get("follower", 0)),
            "allView": format_number(bili_user_info.get("allView", 0)),
            "allLikes": format_number(bili_user_info.get("allLikes", 0)),

            "addFollower": format_number(bili_user_info.get("follower", 0) - baseFollower),
            "addAllView": format_number(bili_user_info.get("allView", 0) - baseAllView),
            "addAllLikes": format_number(bili_user_info.get("allLikes", 0) - baseAllLikes),

            "comment": format_number(bili_user_info.get("comment", 0)),
            "view": format_number(bili_user_info.get("view", 0)),
            "likes": format_number(bili_user_info.get("likes", 0)),
            "latest_bvid": latest_bvid,

            "addComment": format_number(bili_user_info.get("comment", 0) - baseComment),
            "addView": format_number(bili_user_info.get("view", 0) - baseView),
            "addLikes": format_number(bili_user_info.get("likes", 0) - baseLikes),

            "videoDeltaDays": f"距离上次投稿：{video_delta_days}天",
            "liveDeltaDays": f"距离上次直播：{live_delta_days}天",
            "videoDates": video_date_list,
            "liveDates": live_date_list,

            "videoDateTags": build_day_array(video_date_list, '\u00D3'),
            "liveDateTags": build_day_array(live_date_list, '\u00D4'),

            "updateTime": datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        }

    else:
        logger.info(f"Using cached biliInfo, last update: {bili_cache.get('updateTime', '')}")

    return bili_cache


def format_number(n: int) -> str:

    if n <= 0:
        return "0"

    # 0 - 999
    if n <= 999:
        return str(n)

    # 1000 - 9999
    if n <= 9999:
        v = n / 1000
        return f"{v:,.1f}k"

    # 10000 - 99999
    if n <= 99999:
        v = n / 10000
        return f"{v:,.1f}w"

    v = n / 10000
    return f"{int(v)}w"


def build_weather_future_array(weather_future_list):
    result = ['0'] * 31
    today = datetime.now().strftime("%Y-%m-%d")
    for weather_future in weather_future_list:
        date = weather_future["date"]
        if today[0:7] == date[0:7] and today[8:10] != date[8:10]:
            day = int(date[8:10])
            result[day - 1] = weather_future["iconDay"]

    return result


def build_day_array(live_dates, mark_char):
    """
    :param live_dates: 日期字符串数组，如 ["2025-12-14", "2025-12-13", ...]
    :param mark_char:  标记字符，如 '\u0042': tag, '\u0024': dollar, '\u0053': smile, '\u0021': warning
    :return:           长度为31的数组
    """
    result = ['0'] * 31

    for date_str in live_dates:
        day = int(date_str.split('-')[2])
        result[day - 1] = mark_char

    return result


@app.route("/bili_info", methods=["GET"])
def bili_info():
    return jsonify(get_bili_info())


@app.route("/live_dates", methods=["GET"])
def live_dates():
    return jsonify(get_live_dates())


def get_live_dates():
    mid = request.args.get("mid")
    season_id = request.args.get("season_id")

    if not mid or not season_id:
        mid = "3913407"
        season_id = "6632037"
        # return jsonify({"error": "mid and season_id are required"}), 400

    try:
        # ------------------------------
        # 调用 B 站 seasons_archives_list
        # ------------------------------
        resp = requests.get(
            "https://api.bilibili.com/x/polymer/web-space/seasons_archives_list",
            params={
                "mid": mid,
                "season_id": season_id,
                "sort_reverse": "true",
                "page_size": 30,
                "page_num": 1,
            },
            headers=HEADERS,
            # cookies=COOKIES
        ).json()

        if resp.get("code") != 0:
            return {}

        archives = resp["data"].get("archives", [])

        # ------------------------------
        # 从 title 提取日期 YYYY-MM-DD
        # ------------------------------
        date_pattern = re.compile(r"(\d{4}-\d{2}-\d{2})")
        extracted_dates = []

        for item in archives:
            title = item.get("title", "")
            match = date_pattern.search(title)
            if match:
                extracted_dates.append(match.group(1))

        if not extracted_dates:
            return {}

        live_date_list = [d for d in extracted_dates if d.startswith(datetime.now().strftime("%Y-%m"))]

        # 去重 + 按时间倒序排序
        live_date_list = sorted(list(set(live_date_list)), reverse=True)

        delta_days = ""
        if len(live_date_list) > 0:
            latest_date_str = live_date_list[0]  # 第一条通常是最新的
            latest_dt = datetime.strptime(latest_date_str, "%Y-%m-%d")

            today = datetime.now()
            delta_days = (today.date() - latest_dt.date()).days

        return {
            "mid": mid,
            "liveDeltaDays": delta_days,
            "liveDates": live_date_list
        }

    except Exception as e:
        logger.error("Error in get_live_dates: " + str(e))
        return {}


@app.route("/bili_user", methods=["GET"])
def bili_user():
    return jsonify(get_bili_user())


def get_bili_user():
    uid = request.args.get("uid")
    if not uid:
        uid = "4778211"
        # return jsonify({"error": "uid required"}), 400

    try:
        # --------------------------
        # ① 获取粉丝数 relation/stat
        # --------------------------
        rel_resp = requests.get(
            "https://api.bilibili.com/x/relation/stat",
            params={"vmid": uid},
            headers=HEADERS,
            cookies=COOKIES
        ).json()

        if rel_resp.get("code") != 0:
            return {}

        follower = rel_resp["data"]["follower"]

        # --------------------------
        # ② 获取 upstat（播放量 + 获赞）
        # --------------------------
        up_resp = requests.get(
            "https://api.bilibili.com/x/space/upstat",
            params={"mid": uid, "web_location": "333.1387"},
            headers=HEADERS,
            cookies=COOKIES
        ).json()

        archive_view = 0
        total_likes = 0
        if up_resp.get("code") == 0 and not up_resp["data"].get("archive") is None:
            archive_view = up_resp["data"]["archive"]["view"]
            total_likes = up_resp["data"]["likes"]
        else:
            logger.error("get upstat error")

        # --------------------------
        # ③ 获取最新视频（archives）
        # --------------------------
        archive_resp = requests.get(
            "https://api.bilibili.com/x/series/recArchivesByKeywords",
            params={
                "mid": uid,
                "keywords": "",
                "orderby": "pubdate"
            },
            headers=HEADERS,
            # cookies=COOKIES
        ).json()

        if archive_resp.get("code") != 0:
            return {}

        video_detail = {}
        video_dates = []
        delta_days = 0
        latest_bvid = ""
        archives = archive_resp["data"].get("archives", [])
        if archives:
            latest_video = archives[0]
            latest_bvid = latest_video["bvid"]

            # --------------------------
            # ④ 根据 bvid 获取视频详细信息（wbi/view）
            # --------------------------
            view_resp = requests.get(
                "https://api.bilibili.com/x/web-interface/wbi/view",
                params={"bvid": latest_bvid},
                headers=HEADERS,
                cookies=COOKIES
            ).json()

            if view_resp.get("code") == 0:
                stat = view_resp["data"]["stat"]
                video_detail = {
                    "view": stat["view"],
                    "danmaku": stat["danmaku"],
                    "reply": stat["reply"],
                    "like": stat["like"],
                    "favorite": stat["favorite"],
                    "coin": stat["coin"],
                    "share": stat["share"],
                    "latest_bvid": latest_bvid
                }

            for item in archives:
                ts = item.get("pubdate")
                if not ts:
                    continue

                # 转为 YYYY-MM-DD
                dt = datetime.fromtimestamp(ts, tz=timezone.utc).astimezone()
                date_str = dt.strftime("%Y-%m-%d")
                video_dates.append(date_str)

            video_dates = [d for d in video_dates if d.startswith(datetime.now().strftime("%Y-%m"))]
            video_dates = sorted(list(set(video_dates)), reverse=True)

            # ------------------------------
            # 计算最近一次投稿距今天多少天
            # ------------------------------
            delta_days = ""
            if len(video_dates) > 0:
                latest_date_str = video_dates[0]  # 第一条通常是最新的
                latest_dt = datetime.strptime(latest_date_str, "%Y-%m-%d")

                today = datetime.now()
                delta_days = (today.date() - latest_dt.date()).days

        return {
            "uid": uid,
            "follower": follower,
            "allView": archive_view,
            "allLikes": total_likes,
            "comment": (video_detail.get("reply") + video_detail.get("danmaku") + video_detail.get("share")) if video_detail else 0,
            "view": video_detail.get("view") if video_detail else 0,
            "likes": (video_detail.get("like") + video_detail.get("coin") + video_detail.get("favorite")) if video_detail else 0,
            "videoDeltaDays": delta_days,
            "videoDates": video_dates,
            "latest_bvid": latest_bvid
        }

    except Exception as e:
        logger.error("get_bili_user error: " + str(e))
        return {}


@app.route("/holiday", methods=["GET"])
def holiday():
    year_month = request.args.get("yearMonth", type=int)
    return jsonify(get_holiday(year_month))


def get_holiday(year_month=None):
    """
    请求参数: 2026-01
    返回结构:
    {
        "year": 2026,
        "month": 1,
        "length": 4,
        "holidays": [1, 2, 3, -4]
    }
    """

    if not year_month:
        year_month = datetime.now().strftime("%Y-%m")

    # 调用第三方 API 获取节假日数据
    api_url = f"https://timor.tech/api/holiday/year/{year_month}"
    try:
        resp = requests.get(api_url, headers=HEADERS, )
        resp.raise_for_status()
        data = resp.json()
    except Exception as e:
        return jsonify({"error": f"Failed to fetch holiday data: {str(e)}"}), 500

    if data.get("code") != 0 or "holiday" not in data:
        return jsonify({"error": "Invalid holiday data"}), 500

    holidays_list = []
    for day_str, info in data["holiday"].items():
        day = int(day_str[-2:])  # 'YYYYMMDD' -> 'DD'
        is_holiday = info.get("holiday", False)
        holidays_list.append(day if is_holiday else -day)
        if len(holidays_list) >= 50:
            break
    split = year_month.split("-")
    result = {
        "year": int(split[0]),
        "month": int(split[1]),
        "length": len(holidays_list),
        "holidays": holidays_list
    }
    return result


@app.route("/weather")
def weather():
    location = request.args.get('location') or LOCATION
    return jsonify(get_weather(location))


@app.route("/dailyWords", methods=["GET"])
def daily_words():
    return jsonify(get_daily_words())


def get_weather_future(location):
    params = {
        "key": API_KEY,
        "location": location
    }

    # requests 会自动处理 gzip 压缩
    resp = requests.get(QWEATHER_URL_2, params=params, timeout=5)
    data = resp.json()
    logger.info("Fetched weather future data: " + json.dumps(data)[0:50])

    result = []
    daily = data.get("daily", [])

    for day in daily:
        result.append({
            "date": day.get("fxDate", ""),
            "iconDay": get_icon(int(day.get("iconDay", 999)), datetime.now().strftime("%Y-%m-%dT%H:%M+08:00")),
            "iconNight": get_icon(int(day.get("iconNight", 999)), datetime.now().strftime("%Y-%m-%dT%H:%M+08:00")),
            "textDay": day.get("textDay", ""),
            "textNight": day.get("textNight", "")
        })

    return result


def get_weather(location):
    params = {
        "key": API_KEY,
        "location": location
    }

    # requests 会自动处理 gzip 压缩
    resp = requests.get(QWEATHER_URL, params=params, timeout=5)
    data = resp.json()
    logger.info("Fetched weather data: " + json.dumps(data))

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
        "future": build_weather_future_array(get_weather_future(location)),
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


def load_words(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def pick_random_words(data):
    """从词库随机选 3 个 word 项"""
    return random.sample(data, 3)


def pick_phrase_or_word(base_word, data):
    """如果 base_word 有 phrases, 返回随机 phrase, 否则随机 word"""
    if "phrases" in base_word and base_word["phrases"]:
        return random.choice(base_word["phrases"])
    else:
        return random.choice(data)


def format_word_item(item):
    if "phrase" in item:
        return {"en": item["phrase"], "ch": item["translation"]}

    trans = item["translations"][0]
    t = f"{trans['type']}. {trans['translation']}"
    return {"en": item["word"], "ch": t}


def get_random_words():
    data = load_words(CET6_WORDS_PATH)

    # Step 1: 随机取 3 个单词
    first_three = pick_random_words(data)

    # Step 2: 根据前 3 个生成后 3 个
    next_three = []
    for i in range(3):
        base = first_three[i]
        chosen = pick_phrase_or_word(base, data)
        next_three.append(chosen)

    # Step 3: 格式化 [["english","中文"], ...]
    formatted = (
            [format_word_item(w) for w in first_three] +
            [format_word_item(w) for w in next_three]
    )

    return formatted


# 每天生成一次新的单词，缓存起来，返回最近几天的单词总和
def get_daily_words():
    global words_cache
    today = datetime.now().strftime("%Y-%m-%d")

    # 缓存判断
    if not today in words_cache:
        logger.info("Generating new daily words for " + today)
        today_words = get_random_words()
        words_cache[today] = today_words
        logger.info("New day detected, today: " + today)

    # 汇总最近几天的单词
    result_words = list(words_cache[today])
    for i in range(1, 3):
        day = (datetime.now() - timedelta(days=i)).strftime("%Y-%m-%d")
        if not day in words_cache:
            logger.info("Generating new daily words for " + day)
            day_words = get_random_words()
            words_cache[day] = day_words
        result_words += words_cache[day]

    return result_words


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)

