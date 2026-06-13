#include "stdafx.h"
#include "MinHook.h"
#include <unordered_set>
#include <mutex>
#include <string>
#include "global_container.h"
//Hook for sqlite functions during runtime to hide subsong item from database query
// foobar 的媒体库底层用 sqlite 存储。本文件通过 MinHook 在运行时挂钩 sqlite3 的若干函数，
// 拦截媒体库查询 "SELECT name FROM media" 的结果，把用户标记为隐藏的子歌曲从结果中「吃掉」，
// 使它们不出现在自动播放列表等处。
// foobar's media library is backed by sqlite. This file uses MinHook to hook sqlite3 functions at runtime,
// intercepting the result rows of the media-library query "SELECT name FROM media" and silently dropping
// any subsongs the user marked as hidden, so they no longer show up (e.g. in autoplaylists).
namespace {
	struct sqlite3;
	struct sqlite3_stmt;
	using sqlite3_prepare_v2_t = int(__cdecl*)(sqlite3* db, const char* zSql, int nByte, sqlite3_stmt** ppStmt, const char** pzTail);
	using sqlite3_step_t = int(__cdecl*)(sqlite3_stmt* stmt);
	using sqlite3_column_text_t = const unsigned char* (__cdecl*)(sqlite3_stmt*, int iCol);
	using sqlite3_finalize_t = int(__cdecl*)(sqlite3_stmt* pStmt);
	static sqlite3_prepare_v2_t original_sqlite3_prepare_v2 = nullptr;
	static sqlite3_step_t original_sqlite3_step = nullptr;
	static sqlite3_column_text_t sqlite3_column_text = nullptr;
	static sqlite3_finalize_t original_sqlite3_finalize = nullptr;
	constexpr int SQLITE_ROW = 100;
	constexpr int SQLITE_DONE = 101;
	std::unordered_set<sqlite3_stmt*> media_stmt;
	std::mutex media_stmt_mutex;

	int __cdecl my_sqlite3_step(sqlite3_stmt* stmt) {
		// label + goto 构成一个「跳过当前行」的重试循环：若当前行对应的子歌曲被设为隐藏，
		// 就再调一次 original_sqlite3_step 取下一行，对调用方而言被隐藏的行就像不存在。
		// label + goto form a "skip this row" retry loop: if the current row's subsong is hidden,
		// we call original_sqlite3_step again to fetch the next row, so hidden rows appear nonexistent to the caller.
	label:
		int ret;
		ret = original_sqlite3_step(stmt);
		bool is_media_stmt = false;
		{
			std::lock_guard<std::mutex> lock(media_stmt_mutex);
			is_media_stmt = media_stmt.find(stmt) != media_stmt.end();
		}
		if (is_media_stmt && ret == SQLITE_ROW) {
			const unsigned char* text = sqlite3_column_text(stmt, 0);
			int subsong_index = 0;
			if (text) {
				// column 0 文本格式为 "<subsong索引>+<URL文件路径>"，例如 "0+file://C:\music\a.flac"。
				// 下面先解析 '+' 之前的前导数字作为 subsong 索引（*text - 48 即减去 '0'），'+' 之后即文件路径。
				// column 0 text format is "<subsongIndex>+<filePath>", e.g. "0+file://C:\music\a.flac".
				// Parse the leading digits before '+' as the subsong index (*text - 48 == subtract '0'); the rest is the path.
				for (text; *text != '+'; text++) {
					subsong_index = subsong_index * 10 + (*text - 48);
				}
				text++;
				pfc::string8 file_path(reinterpret_cast<const char*>(text));
				_temp_container[file_path].insert(subsong_index);
				// 不同解码器的 subsong 索引有的从 0 开始、有的从 1 开始，因此查 subsong_filter 时要按 is0base 决定是否减 1。
				// Some decoders index subsongs from 0, others from 1, so adjust by is0base (subtract 1) when indexing subsong_filter.
				if (mul_subsong_filter.exists(file_path)) {
					if (mul_subsong_filter[file_path].is0base) {
						if (!mul_subsong_filter[file_path].subsong_filter[subsong_index]) goto label;
					}
					else {
						if (!mul_subsong_filter[file_path].subsong_filter[subsong_index - 1]) goto label;
					}
				}
			}

		}
		return ret;
	}
	int __cdecl my_sqlite3_prepare_v2(sqlite3* db, const char* zSql, int nByte, sqlite3_stmt** ppStmt, const char** pzTail) {
		int ret = original_sqlite3_prepare_v2(db, zSql, nByte, ppStmt, pzTail);
		// 只标记精确匹配 "SELECT name FROM media" 的语句（记入 media_stmt 集合）；
		// my_sqlite3_step 仅对这些被标记的 statement 生效，避免误伤其他查询。
		// Only tag statements that exactly match "SELECT name FROM media" (recorded in media_stmt);
		// my_sqlite3_step only acts on these tagged statements, so other queries are left untouched.
		if (zSql && strcmp(zSql, "SELECT name FROM media") == 0) {
			{
				std::lock_guard<std::mutex> lock(media_stmt_mutex);
				media_stmt.emplace(*ppStmt);
			}
		}
		return ret;
	}

