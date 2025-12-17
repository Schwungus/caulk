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

#include <steam_api.h>

#define CAULK_INTERNAL
#include "caulk.h"

#define LENGTH(expr) (sizeof((expr)) / sizeof(*(expr)))

extern "C" {
static void onCallCompleted(void*);

bool caulk_Init() {
	bool result = SteamAPI_Init();
	if (result) {
		SteamAPI_ManualDispatch_Init();
		caulk_Register(SteamAPICallCompleted_t_iCallback, onCallCompleted);
	}
	return result;
}

void caulk_Shutdown() {
	SteamAPI_Shutdown();
}

typedef struct {
	caulk_ResultHandler fn;
	SteamAPICall_t call;
	bool registered : 1;
} ResultHandler;

typedef struct {
	caulk_CallbackHandler fn;
	uint32_t callback;
	bool registered : 1;
} CallbackHandler;

static ResultHandler resultHandlers[2048] = {0};
static CallbackHandler callbackHandlers[2048] = {0};

void caulk_Resolve(SteamAPICall_t call, caulk_ResultHandler handler) {
	for (size_t idx = 0; idx < LENGTH(resultHandlers); idx++) {
		ResultHandler* iter = &resultHandlers[idx];
		if (!iter->registered) {
			iter->fn = handler;
			iter->call = call;
			iter->registered = true;
			return;
		}
	}

	// shit we ran out o fslots.........
}

void caulk_Register(uint32_t callback, caulk_CallbackHandler handler) {
	for (size_t idx = 0; idx < LENGTH(callbackHandlers); idx++) {
		CallbackHandler* iter = &callbackHandlers[idx];
		if (iter->registered)
			continue;
		iter->fn = handler;
		iter->callback = callback;
		iter->registered = true;
		return;
	}

	// shit we ran out o fslots againds a.........
}

static void dispatchResultHandler(SteamAPICall_t call, void* result, bool ioFailed) {
	for (size_t idx = 0; idx < LENGTH(resultHandlers); idx++) {
		ResultHandler* iter = &resultHandlers[idx];
		if (iter->registered && iter->call == call) {
			iter->registered = false;
			iter->fn(result, ioFailed);
			return;
		}
	}
}

static void onCallCompleted(void* data) {
	HSteamPipe hSteamPipe = SteamAPI_GetHSteamPipe();

	SteamAPICallCompleted_t* pCallback = reinterpret_cast<SteamAPICallCompleted_t*>(data);
	void* callResult = caulk_Malloc(pCallback->m_cubParam); // TODO: just use a static allocation?

	bool bFailed;
	if (SteamAPI_ManualDispatch_GetAPICallResult(hSteamPipe, pCallback->m_hAsyncCall, callResult,
		    pCallback->m_cubParam, pCallback->m_iCallback, &bFailed))
		dispatchResultHandler(pCallback->m_hAsyncCall, callResult, bFailed);

	caulk_Free(callResult);
}

void caulk_Dispatch() {
	HSteamPipe hSteamPipe = SteamAPI_GetHSteamPipe();
	SteamAPI_ManualDispatch_RunFrame(hSteamPipe);

	CallbackMsg_t callback;
	while (SteamAPI_ManualDispatch_GetNextCallback(hSteamPipe, &callback)) {
		for (size_t idx = 0; idx < LENGTH(resultHandlers); idx++) {
			CallbackHandler* iter = &callbackHandlers[idx];
			if (iter->registered && iter->callback == callback.m_iCallback) {
				iter->fn(callback.m_pubParam);
				break;
			}
		}

		SteamAPI_ManualDispatch_FreeLastCallback(hSteamPipe);
	}
}
}
