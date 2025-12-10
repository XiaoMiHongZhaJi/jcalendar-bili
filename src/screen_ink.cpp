#include "screen_ink.h"
#include <api_info.h>
#include <API.hpp>
#include "holiday.h"
#include "nongli.h"
#include <_preference.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <GxEPD2_3C.h>
#include "GxEPD2_display_selection_new_style.h"
#include "font.h"
#include "battery.h"
#include "wiring.h"

#define ROTATION 0
#define FONT_TEXT u8g2_font_wqy16_t_gb2312 // 224825bytes，最大字库（天气描述中“霾”，只有此字库中有）
#define FONT_SUB u8g2_font_wqy12_t_gb2312 // 次要字体，u8g2最小字体
#define FONT_BOLD u8g2_font_helvB12_tf      // 英文粗体

GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, GxEPD2_DRIVER_CLASS::HEIGHT> display(GxEPD2_DRIVER_CLASS(/*CS=D8*/ SPI_CS, /*DC=D3*/ SPI_DC, /*RST=D4*/ SPI_RST, /*BUSY=D2*/ SPI_BUSY));
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

const String week_str[] = { "日", "一", "二", "三", "四", "五", "六" };
const String nl10_str[] = { "初", "十", "廿", "卅" }; // 农历十位
const String nl_str[] = { "十", "一", "二", "三", "四", "五", "六", "七", "八", "九", "十" }; // 农历个位
const String nl_mon_str[] = { "", "正", "二", "三", "四", "五", "六", "七", "八", "九", "十", "冬", "腊" }; // 农历首位

int _screen_status = -1;
String _tag_days_str = "00000005b";
int _week_1st = 1;
int lunarDates[31];
int jqAccDate[24]; // 节气积累日

const int jrLength = 11;
const int jrDate[] = { 101, 214, 308, 312, 501, 504, 601, 701, 801, 910, 1001, 1224, 1225 };
const String jrText[] = { "元旦", "情人节", "妇女节", "植树节", "劳动节", "青年节", "儿童节", "建党节", "建军节", "教师节", "国庆节", "平安夜", "圣诞节" };

int _screen_index = 0;


struct tm tmInfo = { 0 }; // 日历显示用的时间

struct
{
    int16_t topH;

    int16_t yearX;
    int16_t yearY;

    int16_t headerX;
    int16_t headerY;
    int16_t headerH;

    int16_t daysX;
    int16_t daysY;
    int16_t dayW;
} static calLayout;

TaskHandle_t SCREEN_HANDLER;

void init_cal_layout_size() {
    calLayout.topH = 60;

    calLayout.yearX = 10;
    calLayout.yearY = calLayout.topH - 28;

    calLayout.headerX = 0;
    calLayout.headerY = calLayout.topH;
    calLayout.headerH = 20;

    calLayout.daysX = 0;
    calLayout.daysY = calLayout.headerY + calLayout.headerH;
    calLayout.dayW = 56;
}

void draw_cal_header() {
    uint16_t color;

    u8g2Fonts.setFont(FONT_TEXT);
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setFontDirection(0);
    u8g2Fonts.setForegroundColor(GxEPD_WHITE);
    int16_t daysMagin = 4;

    for (int i = 0; i < 7; i++) {
        if ((i + _week_1st) % 7 == 0 || (i + _week_1st) % 7 == 6) {
            color = GxEPD_RED;
        } else {
            color = GxEPD_BLACK;
        }
        // header background
        if (i == 0) {
            display.fillRect(0, calLayout.headerY, (display.width() - 7 * calLayout.dayW) / 2, calLayout.headerH, color);
        } else if (i == 6) {
            display.fillRect((display.width() + 7 * calLayout.dayW) / 2, calLayout.headerY, (display.width() - 7 * calLayout.dayW) / 2, calLayout.headerH, color);
        }
        display.fillRect((display.width() - 7 * calLayout.dayW) / 2 + i * calLayout.dayW, calLayout.headerY, calLayout.dayW, calLayout.headerH, color);

        // header text
        u8g2Fonts.drawUTF8(calLayout.headerX + daysMagin + (calLayout.dayW - u8g2Fonts.getUTF8Width(week_str[i].c_str())) / 2 + i * calLayout.dayW, calLayout.headerY + calLayout.headerH - 3, week_str[(i + _week_1st) % 7].c_str());
    }
}

uint16_t todayColor = GxEPD_BLACK;
String todayLunarYear;
String todayLunarDay;

