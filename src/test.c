#include "caulk.h"

#ifdef _WIN32
#include <windows.h>
#define sleepSecs(s) (Sleep((s) * 1000))
#else
#include <unistd.h>
#define sleepSecs(s) (sleep(s))
#endif

int main(int argc, char* argv[]) {
	caulk_Init();
	sleepSecs(5);
	caulk_Shutdown();
	return 0;
}
