#include "steam_api.h"

#define CAULK_INTERNAL
#include "caulk.h"

#define LENGTH(expr) (sizeof((expr)) / sizeof(*(expr)))

extern "C" {
bool caulk_Init() {
	bool result = SteamAPI_Init();
	if (result)
		SteamAPI_ManualDispatch_Init();
	return result;
}

void caulk_Shutdown() {
	SteamAPI_Shutdown();
}

struct Vutton {
	GucciHandler handler;
	SteamAPICall_t call;
	bool registered : 1;
};

static struct Vutton iCouldHaveMyGucciOn[2048] = {0};
void caulk_Gucci(SteamAPICall_t call, GucciHandler handler) {
	for (size_t idx = 0; idx < LENGTH(iCouldHaveMyGucciOn); idx++) {
		struct Vutton* vutton = &iCouldHaveMyGucciOn[idx];
		if (!vutton->registered) {
			vutton->handler = handler;
			vutton->registered = true;
			vutton->call = call;
			return;
		}
	}

	// shit we ran out o fslots.........
}

static void dispatchFrFr(SteamAPICall_t call, void* result, bool ioFailed) {
	for (size_t idx = 0; idx < LENGTH(iCouldHaveMyGucciOn); idx++) {
		struct Vutton* vutton = &iCouldHaveMyGucciOn[idx];
		if (vutton->registered && vutton->call == call) {
			vutton->registered = false;
			if (!ioFailed)
				vutton->handler(result);
			return;
		}
	}
}

void caulk_Dispatch() {
	HSteamPipe hSteamPipe = SteamAPI_GetHSteamPipe();
	SteamAPI_ManualDispatch_RunFrame(hSteamPipe);

	CallbackMsg_t callback, *pCallback = &callback;
	while (SteamAPI_ManualDispatch_GetNextCallback(hSteamPipe, pCallback)) {
		if (callback.m_iCallback == SteamAPICallCompleted_t::k_iCallback) {
			SteamAPICallCompleted_t* pCallCompleted = reinterpret_cast<SteamAPICallCompleted_t*>(pCallback);
			void* callResult = caulk_Malloc(pCallback->m_cubParam); // TODO: just use a static allocation?

			bool bFailed;
			if (SteamAPI_ManualDispatch_GetAPICallResult(
				hSteamPipe, pCallCompleted->m_hAsyncCall, callResult, pCallback->m_cubParam,
				pCallback->m_iCallback, &bFailed
			    ))
				dispatchFrFr(pCallCompleted->m_hAsyncCall, callResult, bFailed);

			caulk_Free(callResult);
			SteamAPI_ManualDispatch_FreeLastCallback(hSteamPipe);
		}
	}
}
}
