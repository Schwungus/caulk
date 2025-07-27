#pragma once

#if !defined(caulk_Malloc) || !defined(caulk_Free)
#ifdef __cplusplus
#include <cstdlib> // IWYU pragma: keep
#else
#include <stdlib.h> // IWYU pragma: keep
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

typedef void (*caulk_ResultHandler)(void*, bool);
typedef void (*caulk_CallbackHandler)(void*);

bool caulk_Init();
void caulk_Shutdown();

void caulk_Resolve(SteamAPICall_t, caulk_ResultHandler);
void caulk_Register(uint32_t, caulk_CallbackHandler);
void caulk_Dispatch();

#ifdef __cplusplus
}
#endif