// 更新年份
void draw_cal_year() {

    // 日期
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setFontDirection(0);
    u8g2Fonts.setForegroundColor(todayColor);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    u8g2Fonts.setCursor(calLayout.yearX, calLayout.yearY);
    u8g2Fonts.setFont(u8g2_font_fub25_tn);
    u8g2Fonts.print(String(tmInfo.tm_year + 1900).c_str());
    u8g2Fonts.setFont(FONT_TEXT);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.print("年");
    u8g2Fonts.setFont(u8g2_font_fub25_tn);
    u8g2Fonts.setForegroundColor(todayColor);
    u8g2Fonts.print(String(tmInfo.tm_mon + 1).c_str());
    u8g2Fonts.setFont(FONT_TEXT);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.print("月");
    u8g2Fonts.setFont(u8g2_font_fub25_tn);
    u8g2Fonts.setForegroundColor(todayColor);
    u8g2Fonts.printf(String(tmInfo.tm_mday).c_str());
    u8g2Fonts.setFont(FONT_TEXT);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.print("日");
}

void draw_cal_days() {

    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setFontDirection(0);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);

    size_t totalDays = 30; // 小月
    int monthNum = tmInfo.tm_mon + 1;
    if (monthNum == 1 || monthNum == 3 || monthNum == 5 || monthNum == 7 || monthNum == 8 || monthNum == 10 || monthNum == 12) { // 大月
        totalDays = 31;
    }
    if (monthNum == 2) {
        if ((tmInfo.tm_year + 1900) % 4 == 0
            && (tmInfo.tm_year + 1900) % 100 != 0
            || (tmInfo.tm_year + 1900) % 400 == 0) {
            totalDays = 29; // 闰二月
        } else {
            totalDays = 28; // 二月
        }
    }

    // 计算本月第一天星期几
    int wday1 = (36 - tmInfo.tm_mday + tmInfo.tm_wday) % 7;
    // 计算本月第一天是全年的第几天（0～365）
    int yday1 = tmInfo.tm_yday - tmInfo.tm_mday + 1;

    // 确认哪些日期需要打tag
    char tags[31] = { 0 };
    int indexBegin = 0;
    while (_tag_days_str.length() >= (indexBegin + 9)) {
        String y = _tag_days_str.substring(indexBegin, indexBegin + 4);
        String m = _tag_days_str.substring(indexBegin + 4, indexBegin + 6);
        String d = _tag_days_str.substring(indexBegin + 6, indexBegin + 8);
        char t = _tag_days_str.charAt(indexBegin + 8);

        if ((y.equals(String(tmInfo.tm_year + 1900)) || y.equals("0000")) && (m.equals(String(tmInfo.tm_mon + 1)) || m.equals("00"))) {
            tags[d.toInt()] = t;
        }

        // Serial.printf("Format: %s, %s, %s, %c\n", y.c_str(), m.c_str(), d.c_str(), t);

        indexBegin = indexBegin + 9;
        while (indexBegin < _tag_days_str.length() && (_tag_days_str.charAt(indexBegin) < '0' || _tag_days_str.charAt(indexBegin) > '9')) { // 搜索字符串直到下个字符是0-9之间的
            indexBegin++;
        }
    }

    Holiday _holiday;
    Preferences pref;
    pref.begin(PREF_NAMESPACE);
    size_t holiday_size = pref.getBytesLength(PREF_HOLIDAY);
    if (holiday_size > 0) {
        pref.getBytes(PREF_HOLIDAY, &_holiday, holiday_size);
    }
    pref.end();

    if (_holiday.year != tmInfo.tm_year + 1900 || _holiday.month != tmInfo.tm_mon + 1) {
        _holiday = {};
    }

    int jqIndex = 0;
    int jrIndex = 0;
    int shiftDay = (wday1 - _week_1st) >= 0 ? 0 : 7;
    for (size_t iDay = 0; iDay < totalDays; iDay++) {
        uint8_t num = wday1 + iDay - _week_1st + shiftDay; // 根据每周首日星期做偏移
        uint8_t column = num % 7; //(0~6)
        uint8_t row = num / 7;    //(0~4)
        if (row == 5) row = 0;
        int16_t x = calLayout.daysX + 4 + column * 56;
        int16_t y = calLayout.daysY + row * 44;

        // 周六、日，字体红色
        uint16_t color;
        if ((wday1 + iDay) % 7 == 0 || (wday1 + iDay) % 7 == 6) {
            color = GxEPD_RED;
        } else {
            color = GxEPD_BLACK;
        }

        if (tmInfo.tm_year + 1900 == _holiday.year && tmInfo.tm_mon + 1 == _holiday.month) {
            uint8_t holidayIndex = 0;
            for (; holidayIndex < _holiday.length; holidayIndex++) {
                if (abs(_holiday.holidays[holidayIndex]) == (iDay + 1)) {
                    // 显示公休、调班logo和颜色
                    u8g2Fonts.setFont(u8g2_font_open_iconic_all_1x_t);
                    if (_holiday.holidays[holidayIndex] > 0) { // 公休
                        color = GxEPD_RED;
                        u8g2Fonts.setForegroundColor(color);
                        u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
                        u8g2Fonts.drawUTF8(x + 44, y + 11, "\u006c");
                    } else if (_holiday.holidays[holidayIndex] < 0) { // 调班
                        color = GxEPD_BLACK;
                        u8g2Fonts.setForegroundColor(color);
                        u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
                        u8g2Fonts.drawUTF8(x + 44, y + 11, "\u0064");
                    }
                    break;
                }
            }
        }
        u8g2Fonts.setForegroundColor(color); // 设置整体颜色
        u8g2Fonts.setBackgroundColor(GxEPD_WHITE);

        // 画日历日期数字
        u8g2Fonts.setFont(u8g2_font_fub17_tn); // u8g2_font_fub17_tn，u8g2_font_logisoso18_tn
        int16_t numX = x + (56 - u8g2Fonts.getUTF8Width(String(iDay + 1).c_str())) / 2;
        int16_t numY = y + 22;
        u8g2Fonts.drawUTF8(numX, numY, String(iDay + 1).c_str()); // 画日历日期

        // 画节气&节日&农历       
        String lunarStr = "";
        int lunarDate = lunarDates[iDay];
        int isLeapMon = lunarDate < 0 ? 1 : 0; // 闰月
        lunarDate = abs(lunarDate);
        int lunarMon = lunarDate / 100;
        int lunarDay = lunarDate % 100;

        bool isJq = false; // 是否节气
        int accDays0 = tmInfo.tm_yday + 1 - tmInfo.tm_mday; // 本月0日的积累日（tm_yday 从0开始，tm_mday从1开始, i从0开始）
        for (; jqIndex < 24; jqIndex++) {
            if (accDays0 + iDay + 1 < jqAccDate[jqIndex]) {
                break;
            }
            if (accDays0 + iDay + 1 == jqAccDate[jqIndex]) {
                lunarStr = String(nl_jq_text[jqIndex]);
                isJq = true;
                break;
            }
        }
        bool isJr = false; // 是否节日
        int currentDateNum = (tmInfo.tm_mon + 1) * 100 + iDay + 1;
        for (; jrIndex < jrLength; jrIndex++) {
            if (currentDateNum < jrDate[jrIndex]) {
                break;
            }
            if (currentDateNum == jrDate[jrIndex]) {
                lunarStr = jrText[jrIndex];
                isJr = true;
                break;
            }
        }
        if (!isJq && !isJr) { // 农历
            if (lunarDay == 1) {
                // 初一，显示月份
                lunarStr = (isLeapMon == 0 ? "" : "闰") + nl_mon_str[lunarMon] + "月";
            } else {
                if (lunarDay == 10) {
                    lunarStr = "初十";
                } else if (lunarDay == 20) {
                    lunarStr = "二十";
                } else if (lunarDay == 30) {
                    lunarStr = "三十";
                } else {
                    // 其他日期
                    lunarStr = nl10_str[lunarDay / 10] + nl_str[lunarDay % 10];
                }
            }
            if (lunarMon == 1 && lunarDay == 1) {
                lunarStr = "春节";
            } else if (lunarMon == 1 && lunarDay == 15) {
                lunarStr = "元宵节";
            } else if (lunarMon == 5 && lunarDay == 5) {
                lunarStr = "端午节";
            } else if (lunarMon == 7 && lunarDay == 7) {
                lunarStr = "七夕节";
            } else if (lunarMon == 8 && lunarDay == 15) {
                lunarStr = "中秋节";
            } else if (lunarMon == 9 && lunarDay == 9) {
                lunarStr = "重阳节";
            }
        }
        // 画节气/节日/农历文字
        u8g2Fonts.setFont(FONT_TEXT);
        u8g2Fonts.drawUTF8(x + (56 - u8g2Fonts.getUTF8Width(lunarStr.c_str())) / 2, y + 44 - 4, lunarStr.c_str());

        // 今日日期
        if ((iDay + 1) == tmInfo.tm_mday) { // 双线加粗
            todayColor = color;

            // 加框线
            display.drawRoundRect(x, y + 1, 56, 44, 4, GxEPD_RED);
            display.drawRoundRect(x + 1, y + 2, 54, 42, 3, GxEPD_RED);

            // 今日农历年份，e.g. 乙巳年 蛇
            // 如果农历月份小于公历月份，那么说明是上一年
            int tg = nl_tg(tmInfo.tm_year + 1900 - (lunarMon > (tmInfo.tm_mon + 1) ? 1 : 0));
            int dz = nl_dz(tmInfo.tm_year + 1900 - (lunarMon > (tmInfo.tm_mon + 1) ? 1 : 0));;
            todayLunarYear = String(nl_tg_text[tg]) + String(nl_dz_text[dz]) + "年 " + String(nl_sx_text[dz]);

            // 今日农历日期
            if (lunarDay == 10) {
                lunarStr = "初十";
            } else if (lunarDay == 20) {
                lunarStr = "二十";
            } else if (lunarDay == 30) {
                lunarStr = "三十";
            } else {
                // 其他日期
                lunarStr = nl10_str[lunarDay / 10] + nl_str[lunarDay % 10];
            }
            todayLunarDay = (isLeapMon == 0 ? "" : "闰") + nl_mon_str[lunarMon] + "月" + lunarStr;
        }

        // 画日期Tag
        const char* tagChar = NULL;
        if (tags[iDay + 1] == 'a') { //tag
            tagChar = "\u0042";
        } else if (tags[iDay + 1] == 'b') { // dollar
            tagChar = "\u0024";
        } else if (tags[iDay + 1] == 'c') { // smile
            tagChar = "\u0053";
        } else if (tags[iDay + 1] == 'd') { // warning
            tagChar = "\u0021";
        }
        if (tagChar != NULL) {
            u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
            u8g2Fonts.setForegroundColor(GxEPD_RED);
            u8g2Fonts.setFont(u8g2_font_twelvedings_t_all);
            int iconX = numX - u8g2Fonts.getUTF8Width(tagChar) - 1; // 数字与tag间间隔1像素
            iconX = iconX <= (x + 3) ? (iconX + 1) : iconX; // 防止icon与今日框线产生干涉。
            int iconY = y + 15;
            u8g2Fonts.drawUTF8(iconX, iconY, tagChar);
        }
    }
}

