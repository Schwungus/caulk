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
			vutton->handler(result, ioFailed);
			return;
		}
	}
}

void caulk_Dispatch() {
	HSteamPipe hSteamPipe = SteamAPI_GetHSteamPipe();
	SteamAPI_ManualDispatch_RunFrame(hSteamPipe);

	CallbackMsg_t callback;
	while (SteamAPI_ManualDispatch_GetNextCallback(hSteamPipe, &callback)) {
		if (callback.m_iCallback != SteamAPICallCompleted_t::k_iCallback) {
			SteamAPI_ManualDispatch_FreeLastCallback(hSteamPipe);
			continue;
		}

		SteamAPICallCompleted_t* pCallback = reinterpret_cast<SteamAPICallCompleted_t*>(callback.m_pubParam);
		void* callResult = caulk_Malloc(pCallback->m_cubParam); // TODO: just use a static allocation?

		bool bFailed;
		if (SteamAPI_ManualDispatch_GetAPICallResult(
			hSteamPipe, pCallback->m_hAsyncCall, callResult, pCallback->m_cubParam, pCallback->m_iCallback,
			&bFailed
		    ))
			dispatchFrFr(pCallback->m_hAsyncCall, callResult, bFailed);

		caulk_Free(callResult);
		SteamAPI_ManualDispatch_FreeLastCallback(hSteamPipe);
	}
}
}
