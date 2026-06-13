#include "stdafx.h"
#include "global_container.h"
#include "duration_database.h"
#include "GUID.h"
// 当 foobar 重新读取文件信息时（例如刷新元数据），会用文件里的原始时长覆盖掉我们的自定义时长；
// 本过滤器挂在读取信息的链路上，命中数据库中有记录的音轨就「再次」把时长改回自定义值，从而保持设置不被冲掉。
// When foobar re-reads file info (e.g. on a metadata refresh), it would overwrite our custom length with the file's original one;
// this filter sits on the info-read path and, for tracks present in our DB, re-applies the custom length so the setting isn't clobbered.
class duration_info_filter : public input_info_filter {
public:
	void filter_info_read(const playable_location& loc, file_info& info, abort_callback& abort) override {
		pfc::string8 hash = duration_db::content_hasher(loc.get_path(), loc.get_subsong());
        if (song_container.contains(hash)) {
            info.set_length(song_container[hash].custom_duration);
        }
	}
    bool filter_info_write(const playable_location&, file_info&, abort_callback&) override {
        return true;
    }

    void on_info_remove(const char*, abort_callback&) override {}

    GUID get_guid() override { return guid_info_filter; }

    GUID get_preferences_guid() override { return guid_prefs_duration; }

    const char* get_name() override { return "Duration Manager"; }

    bool supports_fallback() override { return false; }

    bool write_fallback(const playable_location&, const file_info&, abort_callback&) override {
        return false;
    }

    void remove_tags_fallback(const char*, abort_callback&) override {}
};

static service_factory_single_t<duration_info_filter> g_duration_info_filter;