// 画天气信息
#include "API.hpp"
void draw_weather() {

    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setFontDirection(0);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    // 实时天气
    Weather* wNow = weather_data();
    
    // 天气图标
    u8g2Fonts.setFont(u8g2_font_qweather_icon_16);
    u8g2Fonts.setCursor(10, 59);
    u8g2Fonts.print(wNow->icon);

    // 天气描述+温度+湿度
    String wind = String(wNow->windDir) + String(wNow->windScale) + "级";
    if (wNow->windScale[0] == '0') {
        wind = "无风";
    }
    u8g2Fonts.setFont(FONT_SUB);
    u8g2Fonts.setCursor(40, 55);
    u8g2Fonts.printf("%s %s°C | %s%% %s", wNow->text, wNow->temp, wNow->humidity, wind.c_str());
}

// Draw err
void draw_err(const char* errMsg) {
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setFontDirection(0);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    u8g2Fonts.setForegroundColor(GxEPD_RED);
    u8g2Fonts.setFont(u8g2_font_open_iconic_all_2x_t);
    u8g2Fonts.setCursor(10, 55);
    u8g2Fonts.print("\u0118"); // error icon
    u8g2Fonts.setFont(FONT_SUB);
    u8g2Fonts.setCursor(30, 55);
    u8g2Fonts.print(errMsg);
}

