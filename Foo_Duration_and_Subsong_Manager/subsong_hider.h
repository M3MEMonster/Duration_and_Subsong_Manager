#pragma once
namespace subsong_db {
	struct subsong_list {
		pfc::string8 file_path;
		std::vector<uint8_t> subsong_filter;
		bool is0base; /*Some decoders use 1-base index for subsong, while others use 0-base*/
	};
	void load();
	void save();
	void uninstall_hook();
	void install_hook();
}


