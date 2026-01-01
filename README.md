<!-- markdownlint-disable MD033 MD045 -->

# caulk

<img align="right" height="128" src="assets/huge-caulk.png" alt="A splatter of caulk paste">

caulk is an **up-to-date**, **functional**, **NON-DEPRECATED** wrapper for Valve's Steamworks API for use with plain C instead of C++ as intended. Capische?

Refer to [the usage section](#basic-usage) and [the code example](src/test.c) for a quick rundown.

:heavy_check_mark: [Schwungus](https://github.com/Schwungus)-certified.

## Rationale

The Steamworks SDK provides the header `steam_api_flat.h` which declares interoperable interface functions. However, it is written in C++, which leads to grotesque build errors if you include it in your plain-C code. This library mitigates that by generating a compatibility layer, gluing C++ classes, structures, functions, and methods defined in the SDK to plain-C code, with the help of `steam_api.json`, which is a repository of all classes and methods used in Steamworks designed for this specific purpose (thanks Valve!).

Gluing the C++ SDK to C objects nonetheless requires using a C++ linker to produce the final binary. This means **you will have to use a C++ toolchain** to build your game.

The key takeaways from going on through with this all are twofold:

1. You get to use Steamworks inside a plain-C game. Doesn't matter whether it's being used for personal amusement or due to technical limitations.
2. Other programming languages that can interface with C native libraries won't have to reinvent a whole new wrapper generator to bind the C++ Steamworks SDK to their C glue module. caulk reduces the friction of porting Steamworks to other programming languages by a whole step.

## Basic usage

caulk requires a [ZIP of the Steamworks SDK](https://partner.steamgames.com/downloads/steamworks_sdk_163.zip) in your project's root. Click that link to semi-legally download it.

caulk uses CMake for the build pipeline. Since CMake is the de-facto standard for cross-platform C/C++ compilation, you shouldn't be afraid to use it - here's a `CMakeLists.txt` example:

```cmake
cmake_minimum_required(VERSION 3.24.0 FATAL_ERROR)
project(myProject)

# REQUIRED: point this to your Steamworks SDK archive.
set(STEAMWORKS_SDK_ZIP ${CMAKE_SOURCE_DIR}/steamworks_sdk_163.zip)

include(FetchContent)
FetchContent_Declare(caulk
    GIT_REPOSITORY https://github.com/Schwungus/caulk.git
    GIT_TAG <release tag or commit SHA>) # edit this line to your liking
FetchContent_MakeAvailable(caulk)

add_executable(myProject main.c)
target_link_libraries(myProject PRIVATE caulk)
caulk_populate(myProject) # automatic copying of steam_appid.txt and shared library objects
```

You will also need to include a [`steam_appid.txt`](steam_appid.txt) in your project's root. You should use the `caulk_populate(targetName)` CMake convenience function: it copies `steam_appid.txt` and `steamapi.dll` over to the passed target's binary output directory.

To actually use the Steamworks SDK from your C code, add `#include <caulk.h>` and prefix each Steamworks function call with `caulk_`:

```c
#include <stdlib.h>
#include <caulk.h>

int main(int argc, char* argv[]) {
    if (!caulk_Init())
        return EXIT_FAILURE;

    /* Do Steamworks stuff here... */

    caulk_Shutdown();
    return EXIT_SUCCESS;
}
```

Again, see [`test.c`](src/test.c) for a more complete example.

The API is designed to be self-documenting. Once you look up a Steamworks object you need to use, calling methods on it is simple: just pass a pointer to your object to a function named `caulk_ClassName_MethodName()`. "Interface" types from the Steamworks SDK are even easier to use: you don't need to make an object for them; just call `caulk_InterfaceName_MethodName()`! (The `I` prefix is absent from `InterfaceName` in this call signature: e.g. `ISteamMatchmaking` becomes just `SteamMatchmaking`.)

## Callbacks and call results

If a method returns `SteamAPICall_t` instead of giving the desired result immediately, it must be an asynchronous call which will spit out a value _eventually_. To use this value, you will have to define a handler function using `caulk_Resolve()`; that function will be called by `caulk_Dispatch()` whenever your result is ready.

Also, you'll be receiving a lot of events from Steamworks. To make use of them, you'll have to register handlers using `caulk_Register()`. These also rely on calls to `caulk_Dispatch()` to trigger.

See the example below for both `caulk_Resolve()` and `caulk_Register()`:

```c
#include <stdlib.h>
#include <caulk.h>

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
    SteamAPICall_t cb = caulk_SteamMatchmaking_CreateLobby(k_ELobbyTypeFriendsOnly, 2);
    caulk_Resolve(cb, resolveCreateLobby);

    for (;;)
        // Dispatch the registered handlers:
        caulk_Dispatch(); // should put a `Sleep` here or smth...

    caulk_Shutdown();
    return EXIT_SUCCESS;
}
```

## Cross-Compilation Warning

Please note that the compatibility-layer generator compiles to a **native binary** and **has to be run** in order for this library to even compile. This means you cannot (currently) compile the library from scratch e.g. on Linux targeting Windows, since the resulting generator binary will be a Windows executable that cannot run natively on the builder Linux.

As a workaround, you'll have to use one of [the releases](https://github.com/Schwungus/caulk/releases) where the glue-code generator is compiled to an [APE binary](https://github.com/jart/cosmopolitan). Change your `FetchContent_Declare` block to something along the lines of:

```cmake
FetchContent_Declare(caulk
    URL https://github.com/toggins/caulk/releases/download/stable/caulk-rolling.zip)
```

Then modify your `CMakeLists.txt` to use the precompiled generator by adding the following after `FetchContent_MakeAvailable(caulk)`:

```cmake
set(CAULK_PREBUILT_GENERATOR ${caulk_SOURCE_DIR}/ape)
```