void draw_battery(int voltage) {

    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setFontDirection(0);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setFont(u8g2_font_siji_t_6x10);

    // 电池icon
    const char* iconStr = "";
    if(voltage >= 4100) { // 满电
        iconStr = "\ue24b";
    } else if (voltage >= 3900) { // 多电
        iconStr = "\ue249";
    } else if (voltage >= 3700) { // 中电量
        iconStr = "\ue247";
    } else if (voltage >= 3500) { // 低电量
        iconStr = "\ue245";
    } else { // 空
        iconStr = "\ue242";
    }
    u8g2Fonts.drawUTF8(400 - 12 - 4, 14, iconStr);
}

// 横向虚线
void drawDashedHLine(int x, int y, int width, int dashLen = 5, int gapLen = 3, int color = GxEPD_BLACK) {
    int pos = 0;
    while (pos < width) {
        int w = min(dashLen, width - pos);
        display.drawFastHLine(x + pos, y, w, color);
        pos += dashLen + gapLen;
    }
}

// 纵向虚线
void drawDashedVLine(int x, int y, int height, int dashLen = 5, int gapLen = 3) {
    int pos = 0;
    while (pos < height) {
        int h = min(dashLen, height - pos);
        display.drawFastVLine(x, y + pos, h, GxEPD_BLACK);
        pos += dashLen + gapLen;
    }
}

