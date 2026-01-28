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
static void on_call_completed(void*);

bool caulk_Init() {
	bool result = SteamAPI_Init();
	if (result) {
		SteamAPI_ManualDispatch_Init();
		caulk_Register(SteamAPICallCompleted_t_iCallback, on_call_completed);
	}
	return result;
}

void caulk_Shutdown() {
	SteamAPI_Shutdown();
}

typedef struct {
	caulk_ResultHandler fn;
	SteamAPICall_t call;
	bool registered;
} ResultHandler;

typedef struct {
	caulk_CallbackHandler fn;
	uint32_t callback;
	bool registered;
} CallbackHandler;

#define COUNT (2048)
static ResultHandler result_handlers[COUNT] = {0};
static CallbackHandler callback_handlers[COUNT] = {0};
#undef COUNT

void caulk_Resolve(SteamAPICall_t call, caulk_ResultHandler handler) {
	for (size_t idx = 0; idx < LENGTH(result_handlers); idx++) {
		ResultHandler* iter = &result_handlers[idx];
		if (!iter->registered) {
			iter->fn = handler, iter->call = call;
			iter->registered = true;
			return;
		}
	}

	// shit we ran out o fslots.........
}

void caulk_Register(uint32_t callback, caulk_CallbackHandler handler) {
	for (size_t idx = 0; idx < LENGTH(callback_handlers); idx++) {
		CallbackHandler* iter = &callback_handlers[idx];
		if (!iter->registered) {
			iter->fn = handler, iter->callback = callback;
			iter->registered = true;
			return;
		}
	}

	// shit we ran out o fslots againds a.........
}

static void handle_dispatch_result(SteamAPICall_t call, void* result, bool io_failed) {
	for (size_t idx = 0; idx < LENGTH(result_handlers); idx++) {
		ResultHandler* iter = &result_handlers[idx];
		if (iter->registered && iter->call == call) {
			iter->registered = false;
			iter->fn(result, io_failed);
			return;
		}
	}
}

static void on_call_completed(void* data) {
	HSteamPipe steam_pipe = SteamAPI_GetHSteamPipe();

	SteamAPICallCompleted_t* callback = reinterpret_cast<SteamAPICallCompleted_t*>(data);
	void* call_result = malloc(callback->m_cubParam); // TODO: just use a static allocation?

	bool failed = false;
	if (SteamAPI_ManualDispatch_GetAPICallResult(steam_pipe, callback->m_hAsyncCall, call_result,
		    (int)callback->m_cubParam, callback->m_iCallback, &failed))
		handle_dispatch_result(callback->m_hAsyncCall, call_result, failed);

	free(call_result);
}

void caulk_Dispatch() {
	HSteamPipe steam_pipe = SteamAPI_GetHSteamPipe();
	SteamAPI_ManualDispatch_RunFrame(steam_pipe);

	CallbackMsg_t callback;
	while (SteamAPI_ManualDispatch_GetNextCallback(steam_pipe, &callback)) {
		for (size_t idx = 0; idx < LENGTH(result_handlers); idx++) {
			CallbackHandler* iter = &callback_handlers[idx];
			if (iter->registered && iter->callback == callback.m_iCallback) {
				iter->fn(callback.m_pubParam);
				break;
			}
		}
		SteamAPI_ManualDispatch_FreeLastCallback(steam_pipe);
	}
}
}
