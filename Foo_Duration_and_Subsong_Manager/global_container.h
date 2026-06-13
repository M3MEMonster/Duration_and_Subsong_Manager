#pragma once
#include "stdafx.h"
#include "duration_database.h"
#include "subsong_hider.h"
#include <set>

extern pfc::map_t<pfc::string8, duration_db::song_item> song_container;
extern pfc::map_t<pfc::string8, subsong_db::subsong_list> mul_subsong_filter;
extern pfc::map_t<pfc::string8, std::set<int>> _temp_container;