const int title_height = 80;

int cellW = display.width() / 2;   // 2列
int cellH = (display.height() - title_height) / 3;  // 3行

boolean drawEnglishWords(int screen_index) {
    
    // drawDashedHLine(0, title_height - 20, display.width(), GxEPD_RED);
    // drawDashedHLine(0, title_height, display.width(), GxEPD_RED);

    display.fillRect(0, 60, display.width(), 20, GxEPD_BLACK);

    drawDashedHLine(0, cellH + title_height, display.width());
    drawDashedHLine(0, cellH * 2 + title_height, display.width());
    drawDashedVLine(cellW, title_height, display.height() + title_height);

    u8g2Fonts.setFont(FONT_TEXT);
    u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
    u8g2Fonts.setForegroundColor(GxEPD_WHITE);

    int word_index = (screen_index - 1) / 2 * 6;
    std::vector<Word> words = daily_words();
    if (words.size() < word_index + 6) {
        Serial.println("ERR: words size less than " + String(word_index + 6));
        const char* title = "单词数据异常";
        int titleWidth = u8g2Fonts.getUTF8Width(title);
        int titleTx = (display.width() - titleWidth) / 2;
        u8g2Fonts.drawUTF8(titleTx, title_height - 4, title);
        return false;
    }

    const char* title = "英语六级练习，20秒后揭晓答案...";
    u8g2Fonts.setFont(FONT_TEXT);
    int titleWidth = u8g2Fonts.getUTF8Width(title);
    int titleTx = (display.width() - titleWidth) / 2;
    u8g2Fonts.drawUTF8(titleTx, title_height - 4, title);

    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    for (int i = 0; i < 6; i++) {
        int row = i % 3;
        int col = i / 3;
        int x0 = col * cellW;
        int y0 = row * cellH + title_height;

        const char* en = words[word_index + i].en.c_str();
        const char* ch = words[word_index + i].ch.c_str();

        // 英文
        int len = strlen(en);
        char underline[len * 2];
        int space_index = 0;
        for (int i = 0; i < len; i++) {
            if (en[i] == ' ') {
                space_index = i;
                underline[i * 2] = ' ';
                underline[(i - 1) * 2] = en[i - 1];
            } else {
                underline[i * 2] = '_';
            }
            underline[i * 2 + 1] = ' ';
        }
        if (space_index > 0) {
            underline[(space_index + 1) * 2] = en[space_index + 1];
        }
        underline[0] = en[0];
        underline[(len - 1) * 2] = en[len - 1];
        underline[len * 2 - 1] = '\0';
        u8g2Fonts.setFont(FONT_BOLD);
        int twEng = u8g2Fonts.getUTF8Width(underline);
        int txEng = x0 + (cellW - twEng) / 2;
        int tyEng = y0 + cellH / 2 - 6;
        if (txEng < x0 + 2) {
            txEng = x0 + 2;
        }
        u8g2Fonts.setForegroundColor(GxEPD_RED);
        u8g2Fonts.drawUTF8(txEng, tyEng, underline);

        // 中文
        u8g2Fonts.setFont(FONT_TEXT);
        int twChi = u8g2Fonts.getUTF8Width(ch);
        if (twChi > cellW) {
            u8g2Fonts.setFont(FONT_SUB);
            twChi = u8g2Fonts.getUTF8Width(ch);
            if (twChi > cellW) {
                char dest[21];
                strncpy(dest, ch, 20);
                dest[20] = '\0';
                twChi = u8g2Fonts.getUTF8Width(dest);
            }
        }
        int txChi = x0 + (cellW - twChi) / 2;
        int tyChi = y0 + cellH / 2 + 16;
        if (txChi < x0 + 2) {
            txChi = x0 + 2;
        }
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        u8g2Fonts.drawUTF8(txChi, tyChi, ch);
    }
    return true;
}

