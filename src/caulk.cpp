#include "steam_api.h"

#define CAULK_INTERNAL
#include "caulk.h"

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

#define LENGTH(expr) (sizeof((expr)) / sizeof(*(expr)))

struct Vutton {
	SteamAPICall_t call;
	bool registered : 1;
	bool gucci : 1;
	bool ioFailed : 1;
};

static struct Vutton iCouldHaveMyGucciOn[2048] = {0};
bool caulk_Gucci(SteamAPICall_t call) {
	for (size_t idx = 0; idx < LENGTH(iCouldHaveMyGucciOn); idx++) {
		struct Vutton* vutton = &iCouldHaveMyGucciOn[idx];
		if (!vutton->registered || vutton->call != call)
			continue;
		if (vutton->gucci) {
			vutton->registered = 0;
			vutton->gucci = 0;
			return !vutton->ioFailed;
		} else
			return false;
	}

	for (size_t idx = 0; idx < LENGTH(iCouldHaveMyGucciOn); idx++) {
		struct Vutton* vutton = &iCouldHaveMyGucciOn[idx];
		if (!vutton->registered) {
			vutton->registered = true;
			vutton->call = call;
			vutton->gucci = 0;
			return false;
		}
	}

	return false;
}

static void makeGucci(SteamAPICall_t call, bool ioFailed) {
	for (size_t idx = 0; idx < LENGTH(iCouldHaveMyGucciOn); idx++) {
		struct Vutton* vutton = &iCouldHaveMyGucciOn[idx];
		if (vutton->call == call && vutton->registered) {
			vutton->gucci = 1;
			vutton->ioFailed = ioFailed;
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
			void* pTmpCallResult =
			    caulk_Malloc(pCallback->m_cubParam); // TODO: just use a static allocation?

			bool bFailed;
			if (SteamAPI_ManualDispatch_GetAPICallResult(
				hSteamPipe, pCallCompleted->m_hAsyncCall, pTmpCallResult, pCallback->m_cubParam,
				pCallback->m_iCallback, &bFailed
			    ))
				makeGucci(pCallCompleted->m_hAsyncCall, bFailed);

			caulk_Free(pTmpCallResult);
			SteamAPI_ManualDispatch_FreeLastCallback(hSteamPipe);
		}
	}
}
}
