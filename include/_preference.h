#ifndef ___PREFERENCE_H__
#define ___PREFERENCE_H__

#include <Preferences.h>
#define PREF_NAMESPACE "J_CALENDAR"

// Preferences KEY定义
// !!!preferences key限制15字符
#define PREF_SI_CAL_DATE "SI_CAL_DATE" // 屏幕当前显示的日期
#define PREF_SI_TYPE "SI_TYPE" // 屏幕显示类型

#define PREF_QWEATHER_HOST "QWEATHER_HOST" // QWEATHER HOST
#define PREF_QWEATHER_LOC "QWEATHER_LOC" // 地理位置

// 假期信息，tm年，假期日(int8)，假期日(int8)...
#define PREF_HOLIDAY "HOLIDAY"

#endif
