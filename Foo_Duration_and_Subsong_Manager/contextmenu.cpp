#include "stdafx.h"
#include <helpers/atl-misc.h>
#include "resource.h"
#include "duration_database.h"
#include "global_container.h"
#include "GUID.h"
#include "duration_pref.h"

void db_saving(metadb_handle_list_cref p_data);

class length_setter : public contextmenu_item_simple {
	unsigned get_num_items() override { return 1; }
	void get_item_name(unsigned p_index, pfc::string_base& p_out) override { p_out = "Set custom duration"; }
	GUID get_item_guid(unsigned p_index) override { return guid_dm; }
	bool get_item_description(unsigned p_index, pfc::string_base& p_out) override { 
		p_out = "Open a dialogue for user to set the length of current track and add it to the duration manager database.";
		return true;
	}
	void context_command(unsigned p_index, metadb_handle_list_cref p_data, const GUID& p_caller) override {
		db_saving(p_data);
		ui_control::get()->show_preferences(guid_prefs_duration);
		duration_pref_refresh();
	}
};

void db_saving(metadb_handle_list_cref p_data) {
	input_decoder::ptr dec;
	file_info_impl info;
	for (size_t trackWalk = 0; trackWalk < p_data.get_size(); ++trackWalk) {
		duration_db::song_item item{};
		auto track = p_data[trackWalk];
		const auto path = track->get_path();
		const auto subsong = track->get_subsong_index();
		auto lock = file_lock_manager::get()->acquire_read(path, fb2k::noAbort);
		input_entry::g_open_for_decoding(dec, nullptr, path, fb2k::noAbort);
		dec->get_info(subsong, info, fb2k::noAbort);
		item.file_path = path;
		pfc::string8 key = duration_db::content_hasher(path, subsong);
		item.hash_key = key;
		item.file_name = fb2k::filename_ext(path);
		auto title = info.meta_get("title", 0);
		if (title == nullptr) {
			item.track_title = fb2k::filename_ext(path);
		}
		else {
			item.track_title = title;
		}
		item.subsong = subsong;
		item.original_duration = item.custom_duration = info.get_length();
		song_container[key] = item;
	}
	duration_db::save();
}

static contextmenu_item_factory_t<length_setter> g_contextmenu_factory;
