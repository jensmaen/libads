# libads for TwinCAT

A lean replacement for the **`TcAdsDll`** on Windows and Linux: programs talk ADS through the TwinCAT router on the
same computer (TCP 127.0.0.1:48898) without the `TcAdsDll` or any other Beckhoff component, neither for building nor
for passing the program on. It reaches everything the router has a route to.

Code written for the `TcAdsDll` builds unchanged: the headers `TcAdsAPI.h` and `TcAdsDef.h` carry the DLL's names and
types, and its 23 functions are there (also those without port parameter, on one default port per process).

[libads-client](https://gitlab.com/fresenborg-development/libads-client) builds on libads and adds the route way
without TwinCAT (with Secure ADS), the C++ classes (`AdsDevice`, `AdsVariable`, `AdsNotification`) and the symbol
upload. Take libads alone for C code against the local router; take libads-client for anything else, it brings libads
along.

Status: 0.3, the API may change until 1.0. Tested with TwinCAT 3.1.4026 on Windows 11 (XAR in kernel mode) and
TwinCAT for Linux; compilers clang-cl (MSVC ABI), GCC 14 and llvm-mingw.

## Using it in a CMake project

Requires CMake 3.25 and a C++14 compiler. The library is static and needs nothing but the system's sockets.

**FetchContent** (simplest):

```cmake
include(FetchContent)
FetchContent_Declare(libads
    GIT_REPOSITORY https://gitlab.com/fresenborg-development/libads.git
    GIT_TAG v0.3.0)
FetchContent_MakeAvailable(libads)
target_link_libraries(myprogram PRIVATE libads::ads)
```

**Installed package:**

```sh
cmake -S libads -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /path/to/install
```

```cmake
find_package(libads 0.3 REQUIRED)
target_link_libraries(myprogram PRIVATE libads::ads)
```

**Source copy or Git submodule:** `add_subdirectory(libads)` and the same `target_link_libraries`.

As part of another project (FetchContent, `add_subdirectory`), the library builds neither its tests nor install rules.

| Option | Default | Meaning |
|---|---|---|
| `LIBADS_BUILD_TESTS` | top level only | test programs |
| `LIBADS_INSTALL` | top level only | install rules and CMake package |

The package also installs inner headers (`include/libads/detail`, target `libads::ads-internal`) for libads-client.
They are not a stable API.

## Example

```cpp
#include "TcAdsAPI.h"

long port = AdsPortOpenEx();                  // local TwinCAT router
AmsAddr plc{};
AdsGetLocalAddressEx(port, &plc);             // own AmsNetId: the local PLC
plc.port = AMSPORT_R0_PLC_TC3;                // 851

uint16_t adsState = 0, deviceState = 0;
long status = AdsSyncReadStateReqEx(port, &plc, &adsState, &deviceState);

AdsPortCloseEx(port);
```

## Behaviour when connections fail

- Like the `TcAdsDll`: if TwinCAT restarts while the connection to the router stays (the router reports "started"),
  the library creates the port's notifications again on the target; the program keeps its handles. If the connection
  to the router is lost, the port stays open but every request returns `ROUTERERR_NOTREGISTERED` (`0x507`) until the
  program closes it and opens a new one. `AdsAmsRegisterRouterNotification()` reports the router's events.
- **Target that does not answer:** once a request to a target ran into its timeout, deleting a notification there
  does not wait again until the target sends something: the notification is forgotten at once and the call returns
  `ADSERR_CLIENT_SYNCTIMEOUT`. Closing a port with many notifications so takes at most one timeout.

## Limits

- Client only: the program does not answer ADS requests, and there is no own AMS router.
- Needs TwinCAT's XAR in kernel mode on Windows; the router of the user-mode XAR refuses TCP connections. The route
  way of libads-client works there.
- Not binary compatible with the `TcAdsDll`: there is no `TcAdsDll.dll` replacement, programs are rebuilt.

## Tests

`ctest` in the build folder runs the tests that need no TwinCAT: two tests against a fake TwinCAT router in the same
process (reconnecting connections, notifications with own handles). The library was also tested by hand against real
TwinCAT systems (Windows, TwinCAT for Linux), including restarts of TwinCAT.

## License

MIT, see [`LICENSE`](LICENSE). Parts are taken from Beckhoff ADS (MIT); [`NOTICE.md`](NOTICE.md) lists them and the
changes. This project is not affiliated with or endorsed by Beckhoff Automation. Beckhoff, TwinCAT and ADS are
trademarks of Beckhoff Automation GmbH & Co. KG.
