#include "stdafx.h"
#include "duration_database.h"
#include "global_container.h"
namespace duration_db {


	// 为每个音轨生成稳定的唯一标识 key（两段式哈希）。
	// 第一段：对文件「内容」做 MD5（而非用路径），这样文件被移动/改名后仍能匹配到同一条记录。
	// 第二段：把（内容哈希 或 路径回退值）+ 分隔符 + subsong 索引 再做一次 MD5，从而区分同一文件内的不同子歌曲。
	// Generates a stable, unique key for each track (two-stage hashing).
	// Stage 1: MD5 over the file CONTENT (not the path), so a moved/renamed file still maps to the same record.
	// Stage 2: MD5 over (content hash OR path fallback) + separator + subsong index, to distinguish subsongs within one file.
	pfc::string8 content_hasher(const char* path, uint32_t subsong) {
        pfc::string8 contentHash;
        bool contentOk = false;
        try {
            service_ptr_t<file> f;
            filesystem::g_open_read(f, path, fb2k::noAbort);

            auto hasher = hasher_md5::get();
            hasher_md5_state state;
            hasher->initialize(state);

            const size_t kChunk = 1 << 20;
            std::vector<uint8_t> buf(kChunk);
            for (;;) {
                t_size done = f->read(buf.data(), (t_size)buf.size(), fb2k::noAbort);
                if (done == 0) break;
                hasher->process(state, buf.data(), done);
            }
            contentHash = hasher->get_result(state).asString();
            contentOk = true;
        }
        catch (...) {
            contentOk = false;
        }

        auto hasher = hasher_md5::get();
        hasher_md5_state finalState;
        hasher->initialize(finalState);
        if (contentOk) {
            hasher->process(finalState, contentHash.c_str(), contentHash.length());
        }
        else {
            // 文件打不开时的回退：改用路径参与哈希，至少保证 key 仍然唯一可用。
            // Fallback for inaccessible files: hash the path instead so the key stays unique/usable.
            hasher->process(finalState, path, strlen(path));
        }
        // 用 '\0' 作为分隔符，防止前面的哈希/路径与下面的 subsong 字节粘连而产生碰撞。
        // '\0' acts as a separator so the preceding hash/path can't run into the subsong bytes and cause collisions.
        hasher->process(finalState, "\0", 1);
        // 把 subsong 的原始字节并入哈希：同一文件的不同子歌曲会得到不同的 key。
        // Mix in the raw subsong bytes: different subsongs of the same file get different keys.
        hasher->process(finalState, &subsong, sizeof(subsong));
        return hasher->get_result(finalState).toString();
	}
    void save() {
        pfc::string8 db_path = core_api::pathInProfile("duration_db.dat");
        stream_writer_formatter_simple<> writer;
        service_ptr_t<file> f{ nullptr };
        filesystem::g_open(f, db_path, filesystem::open_mode_write_new, fb2k::noAbort);
        for (auto& [key,item]:song_container) {
            writer << key << item.file_path << item.file_name << item.track_title << item.subsong << item.original_duration << item.custom_duration;
        }
        f->write_object(writer.m_buffer.get_ptr(), writer.m_buffer.get_size(), fb2k::noAbort);
    }
    void load() {
        pfc::string8 db_path = core_api::pathInProfile("duration_db.dat");
        service_ptr_t<file> f{ nullptr };
        metadb_hint_list_v3::ptr hints = metadb_hint_list_v3::create();;
        if (filesystem::g_exists(db_path, fb2k::noAbort)) {
            filesystem::g_open(f, db_path, filesystem::open_mode_read, fb2k::noAbort);
            pfc::array_t<t_uint8> buffer;
            const t_filesize size = f->get_size(fb2k::noAbort);
            buffer.set_size(size);
            f->read_object(buffer.get_ptr(), buffer.get_size(), fb2k::noAbort);
            stream_reader_formatter_simple_ref<> reader(buffer);
            while (reader.get_remaining() > 0) {
                song_item item{};
                pfc::string8 key{};
                reader >> key >> item.file_path >> item.file_name >> item.track_title >> item.subsong >> item.original_duration >> item.custom_duration;
                item.hash_key = key;
                song_container[key] = item;
            }
            // 只有用户真正改过时长（custom != original）的条目才需要把改动推回 metadb，未改过的跳过以节省开销。
            // Only push changes back to metadb for entries the user actually edited (custom != original); skip the rest.
            for (auto& [key, item] : song_container) {
                if (item.custom_duration != item.original_duration) {
                    refresh_metadb(hints, &item, 'C');
                }
            }
            hints->on_done();
        }
        
    }
    // 通过 metadb hints 把内存中的时长「强制覆盖」回 foobar 的元数据库。
    // mode 'C'(Custom) 写入自定义时长；mode 'R'(Recover) 写回原始时长（删除条目时用于还原）。
    // Force-overrides the track length in foobar's metadb via hints.
    // mode 'C'(Custom) writes the custom length; mode 'R'(Recover) restores the original length (used when deleting entries).
    void refresh_metadb(metadb_hint_list_v3::ptr& hints,const song_item* item, char mode) {
        metadb_handle_ptr handle;
        metadb_info_container::ptr oldInfoRef;
        static_api_ptr_t<metadb>()->handle_create(handle, make_playable_location(item->file_path, item->subsong));
        if (handle->get_info_ref(oldInfoRef)) {
            file_info_impl info(oldInfoRef->info());
            switch (mode) {
            case 'C': /*Custom*/
                info.set_length(item->custom_duration);
                break;
            case 'R': /*Recover*/
                info.set_length(item->original_duration);
                break;
            default:
                uBugCheck();
            }
            hints->add_hint_forced(handle, info, oldInfoRef->stats(), false);
        }
    }
}