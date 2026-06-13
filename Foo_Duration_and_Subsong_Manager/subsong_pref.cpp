#include "stdafx.h"
#include "resource.h"
#include "GUID.h"
#include "global_container.h"
#include <pfc/sortstring.h>
#include <helpers/atl-misc.h>
#include <helpers/DarkMode.h>
#include <libPPUI/CListControlOwnerData.h>
#include <libPPUI/CListControl-Cells.h>


class CSubsongPreferences : public CDialogImpl<CSubsongPreferences>, public preferences_page_instance, public IListControlOwnerDataSource, public IListControlOwnerDataCells {
private:
	preferences_page_callback::ptr m_callback;
	fb2k::CDarkModeHooks m_dark;
	CListControlOwnerData m_file_list;
	CListControlOwnerDataCells m_subsong_list;
	size_t m_file_sel = SIZE_MAX;
	std::vector<subsong_db::subsong_list> temp_filter;
	bool m_internal_update = false;

public:
	CSubsongPreferences(preferences_page_callback::ptr callback) : m_callback(callback), m_file_list(this), m_subsong_list(this, this) {};
	enum {IDD=IDD_SUBSONG_MANAGER};

	t_uint32 get_state() {
		t_uint32 state = preferences_state::dark_mode_supported;
		// needs_restart：隐藏过滤是靠 hook 媒体库查询生效的，改动需要重启后才会反映出来。
		// needs_restart: the hide filter works by hooking media-library queries, so changes only take effect after a restart.
		if (HasChanged()) state |= preferences_state::changed|preferences_state::needs_restart;
		return state;
	}
	void apply() {
		for (const subsong_db::subsong_list& item : temp_filter) {
			mul_subsong_filter[item.file_path] = item;
		}
		subsong_db::save();
		OnChanged();
	}
	void reset() {
		//NOT SUPPORT
	}
	void OnChanged() {
		m_callback->on_state_changed();
	}
	bool HasChanged() {
		for (subsong_db::subsong_list item : temp_filter) {
			if (item.subsong_filter != mul_subsong_filter[item.file_path].subsong_filter) return true;
		}
		return false;
	}

