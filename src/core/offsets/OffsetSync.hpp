#pragma once

#include <string>

namespace OffsetSync {
	// Default URL — point this at your VPS (private). Change in menu or config.
	inline std::string url = "https://raw.githubusercontent.com/1-RageNova-1/-cs2-external-hub/main/offsets.json";
	inline std::string last_error;
	inline std::string last_build;
	inline std::string last_status = "idle";

	bool FetchAndApply();
	bool ApplyJson(const std::string& body);
}
