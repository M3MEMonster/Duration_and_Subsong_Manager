#include "stdafx.h"
#include "duration_database.h"
#include "subsong_hider.h"

namespace {
	void on_init() {
		subsong_db::load();
		subsong_db::install_hook();
		duration_db::load();
	}

	void on_quit() {
		duration_db::save();
		subsong_db::save();
		subsong_db::uninstall_hook();
	}
	FB2K_ON_INIT_STAGE(on_init, init_stages::after_config_read);
	FB2K_RUN_ON_QUIT(on_quit);
}