	BEGIN_MSG_MAP_EX(CSubsongPreferences)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_DESTROY(OnDestroy)
		COMMAND_HANDLER_EX(IDC_MODE_COMBO,CBN_SELCHANGE,OnModeChange)
		COMMAND_HANDLER_EX(IDC_RANGE_INPUT,EN_CHANGE,OnRangeChange)
	END_MSG_MAP()
private:
	BOOL OnInitDialog(CWindow, LPARAM) {
		m_file_list.CreateInDialog(*this, IDC_FILE_LIST);
		m_file_list.SetSelectionModeSingle();
		m_subsong_list.CreateInDialog(*this, IDC_SUBSONG_LIST);
		m_dark.AddDialogWithControls(*this);
		auto dpi = m_file_list.GetDPI();
		m_file_list.AddColumn("File Name", MulDiv(130, dpi.cx, 96));
		m_file_list.AddColumnAutoWidth("Subsongs");

		m_subsong_list.AddColumn("", MulDiv(24, dpi.cx, 96));
		m_subsong_list.AddColumn("Index", MulDiv(40, dpi.cx, 96));
		m_subsong_list.AddColumn("Name", MulDiv(800, dpi.cx, 96));

		CComboBox combo(GetDlgItem(IDC_MODE_COMBO));
		combo.AddString(L"Include");
		combo.AddString(L"Exclude");
		combo.SetCurSel(0);
		get_subsong();
		return FALSE;
	}
	void OnDestroy() {
		temp_filter.clear();
	}
	static int string_compare(const subsong_db::subsong_list& f_a, const subsong_db::subsong_list& f_b) {
		return pfc::sortStringCompare(pfc::makeSortString(f_a.file_path), pfc::makeSortString(f_b.file_path));
	}
	void OnModeChange(UINT, int, CWindow) {
		text_to_range();
	}
	void OnRangeChange(UINT, int, CWindow) {
		if (m_internal_update) return;
		text_to_range();
	}
	void get_subsong() {
		if (_temp_container.size() != 0) {
			for (auto const& [key, item] : _temp_container) {
				int subsong_count = item.size();
				if (subsong_count > 1) {
					subsong_db::subsong_list su_item{};
					su_item.file_path = key;
					su_item.subsong_filter.assign(subsong_count, true);
					// 推断索引是 0 基还是 1 基. Infer the index is 0-based or 1-based
					su_item.is0base = item.size() - *item.rbegin();/*Exmaple: 0-based {0,1,2}: size3 - max2 = 1 -> true; 1-based {1,2,3}: size3 - max3 = 0 -> false.*/
					if (mul_subsong_filter.exists(key)) {
						temp_filter.emplace_back(mul_subsong_filter[key]);
					}
					else {
						mul_subsong_filter[key] = su_item;
						temp_filter.emplace_back(su_item);
					}
					
				}
				
			}
			
		}
		pfc::sort_t(temp_filter, string_compare, temp_filter.size());
	}
	void listSelChanged(ctx_t ctx) override {
		if (ctx != &m_file_list) return;
		m_file_sel = m_file_list.GetSelectionStart();
		m_subsong_list.ReloadData();
		CComboBox combo(GetDlgItem(IDC_MODE_COMBO));
		combo.SetCurSel(0);
		range_to_text();
	}
	size_t listGetItemCount(ctx_t ctx) override {
		if (ctx == &m_file_list) {
			return temp_filter.size();
		}
		if (ctx == &m_subsong_list) {
			if (m_file_sel == SIZE_MAX || m_file_sel >= mul_subsong_filter.size()) return 0;
			return temp_filter[m_file_sel].subsong_filter.size();
		}
		return 0;
	}
	pfc::string8 listGetSubItemText(ctx_t ctx, size_t row, size_t col) override {
		if (ctx == &m_file_list) {
			auto& f = temp_filter[row];
			switch (col) {
			case 0: return fb2k::filename_ext(f.file_path);
			case 1: return pfc::string8(pfc::format_uint(f.subsong_filter.size()));
			default: return "";
			}
		}
		if (ctx == &m_subsong_list) {
			if (m_file_sel == SIZE_MAX || m_file_sel >= temp_filter.size()) return "";
			input_decoder::ptr dec;
			file_info_impl info;
			auto& s = temp_filter[m_file_sel];
			switch (col) {
			case 0: return "";
			case 1: 
				if (s.is0base) {
					return pfc::string8(pfc::format_uint(row));
				}
				else {
					return pfc::string8(pfc::format_uint(row + 1));
				}
			case 2:
				auto lock = file_lock_manager::get()->acquire_read(s.file_path, fb2k::noAbort);
				input_entry::g_open_for_decoding(dec, nullptr, s.file_path, fb2k::noAbort);
				int subsong_index = s.is0base ? row : row + 1;
				dec->get_info(subsong_index, info, fb2k::noAbort);
				auto title = info.meta_get("title", 0);
				if (title == nullptr) {
					return fb2k::filename_ext(s.file_path);
				}
				else {
					return title;
				}
			}
		}
		return "";
	}
	CListControl::cellType_t listCellType(cellsCtx_t ctx, size_t row, size_t col) override {
		if (ctx != &m_subsong_list) return nullptr;
		if (col == 0) return &PFC_SINGLETON(CListCell_Checkbox);
		return &PFC_SINGLETON(CListCell_Text);
	}
	bool listCellCheckState(cellsCtx_t ctx, size_t row, size_t col) override {
		if (ctx != &m_subsong_list || col != 0) return false;
		if (m_file_sel == SIZE_MAX || m_file_sel >= temp_filter.size()) return false;
		auto& s = temp_filter[m_file_sel];
		return s.subsong_filter[row];
	}
	void listCellSetCheckState(cellsCtx_t ctx, size_t row, size_t col, bool state) override {
		if (ctx != &m_subsong_list || col != 0) return;
		if (m_file_sel == SIZE_MAX || m_file_sel >= temp_filter.size()) return;
		auto& s = temp_filter[m_file_sel];
		s.subsong_filter[row] = state;
		range_to_text();
		OnChanged();
	}
	// 把 bool 过滤数组转换成区间文本（如 "1-3,5,7"，无空格）。
	// Converts the bool filter array into range text (e.g. "1-3,5,7", no space).
	void range_to_text() {
		m_internal_update = true;
		if (m_file_sel == SIZE_MAX || m_file_sel >= mul_subsong_filter.size()) {
			SetDlgItemText(IDC_RANGE_INPUT, _T(""));
			CComboBox(GetDlgItem(IDC_MODE_COMBO)).SetCurSel(0);
			return;
		}
		subsong_db::subsong_list s = temp_filter[m_file_sel];
		CString mode;
		pfc::string8 range;
		int index = 0;
		int index_2 = 0;
		GetDlgItemText(IDC_MODE_COMBO, mode);
		if (mode == L"Exclude") {
			for (size_t i = 0; i < s.subsong_filter.size(); i++) {
				s.subsong_filter[i] = !s.subsong_filter[i];
			}
		}
		uint8_t* su_ptr = s.subsong_filter.data();
		uint8_t* su_ptr_2 = nullptr;
		uint8_t* end = s.subsong_filter.data() + s.subsong_filter.size();
	loop:
		while (su_ptr != end && *su_ptr == 0) {
			su_ptr++;
			index++;
		}
		//Record start position of 1
		if (su_ptr == end) goto finish;
		if (s.is0base) {
			range << pfc::format_uint(index);
		}
		else {
			range << pfc::format_uint(index + 1);
		}
		//To see whether the length of 1 is larger than 1. If yes, record the end position of 1
		index_2 = index;
		while (su_ptr != end && *su_ptr == 1) {
			su_ptr++;
			index_2++;
		}
		if (index_2 - index != 1) {
			if (s.is0base) {
				range << "-" << pfc::format_uint(index_2 - 1);
			}
			else {
				range << "-" << pfc::format_uint(index_2);
			}
		}
		index = index_2;
		if (su_ptr == end) goto finish;
		//To see if this chunk of 1 is the last chunk. If not, add a comma
		su_ptr_2 = su_ptr;
		while (su_ptr_2 != end && *su_ptr_2 == 0) {
			su_ptr_2++;
		}
		if (su_ptr_2 != end) {
			range << ",";
		}
		goto loop;
	finish:
		SetDlgItemText(IDC_RANGE_INPUT, pfc::stringcvt::string_wide_from_utf8(range.c_str()).get_ptr());
		m_internal_update = false;
	}
	// range_to_text 的逆操作
	// Inverse of range_to_text
	void text_to_range() {
		if (m_file_sel == SIZE_MAX || m_file_sel >= temp_filter.size()) return;
		auto& s = temp_filter[m_file_sel];
		CString text;
		CString mode;
		GetDlgItemText(IDC_RANGE_INPUT, text);
		pfc::string8 text_p8 = pfc::stringcvt::string_utf8_from_wide(text.GetString()).get_ptr();
		pfc::chain_list_v2_t<pfc::string8> out;
		std::vector<uint8_t> mask;
		mask.assign(s.subsong_filter.size(), false);
		if (text_p8.isEmpty()) goto end;
		//若包含非法字符则回退
		//return if contains illegal char
		{
			auto isAllowed = [](char c) {
				return (c >= '0' && c <= '9') || c == '-' || c == ',';
				};
			for (size_t i = 0; i < text_p8.length(); ++i) {
				if (!isAllowed(text_p8[i])) return;
			}
		}
		
		pfc::splitStringByChar(out, text_p8, ',');
		for (auto& id_str : out) {
			if (id_str.contains('-')) {
				std::vector<size_t> se;
				pfc::chain_list_v2_t<pfc::string8> out_2;
				pfc::splitStringByChar(out_2, id_str, '-');
				for (auto& num_str : out_2) {
					se.emplace_back(pfc::atoui_ex(num_str.c_str(), num_str.length()));
				}
				//数字异常或格式异常则回退
				//return if the number or format is abnormal
				if (se.size() != 2 || se[0] > se[1] || se[0] > mask.size() || se[1] > mask.size()) return;
				
				if (!s.is0base) {
					if (se[0] == 0 || se[1] == 0) return;
					std::fill(mask.begin() + se[0]-1, mask.begin() + se[1], true);
				}
				else {
					std::fill(mask.begin() + se[0], mask.begin() + se[1] + 1, true);
				}
				
			}
			else {
				size_t i_num = pfc::atoui_ex(id_str.c_str(), id_str.length());
				if (!s.is0base) {
					if (i_num == 0 || i_num > mask.size()) return;
					mask[i_num - 1] = true;
				}
				else {
					if (i_num >= mask.size()) return;
					mask[i_num] = true;
				}
			}
		}
	end:
		GetDlgItemText(IDC_MODE_COMBO, mode);
		if (mode == L"Exclude") {
			for (size_t i = 0; i < mask.size(); i++) {
				mask[i] = !mask[i];
			}
		}
		if (mask != s.subsong_filter) {
			s.subsong_filter = mask;
			m_subsong_list.ReloadData();
			OnChanged();
		}
	}
};


class preferences_page_subsong_impl : public preferences_page_impl<CSubsongPreferences> {
	// preferences_page_impl<> helper deals with instantiation of our dialog; inherits from preferences_page_v3.
public:
	const char* get_name() { return "Subsong Manager"; }
	GUID get_guid() { return guid_prefs_subsong; }
	GUID get_parent_guid() { return guid_prefs_branch; }
};

static preferences_page_factory_t<preferences_page_subsong_impl> g_preferences_page_subsong_impl_factory;