	int __cdecl my_sqlite3_finalize(sqlite3_stmt* pStmt) {
		int ret;
		{
			std::lock_guard<std::mutex> lock(media_stmt_mutex);
			media_stmt.erase(pStmt);
		}
		ret = original_sqlite3_finalize(pStmt);
		return ret;
	}
}




namespace subsong_db {
	void load() {
		pfc::string8 db_path = core_api::pathInProfile("subsong_db.dat");
		service_ptr_t<file> f{ nullptr };
		if (filesystem::g_exists(db_path, fb2k::noAbort)) {
			filesystem::g_open(f, db_path, filesystem::open_mode_read, fb2k::noAbort);
			pfc::array_t<t_uint8> buffer;
			const t_filesize size = f->get_size(fb2k::noAbort);
			buffer.set_size(size);
			f->read_object(buffer.get_ptr(), buffer.get_size(), fb2k::noAbort);
			stream_reader_formatter_simple_ref<> reader(buffer);
			while (reader.get_remaining() > 0) {
				subsong_list su_item;
				size_t length;
				reader >> su_item.file_path >> length;
				su_item.subsong_filter.assign(length, false);
				for (size_t i = 0; i < length; i++) {
					reader >> su_item.subsong_filter[i];
				}
				reader >> su_item.is0base;
				mul_subsong_filter[su_item.file_path] = su_item;
			}

		}
	}
	void save() {
		pfc::string8 su_db_path = core_api::pathInProfile("subsong_db.dat");
		stream_writer_formatter_simple<> writer;
		service_ptr_t<file> f{ nullptr };
		filesystem::g_open(f, su_db_path, filesystem::open_mode_write_new, fb2k::noAbort);
		for (auto& [key, item] : mul_subsong_filter) {
			writer << key << item.subsong_filter.size();
			for (size_t i = 0; i < item.subsong_filter.size(); i++) {
				writer << item.subsong_filter[i];
			}
			writer << item.is0base;
		}
		f->write_object(writer.m_buffer.get_ptr(), writer.m_buffer.get_size(), fb2k::noAbort);
	}
	void install_hook() {
		MH_Initialize();
		auto sqlite = GetModuleHandleA("sqlite3.dll");
		auto prepare = GetProcAddress(sqlite, "sqlite3_prepare_v2");
		auto step = GetProcAddress(sqlite, "sqlite3_step");
		auto col_text = GetProcAddress(sqlite, "sqlite3_column_text");
		auto final = GetProcAddress(sqlite, "sqlite3_finalize");
		// 注意：sqlite3_column_text 不做 hook，只是保存原始函数指针供 my_sqlite3_step 直接调用读取列文本；
		// 只有 prepare_v2、step、finalize 三个函数。
		// Note: sqlite3_column_text is NOT hooked — we just keep its original pointer to read column text in my_sqlite3_step;
		// only prepare_v2 / step / finalize are actually replaced by MinHook.
		sqlite3_column_text = reinterpret_cast<sqlite3_column_text_t>(col_text);
		MH_CreateHook(reinterpret_cast<LPVOID>(prepare), &my_sqlite3_prepare_v2, reinterpret_cast<LPVOID*>(&original_sqlite3_prepare_v2));
		MH_CreateHook(reinterpret_cast<LPVOID>(step), &my_sqlite3_step, reinterpret_cast<LPVOID*>(&original_sqlite3_step));
		MH_CreateHook(reinterpret_cast<LPVOID>(final), &my_sqlite3_finalize, reinterpret_cast<LPVOID*>(&original_sqlite3_finalize));
		MH_EnableHook(MH_ALL_HOOKS);
	}
	void uninstall_hook() {
		MH_DisableHook(MH_ALL_HOOKS);
		MH_Uninitialize();
	}
}