boolean drawChineseWords(int screen_index) {

    // drawDashedHLine(0, title_height - 20, display.width(), GxEPD_RED);
    // drawDashedHLine(0, title_height, display.width(), GxEPD_RED);

    display.fillRect(0, 60, display.width(), 20, GxEPD_BLACK);

    drawDashedHLine(0, cellH + title_height, display.width());
    drawDashedHLine(0, cellH * 2 + title_height, display.width());
    drawDashedVLine(cellW, title_height, display.height());

    u8g2Fonts.setFont(FONT_TEXT);
    u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
    u8g2Fonts.setForegroundColor(GxEPD_WHITE);

    int word_index = (screen_index - 1) / 2 * 6;
    std::vector<Word> words = daily_words();
    if (words.size() < word_index + 6) {
        Serial.println("ERR: words size less than " + String(word_index + 6));
        const char* title = "单词数据异常";
        int titleWidth = u8g2Fonts.getUTF8Width(title);
        int titleTx = (display.width() - titleWidth) / 2;
        u8g2Fonts.drawUTF8(titleTx, title_height - 4, title);
        return false;
    }

    const char* title = "英语六级练习，答案揭晓";
    int titleWidth = u8g2Fonts.getUTF8Width(title);
    int titleTx = (display.width() - titleWidth) / 2;
    u8g2Fonts.drawUTF8(titleTx, title_height - 4, title);

    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    for (int i = 0; i < 6; i++) {
        int row = i % 3;
        int col = i / 3;
        int x0 = col * cellW;
        int y0 = row * cellH + title_height;

        const char* en = words[word_index + i].en.c_str();
        const char* ch = words[word_index + i].ch.c_str();

        // 英文
        u8g2Fonts.setFont(FONT_BOLD);
        int twEng = u8g2Fonts.getUTF8Width(en);
        int txEng = x0 + (cellW - twEng) / 2;
        int tyEng = y0 + cellH / 2 - 6;
        if (txEng < x0 + 2) {
            txEng = x0 + 2;
        }
        u8g2Fonts.setForegroundColor(GxEPD_RED);
        u8g2Fonts.drawUTF8(txEng, tyEng, en);

        // 中文
        u8g2Fonts.setFont(FONT_TEXT);
        int twChi = u8g2Fonts.getUTF8Width(ch);
        if (twChi > cellW) {
            u8g2Fonts.setFont(FONT_SUB);
            twChi = u8g2Fonts.getUTF8Width(ch);
            if (twChi > cellW) {
                char dest[21];
                strncpy(dest, ch, 20);
                dest[20] = '\0';
                twChi = u8g2Fonts.getUTF8Width(dest);
            }
        }
        int txChi = x0 + (cellW - twChi) / 2;
        int tyChi = y0 + cellH / 2 + 16;
        if (txChi < x0 + 2) {
            txChi = x0 + 2;
        }
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        u8g2Fonts.drawUTF8(txChi, tyChi, ch);
    }
    return true;
}

///////////// Calendar //////////////
/**
 * 处理时间日期信息
 */
boolean get_datetime() {

    time_t now = 0;
    time(&now);
    localtime_r(&now, &tmInfo); // 时间戳转化为本地时间结构
    Serial.printf("System Time: %d-%02d-%02d %02d:%02d:%02d\n", (tmInfo.tm_year + 1900), tmInfo.tm_mon + 1, tmInfo.tm_mday, tmInfo.tm_hour, tmInfo.tm_min, tmInfo.tm_sec);

    // 如果当前时间无效
    if (tmInfo.tm_year + 1900 < 2025) {
        if (api_info_status() == 1) {
            // 尝试使用api获取的时间
            Weather* weatherNow = weather_data();
            const char* apiTime = weatherNow->updateTime.c_str(); // e.g. 2024-11-14T17:36+08:00
            Serial.printf("API Time: %s\n", apiTime);
            tmInfo = { 0 }; // 重置为0
            if (strptime(apiTime, "%Y-%m-%dT%H:%M", &tmInfo) != NULL) {  // 将时间字符串转成tm时间 e.g. 2024-11-14T17:36+08:00
                time_t set = mktime(&tmInfo);
                timeval tv;
                tv.tv_sec = set;
                Serial.println("WARN: Set system time by api time.");
            } else {
                Serial.println("ERR: Fail to format api time.");
            }
        } else {
            // 如果天气也未获取成功，那么返回
            Serial.println("ERR: invalid time & not weather info got.");
            return false;
        }
    }

    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tmInfo);
    Serial.printf("Calendar Show Time: %s\n", buffer);

    nl_month_days(tmInfo.tm_year + 1900, tmInfo.tm_mon + 1, lunarDates);
    nl_year_jq(tmInfo.tm_year + 1900, jqAccDate);

    return true;
}

