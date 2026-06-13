#include "stdafx.h"
#include "resource.h"
#include <helpers/atl-misc.h>
#include <helpers/DarkMode.h>
#include <libPPUI/CListControlOwnerData.h>
#include <pfc/sortstring.h>
#include <pfc/SmartStrStr.h>
#include "duration_database.h"
#include "global_container.h"
#include "GUID.h"

class CDurationPreferences;

namespace {
	CDurationPreferences* g_open_prefs = nullptr;
}


class CDurationPreferences : public CDialogImpl<CDurationPreferences>, public preferences_page_instance, public IListControlOwnerDataSource {
private:
	class song_item_with_key : public duration_db::song_item {
	public:
		pfc::sortString_t file_name_sortkey;
		pfc::sortString_t track_title_sortkey;
		explicit song_item_with_key(const duration_db::song_item& base) : duration_db::song_item(base),
			file_name_sortkey(pfc::makeSortString(base.file_name)),
			track_title_sortkey(pfc::makeSortString(base.track_title)) {}
		// 由于pfc::sotString_t为unique_ptr，本结构仅允许移动
		//Since pfc::sotString_t is unique_ptr, this class can only be MOVED
		song_item_with_key(const song_item_with_key&) = delete;
		song_item_with_key& operator=(const song_item_with_key&) = delete;
		song_item_with_key(song_item_with_key&&) noexcept = default;
		song_item_with_key& operator=(song_item_with_key&&) noexcept = default;
	};
	preferences_page_callback::ptr m_callback;
	fb2k::CDarkModeHooks m_dark;
	CListControlOwnerData m_pref_list;
	std::vector<song_item_with_key> m_song_list;
	std::vector<duration_db::song_item> candidate_list;
	std::vector<duration_db::song_item> pending_delete_list;
	std::vector<size_t> visible_index;
	
	
public:
	CDurationPreferences(preferences_page_callback::ptr callback) : m_callback(callback),m_pref_list(this) {};
	enum {IDD=IDD_DURATION_MANAGER}; //Use which "blueprint" in resource.h
	
	t_uint32 get_state() {
		t_uint32 state=preferences_state::dark_mode_supported;
		if (HasChanged()) state |= preferences_state::changed;
		return state;
	}
	// 点击「应用」时统一提交两类挂起改动：
	// pending_delete_list —— 删除条目，并用 'R' 把时长还原成原始值；
	// candidate_list      —— 写入用户编辑后的自定义时长，并用 'C' 覆盖。
	// 最后统一 on_done() 提交 hints 并落盘保存。
	// On Apply, commit the two kinds of pending changes together:
	// pending_delete_list -> remove entries and restore original length via 'R';
	// candidate_list      -> write the user-edited custom length and override via 'C'.
	// Finally on_done() submits the hints and the DB is persisted.
	void apply() {
		metadb_hint_list_v3::ptr hints = metadb_hint_list_v3::create();
		for (duration_db::song_item del_item : pending_delete_list) {
			song_container.remove(del_item.hash_key);
			duration_db::refresh_metadb(hints, &del_item, 'R');
		}
		for (const duration_db::song_item& c_item : candidate_list) {
			song_container[c_item.hash_key].custom_duration = c_item.custom_duration;
			duration_db::refresh_metadb(hints, &c_item, 'C');
		}
		hints->on_done();
		duration_db::save();
		candidate_list.clear();
		pending_delete_list.clear();
		OnChanged();
	}
	void reset() {
		//NOT SUPPORT
	}
	void OnChanged() {
		m_callback->on_state_changed();
	}
	bool HasChanged() {
		return !candidate_list.empty() || !pending_delete_list.empty();
	}
	void DataReload() {
		candidate_list.clear();
		pending_delete_list.clear();
		get_data();
		m_pref_list.ReloadData();
		OnChanged();
	}

