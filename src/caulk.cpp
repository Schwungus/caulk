#include "steam_api.h"

namespace caulk {
#include "caulk.h"
}

extern "C" {
bool caulk::caulk_Init() {
	return ::SteamAPI_Init();
}

void caulk::caulk_Shutdown() {
	::SteamAPI_Shutdown();
}
}
