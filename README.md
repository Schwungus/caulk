# caulk

<img align="right" height="128" src="assets/huge-caulk.png" alt="A splatter of caulk paste">

caulk is an **up-to-date**, **functional**, **NON-DEPRECATED** wrapper for Valve's Steamworks API for use with plain C. Capische?

Refer to the [usage section](#basic-usage) and the [code example](src/caulk-test.c) for a quick rundown.

:heavy_check_mark: [Schwungus](https://github.com/Schwungus)-certified.

## Rationale

The Steamworks SDK provides the header `steam_api_flat.h` which declares interoperable interface functions. However, it isn't pure C, leading to build errors (duh). This library mitigates that by generating a plain-C compatibility layer to C++ types, functions, and methods defined in the SDK, with the help of `steam_api.json`.

## Basic usage

caulk requires a [ZIP of the Steamworks SDK](https://partner.steamgames.com/downloads/steamworks_sdk_162.zip) in your project's root. Click that link to semi-legally download it.

caulk uses CMake for the build pipeline. Since CMake is the de-facto standard for cross-platform C/C++ compilation, you shouldn't be afraid to use it - here's a `CMakeLists.txt` example:

```cmake
cmake_minimum_required(VERSION 3.24.0 FATAL_ERROR)
project(myProject)

# REQUIRED: point this to your Steamworks SDK archive.
set(STEAMWORKS_SDK_ZIP ${CMAKE_SOURCE_DIR}/steamworks_sdk_162.zip)

include(FetchContent)
FetchContent_Declare(
    caulk
    GIT_REPOSITORY "https://github.com/Schwungus/caulk.git"
    GIT_TAG <release tag or commit SHA>) # edit this line to your liking
FetchContent_MakeAvailable(caulk)

add_executable(myProject main.c)
caulk_register(myProject) # automatic copying of steam_appid.txt and shared library objects
```

You'll also need to include a [steam_appid.txt](steam_appid.txt) in your project's root; `caulk_register` copies that over as well.

To use the Steam API from your C code, add `#include "caulk.h"` and prefix each SteamAPI function with `caulk_`:

```c
#include <stdlib.h>
#include "caulk.h"

int main(int argc, char* argv[]) {
    if (!caulk_Init())
        return EXIT_FAILURE;

    /* Do SteamAPI stuff here... */

    caulk_Shutdown();
    return EXIT_SUCCESS;
}
```

Again, see [test.c](src/caulk-test.c) for a more complete example.

## Callbacks and call results

The Steamworks API relies a lot on callbacks and call results. Here's how to use them.

If a method returns `SteamAPICall_t` instead of giving you the desired result, it must be an asynchronous call which will spit out a value _eventually_. To use said value, you'll have to define a handler function using `caulk_Resolve()`, which is called by `caulk_Dispatch()` whenever your result is ready.

Also, you'll be receiving a lot of events from Steamworks. To make any use of them, you'll have to register handlers using `caulk_Register()`.

See the example below for both `caulk_Resolve()` and `caulk_Register()`:

```c
#include <stdlib.h>
#include "caulk.h"

static void resolveCreateLobby(void* pData, bool ioFail) {
    LobbyCreated_t* data = pData;
    if (!ioFail && data->m_eResult == k_EResultOK)
        printf("Created lobby ID=%d!!!\n", data->m_ulSteamIDLobby);
}

static void onEnterLobby(void* pData) {
    LobbyEnter_t* data = pData;
    // Do cool stuff with `data`.
}

int main(int argc, char* argv[]) {
    if (!caulk_Init())
        return EXIT_FAILURE;

    // Call `onEnterLobby()` every time we enter a lobby.
    caulk_Register(LobbyEnter_t_iCallback, onEnterLobby);

    // Let a lobby be created in the background, and run `resolveCreateLobby()` when it's done.
    SteamAPICall_t cb = caulk_ISteamMatchmaking_CreateLobby(mm, k_ELobbyTypeFriendsOnly, 2);
    caulk_Resolve(cb, resolveCreateLobby);

    for (;;)
        // Dispatch the registered handlers:
        caulk_Dispatch(); // should put a `Sleep` here or smth...

    caulk_Shutdown();
    return EXIT_SUCCESS;
}
```
