#include "steam_api.h"

#define CAULK_INTERNAL
#include "caulk.h"

#define LENGTH(expr) (sizeof((expr)) / sizeof(*(expr)))

extern "C" {
static void onResolveCall(void*);

bool caulk_Init() {
	bool result = SteamAPI_Init();
	if (result) {
		SteamAPI_ManualDispatch_Init();
		caulk_Register(SteamAPICallCompleted_t::k_iCallback, onResolveCall);
	}
	return result;
}

void caulk_Shutdown() {
	SteamAPI_Shutdown();
}

struct ResultHandler {
	caulk_ResultHandler fn;
	SteamAPICall_t call;
	bool registered : 1;
};

struct CallbackHandler {
	caulk_CallbackHandler fn;
	uint32_t callback;
	bool registered : 1;
};

static struct ResultHandler resultHandlers[2048] = {0};
static struct CallbackHandler callbackHandlers[2048] = {0};

void caulk_Resolve(SteamAPICall_t call, caulk_ResultHandler handler) {
	for (size_t idx = 0; idx < LENGTH(resultHandlers); idx++) {
		struct ResultHandler* iter = &resultHandlers[idx];
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
		struct CallbackHandler* iter = &callbackHandlers[idx];
		if (!iter->registered) {
			iter->fn = handler;
			iter->callback = callback;
			iter->registered = true;
			return;
		}
	}

	// shit we ran out o fslots againds a.........
}

static void dispatchResultHandler(SteamAPICall_t call, void* result, bool ioFailed) {
	for (size_t idx = 0; idx < LENGTH(resultHandlers); idx++) {
		struct ResultHandler* iter = &resultHandlers[idx];
		if (iter->registered && iter->call == call) {
			iter->registered = false;
			iter->fn(result, ioFailed);
			return;
		}
	}
}

static void onResolveCall(void* data) {
	HSteamPipe hSteamPipe = SteamAPI_GetHSteamPipe();

	SteamAPICallCompleted_t* pCallback = reinterpret_cast<SteamAPICallCompleted_t*>(data);
	void* callResult = caulk_Malloc(pCallback->m_cubParam); // TODO: just use a static allocation?

	bool bFailed;
	if (SteamAPI_ManualDispatch_GetAPICallResult(
		hSteamPipe, pCallback->m_hAsyncCall, callResult, pCallback->m_cubParam, pCallback->m_iCallback, &bFailed
	    ))
		dispatchResultHandler(pCallback->m_hAsyncCall, callResult, bFailed);

	caulk_Free(callResult);
}

void caulk_Dispatch() {
	HSteamPipe hSteamPipe = SteamAPI_GetHSteamPipe();
	SteamAPI_ManualDispatch_RunFrame(hSteamPipe);

	CallbackMsg_t callback;
	while (SteamAPI_ManualDispatch_GetNextCallback(hSteamPipe, &callback)) {
		for (size_t idx = 0; idx < LENGTH(resultHandlers); idx++) {
			struct CallbackHandler* perdun = &callbackHandlers[idx];
			if (perdun->registered && perdun->callback == callback.m_cubParam) {
				perdun->registered = false;
				perdun->fn(callback.m_pubParam);
				break;
			}
		}

		SteamAPI_ManualDispatch_FreeLastCallback(hSteamPipe);
	}
}
}
