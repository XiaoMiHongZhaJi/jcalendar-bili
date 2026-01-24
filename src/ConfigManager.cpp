#include <Preferences.h>
#include "_preference.h"

/* -------------------- ConfigManager --------------------
   用来管理所有可配置值，并支持：
   - load() 从 NVS 读入持久值
   - setTempXXX(...) 修改运行时临时值（不写 NVS）
   - setPersistentXXX(...) 立即修改并写入 NVS
   - getXXX() 返回生效值（优先临时 -> 持久 -> 默认）
*/
struct Config {
    String qweather_host;
    String qweather_loc;
    int screen_index;        // 默认页面索引
    unsigned long time_to_sleep_ms; // ms，配置为方便使用
    // 如果后续添加更多配置项，可以在这里扩展
};

class ConfigManager {
public:
    void begin(const char* ns = PREF_NAMESPACE) {
        _ns = ns;
        loadFromNVS();
        // 初始化临时存储为空
        _hasTemp = false;
    }

    // 读取生效配置（临时优先）
    Config get() {
        if (_hasTemp) return _temp;
        return _nvs;
    }

    // --- 临时修改（仅 RAM，本次开机有效） ---
    void setTemp_qweather(const String& host, const String& loc) {
        _temp.qweather_host = host;
        _temp.qweather_loc = loc;
        _hasTemp = true;
    }
    void setTemp_screen_index(int idx) {
        _temp.screen_index = idx;
        _hasTemp = true;
    }
    void setTemp_time_to_sleep_ms(unsigned long ms) {
        _temp.time_to_sleep_ms = ms;
        _hasTemp = true;
    }

    // --- 持久修改（写入 Preferences 且立即生效） ---
    void setPersistent_qweather(const String& host, const String& loc) {
        Preferences p;
        p.begin(_ns, false);
        p.putString(PREF_QWEATHER_HOST, host);
        p.putString(PREF_QWEATHER_LOC, loc);
        p.end();
        // 同步到运行时持久副本（立即生效）
        _nvs.qweather_host = host;
        _nvs.qweather_loc = loc;
    }

    void setPersistent_screen_index(int idx) {
        Preferences p;
        p.begin(_ns, false);
        p.putInt(PREF_SI_TYPE, idx);
        p.end();
        _nvs.screen_index = idx;
    }

    void setPersistent_time_to_sleep_ms(unsigned long ms) {
        Preferences p;
        p.begin(_ns, false);
        p.putULong(PREF_TIME_TO_SLEEP_MS, ms);
        p.end();
        _nvs.time_to_sleep_ms = ms;
    }

    // 清除 NVS（例如在长按配置里用到）
    void clearPersistent() {
        Preferences p;
        p.begin(_ns, false);
        p.clear();
        p.end();
        // reload defaults
        loadFromNVS(); // will reapply defaults
    }

    // 直接从 NVS 读取（用于 startup）
    void loadFromNVS() {
        Preferences p;
        p.begin(_ns, true); // read-only
        // 如果 NVS 中没有值，使用默认值
        String host = p.getString(PREF_QWEATHER_HOST, "192.168.10.225:5000");
        String loc = p.getString(PREF_QWEATHER_LOC, "101021600");
        int si = p.getInt(PREF_SI_TYPE, 0);
        unsigned long tts = p.getULong(PREF_TIME_TO_SLEEP_MS, 180UL * 1000UL); // 默认 180s

        p.end();

        _nvs.qweather_host = host;
        _nvs.qweather_loc = loc;
        _nvs.screen_index = si;
        _nvs.time_to_sleep_ms = tts;
    }

private:
    const char* _ns = PREF_NAMESPACE;
    Config _nvs;   // 来自持久化的值
    Config _temp;  // 临时覆盖
    bool _hasTemp = false;
};