#include <stdio.h>
#include <stdlib.h>

#include <caulk.h>

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

	printf("Logged in as %s (%llu)\n\n", caulk_SteamFriends_GetPersonaName(), caulk_SteamUser_GetSteamID());

	int numFriends = caulk_SteamFriends_GetFriendCount(k_EFriendFlagImmediate);
	printf("You have %d friends%s\n", numFriends, numFriends ? ":" : ", ...huh");

	for (int i = 0; i < numFriends; i++) {
		CSteamID friend = caulk_SteamFriends_GetFriendByIndex(i, k_EFriendFlagImmediate);
		const char* friendName = caulk_SteamFriends_GetFriendPersonaName(friend);
		printf("%d. %s (%llu)\n", i + 1, friendName, friend);
	}

	fflush(stdout);
	sleepSecs(5);

	caulk_Shutdown();
	return EXIT_SUCCESS;
}
