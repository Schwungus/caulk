// This is free and unencumbered software released into the public domain.
//
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.
//
// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// For more information, please refer to <https://unlicense.org>

#include <stdio.h>
#include <stdlib.h>

#include <caulk.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_secs(s) (Sleep((s) * 1000))
#else
#include <unistd.h>
#define sleep_secs(s) (sleep(s))
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

	int num_friends = caulk_SteamFriends_GetFriendCount(k_EFriendFlagImmediate);
	printf("You have %d friends%s\n", num_friends, num_friends ? ":" : ", ...huh");

	for (int i = 0; i < num_friends; i++) {
		CSteamID friend = caulk_SteamFriends_GetFriendByIndex(i, k_EFriendFlagImmediate);
		const char* friend_name = caulk_SteamFriends_GetFriendPersonaName(friend);
		printf("%d. %s (%llu)\n", i + 1, friend_name, friend);
	}

	fflush(stdout);
	sleep_secs(5);

	caulk_Shutdown();
	return EXIT_SUCCESS;
}