	BEGIN_MSG_MAP_EX(CDurationPreferences)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_CONTEXTMENU(OnContextMenu)
		COMMAND_HANDLER_EX(IDC_SEARCH, EN_CHANGE, OnSearchChange)
		MSG_WM_DESTROY(OnDestroy)
	END_MSG_MAP()
private:
	BOOL OnInitDialog(CWindow, LPARAM) {
		g_open_prefs = this;
		m_pref_list.CreateInDialog(*this, IDC_DURATION_LIST);
		m_dark.AddDialogWithControls(*this);
		auto dpi = m_pref_list.GetDPI();
		m_pref_list.AddColumn("File Name", MulDiv(180, dpi.cx, 96));
		m_pref_list.AddColumn("Track Title", MulDiv(180, dpi.cx, 96));
		m_pref_list.AddColumn("Subsong", MulDiv(50, dpi.cx, 96));
		m_pref_list.AddColumnAutoWidth("Duration");
		get_data();
		m_pref_list.ReloadData();
		return FALSE;
	}
	void OnDestroy() {
		m_song_list.clear();
		visible_index.clear();
		candidate_list.clear();
		pending_delete_list.clear();
		g_open_prefs = nullptr;
	}
	void OnContextMenu(CWindow wnd, CPoint point) {
		if (wnd != m_pref_list) return;

		point = m_pref_list.GetContextMenuPoint(point);

		CMenu menu;
		WIN32_OP_D(menu.CreatePopupMenu());

		enum {
			ID_EDIT_DURATION = 1,
			ID_DELETE_ITEMS
		};

		const bool hasSel = m_pref_list.GetSelectedCount() > 0;
		const bool oneSel = m_pref_list.GetSelectedCount() == 1;

		menu.AppendMenu(MF_STRING | (oneSel ? 0 : MF_GRAYED), ID_EDIT_DURATION, L"Edit duration");
		menu.AppendMenu(MF_STRING | (hasSel ? 0 : MF_GRAYED), ID_DELETE_ITEMS, L"Delete");
		int cmd = menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, point.x, point.y, *this, nullptr);

