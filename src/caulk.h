#pragma once

#if !defined(caulk_Malloc) || !defined(caulk_Free)
#ifdef __cplusplus
#include <cstdlib>
#else
#include <stdlib.h>
#endif
#endif

#ifndef caulk_Malloc
#define caulk_Malloc malloc
#endif

#ifndef caulk_Free
#define caulk_Free free
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "__gen.h"

bool caulk_Init();
void caulk_Shutdown();

bool caulk_Gucci(SteamAPICall_t);
void caulk_Dispatch();

#ifdef __cplusplus
}
#endif
