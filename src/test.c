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
	if (!caulk_Init())
		return EXIT_FAILURE;
	sleepSecs(5);
	caulk_Shutdown();
	return EXIT_SUCCESS;
}
