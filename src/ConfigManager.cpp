#include "_preference.h"
#include <Preferences.h>

#define DEFAULT_API_HOST "192.168.10.225:5000"
#define DEFAULT_BACKUP_HOST "jcalendar.cyf.lol"
#define DEFAULT_QWEATHER_LOC "101180101"
#define ENABLE_WORDS true  // 是否打开单词功能
#define WORDS_PAGES      6 // 单词页面总数
#define IDLE_TO_SLEEP    5 // 页面刷新后进入休眠等待时间
#define FLUSH_WORDS     25 // 单词页面填充等待时间
#define FLUSH_CALENDAR 120 // 单词转到日历页面等待时间
#define PORTAL_TIMEOUT 180 // 配置超时时间

/* -------------------- ConfigManager --------------------*/
struct Config {
  String api_host;
  String backup_host;
  String qweather_loc;
  int screen_index;
  // 如果后续添加更多配置项，可以在这里扩展
};

class ConfigManager {
public:
  void begin() {
    loadFromNVS();
  }

  // 读取生效配置
  const Config& get() const {
    return _nvs;
  }

  void set_api_host(const String &api_host) {
    Preferences p;
    p.begin(PREF_NAMESPACE, false);
    if (p.getString(PREF_API_HOST) != api_host) {
      p.putString(PREF_API_HOST, api_host);
    }
    p.end();
    _nvs.api_host = api_host;
  }

  void set_backup_host(const String &backup_host) {
    Preferences p;
    p.begin(PREF_NAMESPACE, false);
    if (p.getString(PREF_BACKUP_HOST) != backup_host) {
      p.putString(PREF_BACKUP_HOST, backup_host);
    }
    p.end();
    _nvs.backup_host = backup_host;
  }

  void set_qweather_loc(const String &qweather_loc) {
    Preferences p;
    p.begin(PREF_NAMESPACE, false);
    if (p.getString(PREF_QWEATHER_LOC) != qweather_loc) {
      p.putString(PREF_QWEATHER_LOC, qweather_loc);
    }
    p.end();
    _nvs.qweather_loc = qweather_loc;
  }

  void set_screen_index(int idx) {
    Preferences p;
    p.begin(PREF_NAMESPACE, false);
    if (p.getInt(PREF_SI_TYPE) != idx) {
      p.putInt(PREF_SI_TYPE, idx);
    }
    p.end();
    _nvs.screen_index = idx;
  }

  // 清除 NVS（例如在长按配置里用到）
  void clearPersistent() {
    Preferences p;
    p.begin(PREF_NAMESPACE, false);
    p.clear();
    p.end();
    // reload defaults
    loadFromNVS(); // will reapply defaults
  }

  // 直接从 NVS 读取（用于 startup）
  void loadFromNVS() {
    Preferences p;
    p.begin(PREF_NAMESPACE, true); // read-only
    // 如果 NVS 中没有值，使用默认值
    String api_host = p.getString(PREF_API_HOST, DEFAULT_API_HOST);
    String backup_host = p.getString(PREF_BACKUP_HOST, DEFAULT_BACKUP_HOST);
    String qweather_loc = p.getString(PREF_QWEATHER_LOC, DEFAULT_QWEATHER_LOC);
    int screen_index = p.getInt(PREF_SI_TYPE, 0);

    _nvs.api_host = api_host;
    _nvs.backup_host = backup_host;
    _nvs.qweather_loc = qweather_loc;
    _nvs.screen_index = screen_index;
  }

private:
  Config _nvs;
};