		switch (cmd) {
		case ID_EDIT_DURATION:
			if (oneSel) {
				m_pref_list.TableEdit_Start(m_pref_list.GetSelectionStart(), 3);
			}
			break;

		case ID_DELETE_ITEMS:
			if (hasSel) {
				m_pref_list.RequestRemoveSelection();
			}
			break;
		}
	}
	void OnSearchChange(UINT, int, CWindow) {
		CString text;
		GetDlgItemText(IDC_SEARCH, text);
		reload_filter(text);
		m_pref_list.ReloadData();
	}
	size_t listGetItemCount(ctx_t) override {
		return visible_index.size();
	}
	pfc::string8 listGetSubItemText(ctx_t, size_t row, size_t col) override {
		auto& it = m_song_list[visible_index[row]];
		switch (col) {
		case 0: return it.file_name;
		case 1: return it.track_title;
		case 2: return pfc::string8(pfc::format_uint(it.subsong));
		case 3: return pfc::format_time_ex(it.custom_duration, 3);
		default: return "";
		}
	}
	bool listIsColumnEditable(ctx_t, size_t col) override {
		return col == 3;
	}
	void listItemAction(ctx_t ctx, size_t row) override {
		auto* list = const_cast<CListControlOwnerData*>(ctx);
		list->TableEdit_Start(row, 3);
	}
	// 列表删除选中项时做三件事：
	// 1) 把被删项加入 pending_delete_list（真正还原时长发生在 apply 时）；
	// 2) 若该项之前被编辑过，从 candidate_list 中移除其待写入记录；
	// 3) 重建 m_song_list，只保留未删除项，然后按当前搜索条件刷新可见列表。
	// 注意 mask 的下标是「可见行」，需经 visible_index 映射回 m_song_list 的真实下标。
	// Removing selected rows does three things:
	// 1) push removed items into pending_delete_list (the actual length restore happens at apply);
	// 2) if an item was previously edited, drop its pending write from candidate_list;
	// 3) rebuild m_song_list keeping only non-removed items, then refresh the visible list by the current search.
	// Note: mask is indexed by VISIBLE row, so it must be mapped back to the real m_song_list index via visible_index.
	bool listRemoveItems(ctx_t, pfc::bit_array const& mask) override {
		std::vector<song_item_with_key> kept;
		std::vector<bool> remove_real(m_song_list.size(), false);
		for (size_t row = 0; row < visible_index.size(); row++) {
			if (mask[row]) {
				size_t real = visible_index[row];
				remove_real[real] = true;
				pfc::string8 hk = m_song_list[real].hash_key;
				pending_delete_list.emplace_back(m_song_list[real]);
				candidate_list.erase(std::remove_if(candidate_list.begin(), candidate_list.end(), [hk](const duration_db::song_item& it) {return it.hash_key == hk; }), candidate_list.end());
			}
		}
		for (size_t i = 0; i < remove_real.size(); i++) {
			if (!remove_real[i]) {
				kept.emplace_back(std::move(m_song_list[i]));
			}
			
		}
		m_song_list = std::move(kept);
		CString text;
		GetDlgItemText(IDC_SEARCH, text);
		reload_filter(text);
		m_pref_list.ReloadData();
		OnChanged();
		return true;
	}
	void listSetEditField(ctx_t, size_t row, size_t col, const char* val) override {
		if (col != 3) return;
		double sec = -1;
		try {
			sec = pfc::parse_timecode(val);
		}
		catch (...) { return; }
		if (sec < 0 || pfc::parse_timecode(pfc::format_time_ex(m_song_list[visible_index[row]].custom_duration, 3)) == sec) return;
		m_song_list[visible_index[row]].custom_duration = sec;
		candidate_list.emplace_back(m_song_list[visible_index[row]]);
		OnChanged();
	}
	static int song_compare(const song_item_with_key& song_a, const song_item_with_key& song_b) {
		int ret = pfc::sortStringCompare(song_a.file_name_sortkey, song_b.file_name_sortkey);
		if (ret == 0) {
			ret = pfc::sgn_t((t_uint32)song_a.subsong - (t_uint32)song_b.subsong);
			if (ret == 0) {
				ret = pfc::sortStringCompare(song_a.track_title_sortkey, song_b.track_title_sortkey);
			}
		}
		return ret;
	}
	void get_data() {
		CString text;
		GetDlgItemText(IDC_SEARCH, text);
		m_song_list.clear();
		for (auto& [key, s_item] : song_container) {
			song_item_with_key s_item_wkey(s_item);
			m_song_list.emplace_back(std::move(s_item_wkey));
		}
		pfc::sort_t(m_song_list, song_compare, m_song_list.size());
		reload_filter(text);
	}
	void reload_filter(const CString& text) {
		visible_index.clear();
		pfc::string8 search_query = pfc::stringcvt::string_utf8_from_wide(text.GetString()).get_ptr();
		for (size_t i = 0; i < m_song_list.size(); i++) {
			if (item_filter(m_song_list[i], search_query)) {
				visible_index.emplace_back(i);
			}
		}
	}
	static bool item_filter(const duration_db::song_item& it, const char* q) {
		if (!q || !*q) return true;
		pfc::string8 blob;
		blob << it.file_name << " " << it.track_title;
		SmartStrFilter filter(q);
		return filter.test(blob);
	}
};
//由于foobar2000允许在Preferences打开的情况下继续操作播放列表，故写了该函数以能够在窗口已经打开的状态下刷新preferences page
//Since fb2k allows users do actions while the preferences page opens, this function is written to refresh preferences page even it has been opend
void duration_pref_refresh() {
	fb2k::inMainThread([] {
		if (g_open_prefs != nullptr) {
			g_open_prefs->DataReload();
		}
		});
}
static preferences_branch_factory g_prefs_branch(guid_prefs_branch, preferences_page::guid_tools, "Duration & Subsong Manager");

class preferences_page_duration_impl : public preferences_page_impl<CDurationPreferences> {
	// preferences_page_impl<> helper deals with instantiation of our dialog; inherits from preferences_page_v3.
public:
	const char* get_name() { return "Duration Manager"; }
	GUID get_guid() { return guid_prefs_duration; }
	GUID get_parent_guid() { return guid_prefs_branch; }
};

static preferences_page_factory_t<preferences_page_duration_impl> g_preferences_page_duration_impl_factory;