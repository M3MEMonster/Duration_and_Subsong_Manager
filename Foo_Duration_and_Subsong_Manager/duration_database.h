#pragma once
#include "stdafx.h"

// =============================================================================
// 「自定义时长」功能由三条相互独立的机制共同保证，分散在不同文件，这里给出全局视角：
//   1) refresh_metadb（本文件 / duration_database.cpp）—— 启动加载或用户编辑时，通过 metadb hints 把时长写进元数据库。
//   2) duration_info_filter（info_filter.cpp）—— 当 foobar 重新读取文件信息时再次覆盖时长，防止被原始信息冲掉。
//   3) duration_playback_monitor（playback_monitor.cpp）—— 播放时定时监控位置，到达自定义时长就切下一首，实现「截断」效果。
// 每个音轨在数据库中的唯一标识由 content_hasher 生成（基于文件内容 + subsong，详见其实现）。
//
// The "custom duration" feature is upheld by three independent mechanisms spread across files; here's the big picture:
//   1) refresh_metadb (this file / duration_database.cpp) - on load or user edit, writes the length into the metadb via hints.
//   2) duration_info_filter (info_filter.cpp) - re-applies the length whenever foobar re-reads file info, so it isn't overwritten.
//   3) duration_playback_monitor (playback_monitor.cpp) - during playback, polls the position and skips to the next track at the custom length ("truncation").
// Each track's unique DB key is produced by content_hasher (based on file content + subsong; see its implementation).
// =============================================================================
namespace duration_db {
    struct song_item {
        pfc::string8 hash_key;
        pfc::string8 file_path;
        pfc::string8 file_name;
        pfc::string8 track_title;
        uint32_t subsong = 0;
        double original_duration = 0;
        double custom_duration = 0;
    };

    pfc::string8 content_hasher(const char* path, uint32_t subsong);
    void save();
    void load();
    void refresh_metadb(metadb_hint_list_v3::ptr& hints, const song_item* item, char mode);
}


