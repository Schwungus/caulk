#include "steam_api.h"

namespace caulk {
#include "caulk.h"
}

extern "C" {
bool caulk::SteamAPI_Init() {
	return ::SteamAPI_Init();
}

void caulk::SteamAPI_Shutdown() {
	::SteamAPI_Shutdown();
}
}