///////////// Screen //////////////
/**
 * 屏幕刷新
 */
void show_screen_task(void* param) {
    _screen_status = 0;
    Serial.println("show_screen, _screen_index: " + String(_screen_index));

    if (!get_datetime()) { // 准备时间日期信息
        Serial.println("ERR: System time prepare failed.");
        _screen_status = 2;
        SCREEN_HANDLER = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    int voltage = readBatteryVoltage();

    delay(100);

    Serial.println("[Task] screen update begin...");
    display.init(115200);          // 串口使能 初始化完全刷新使能 复位时间 ret上拉使能
    display.setRotation(ROTATION); // 设置屏幕旋转1和3是横向  0和2是纵向
    u8g2Fonts.begin(display);

    init_cal_layout_size();
    display.setFullWindow();
    display.firstPage();
    display.fillScreen(GxEPD_WHITE);
    do {
        if (_screen_index == 1 || _screen_index == 3 || _screen_index == 5) {
            drawEnglishWords(_screen_index);
        } else if (_screen_index == 2 || _screen_index == 4 || _screen_index == 6) {
            drawChineseWords(_screen_index);
        } else {
            _screen_index = 0;
            draw_cal_days();
            draw_cal_header();
        }

        draw_cal_year();

        if (api_info_status() == 1) {
            draw_weather();
        } else {
            draw_err("获取天气数据失败");
        }
        if (voltage > 3000 && voltage < 4300) {
            draw_battery(voltage);
        }
    } while (display.nextPage());
    
    Preferences pref;
    pref.begin(PREF_NAMESPACE);
    pref.putInt(PREF_SI_TYPE, _screen_index);
    pref.end();

    display.powerOff();
    display.hibernate();
    Serial.println("[Task] screen update end...");

    _screen_status = 1;
    SCREEN_HANDLER = NULL;
    vTaskDelete(NULL);
}

void show_screen(int screen_index) {

    if (SCREEN_HANDLER != NULL) {
        vTaskDelete(SCREEN_HANDLER);
    }
    _screen_index = screen_index;
    xTaskCreate(show_screen_task, "Screen", 4096, NULL, 2, &SCREEN_HANDLER);
}

int show_screen_status() {
    // -1: 初始化 0: 显示中 1: 显示成功 2: 显示失败
    return _screen_status;
}

void si_warning(const char* str) {
    Serial.println("Screen warning...");
    display.init(115200);          // 串口使能 初始化完全刷新使能 复位时间 ret上拉使能
    display.setRotation(ROTATION); // 设置屏幕旋转1和3是横向  0和2是纵向
    u8g2Fonts.begin(display);

    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    do {
        u8g2Fonts.setFontMode(1);
        u8g2Fonts.setFontDirection(0);
        u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);

        u8g2Fonts.setFont(u8g2_font_open_iconic_all_4x_t);
        int space = 8;
        int w = u8g2Fonts.getUTF8Width("\u0118") + space;
        u8g2Fonts.setFont(FONT_TEXT);
        w += u8g2Fonts.getUTF8Width(str);

        u8g2Fonts.setForegroundColor(GxEPD_RED);
        u8g2Fonts.setFont(u8g2_font_open_iconic_all_4x_t);
        u8g2Fonts.setCursor((display.width() - w) / 2, (display.height() + 32) / 2);
        u8g2Fonts.print("\u0118");

        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        u8g2Fonts.setCursor(u8g2Fonts.getCursorX() + space, u8g2Fonts.getCursorY() - 8);
        u8g2Fonts.setFont(FONT_TEXT);
        u8g2Fonts.print(str);
    } while (display.nextPage());

    display.powerOff();
    display.hibernate();
}