#include <stdio.h>
#include <stdlib.h>

#include "caulk.h"

#ifdef _WIN32
#include <windows.h>
#define sleepSecs(s) (Sleep((s) * 1000))
#else
#include <unistd.h>
#define sleepSecs(s) (sleep(s))
#endif

int main(int argc, char* argv[]) {
	printf("==========\n");
	printf("CAULK TEST\n");
	printf("==========\n\n");

	if (!caulk_Init()) {
		printf("ERROR: No caulk here, try opening Steam.\n");
		return EXIT_FAILURE;
	}

	ISteamUser* steamUser = caulk_SteamUser();
	ISteamFriends* steamFriends = caulk_SteamFriends();
	printf(
	    "Logged in as %s (%lu)\n\n", caulk_ISteamFriends_GetPersonaName(steamFriends),
	    caulk_ISteamUser_GetSteamID(steamUser)
	);

	int numFriends = caulk_ISteamFriends_GetFriendCount(steamFriends, k_EFriendFlagImmediate);
	printf("You have %d friends%s\n", numFriends, numFriends ? ":" : ", ...huh");

	for (int i = 0; i < numFriends; i++) {
		CSteamID friend = caulk_ISteamFriends_GetFriendByIndex(steamFriends, i, k_EFriendFlagImmediate);
		const char* friendName = caulk_ISteamFriends_GetFriendPersonaName(steamFriends, friend);
		printf("%d. %s (%lu)\n", i + 1, friendName, friend);
	}

	fflush(stdout);
	sleepSecs(5);

	caulk_Shutdown();
	return EXIT_SUCCESS;
}
