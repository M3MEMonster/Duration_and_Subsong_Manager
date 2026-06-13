#include"stdafx.h"

// Declaration of your component's version information
// Since foobar2000 v1.0 having at least one of these in your DLL is mandatory to let the troubleshooter tell different versions of your component apart.
// Note that it is possible to declare multiple components within one DLL, but it's strongly recommended to keep only one declaration per DLL.
// As for 1.1, the version numbers are used by the component update finder to find updates; for that to work, you must have ONLY ONE declaration per DLL. If there are multiple declarations, the component is assumed to be outdated and a version number of "0" is assumed, to overwrite the component with whatever is currently on the site assuming that it comes with proper version numbers.
DECLARE_COMPONENT_VERSION("Duration & Subsong Manager", "1.0", 
	"Let users be able to customize length of any tracks\n"
	"Let users be able to hide subsongs from the autoplaylist\n"
	"Adds a high-precision timer UI that shows and updates playback time and track length in ms([HH:]MM:SS.mmm)");


// This will prevent users from renaming your component around (important for proper troubleshooter behaviors) or loading multiple instances of it.
VALIDATE_COMPONENT_FILENAME("foo_duration_subsong_manager.dll");

// Activate cfg_var downgrade functionality if enabled. Relevant only when cycling from newer FOOBAR2000_TARGET_VERSION to older.
FOOBAR2000_IMPLEMENT_CFG_VAR_DOWNGRADE;