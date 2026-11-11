# Notice

libads is MIT licensed (`LICENSE`). Parts of it are taken from **Beckhoff ADS**,
https://github.com/Beckhoff/ADS, commit `24e598a113edfdc0d34414bfefb847930d94cb87` (2026-09-17), also MIT licensed;
its license is in `LICENSE.Beckhoff`. Every taken file keeps its SPDX header and Beckhoff's copyright line.

This project is not affiliated with or endorsed by Beckhoff Automation. Beckhoff, TwinCAT and ADS are trademarks of
Beckhoff Automation GmbH & Co. KG.

## Files taken from Beckhoff ADS

Paths on the left in `AdsLib/` of Beckhoff ADS, on the right in `src/`.

| Beckhoff ADS | libads |
|---|---|
| `AdsDef.h`, `AdsDef.cpp`, `AmsHeader.h`, `Frame.*`, `Log.*`, `wrap_endian.h`, `wrap_socket.h` | `core/` (same names) |
| `standalone/AmsNetId.cpp` | `core/AmsNetId.cpp` |
| `standalone/AdsDef.h` | `core/StandaloneAdsDef.h` |
| `Sockets.*`, `AmsConnection.h`, `Router.h`, `NotificationDispatcher.h`, `AdsNotification.h`, `RingBuffer.h`, `Semaphore.h` | `connection/` (same names) |
| `standalone/AmsConnection.cpp`, `standalone/NotificationDispatcher.cpp` | `connection/` (same names) |
| `standalone/AdsLib.h`, `standalone/AdsLib.cpp`, the port functions of `AdsLib.h` | `local/StandaloneAdsLib.h`, `local/StandaloneAdsLib.cpp` |

Not taken: `TwinCAT/*`, `bhf/*`, the special accesses (`SymbolAccess`, `RegistryAccess`, `RouterAccess`, …),
Beckhoff's build files, tests and tools. The route way, the C++ classes and the rest of `AdsLib.h` are in
libads-client.

## Main changes to those files

- The switch to Beckhoff's `TcAdsDll` (`USE_TWINCAT_ROUTER`) is gone; the library always talks TCP itself.
- **Local way:** `AdsPortOpenEx()` opens a port at the local TwinCAT router (127.0.0.1:48898), as with the
  `TcAdsDll`. Every C function is forwarded to the backend that owns the port: the local router, or one registered
  with `bhf::ads::RegisterBackend()` (the route way of libads-client). `AmsTcpHeader` carries the router command,
  `Router` got hooks for router commands, lost and restored connections.
- **TcAdsDll compatibility:** types `ads_i32`/`ads_ui32` …, `TCADSDLL_API`, the notification header in TwinCAT order,
  callbacks as in the DLL, router events, the DLL's 23 functions; wrappers set `AdsGetLastError()`.
- **Exchangeable transport:** `AmsConnection` holds a transport (TCP here, TLS in libads-client) instead of a socket.
- **Reconnecting:** `AmsConnection` can reconnect through a connector; requests during an outage wait until their
  deadline; `Sockets` connect and wait in slices so that they can be cancelled. Notifications are kept with own
  handles and created again on the target after reconnecting or after TwinCAT restarted.
- A second request on the same port waits for the free slot instead of failing at once.
- Small fixes: `AmsConnection.h` includes only what it needs.

## Own code

Copyright Jens Fresenborg, MIT: `connection/AdsBackend.h`, `connection/AmsTransport.*`, `connection/Subscriptions.*`,
`local/LocalRouter.*`, `local/TcAdsDef.h`, `local/TcAdsAPI.h` (written from the public documentation, not copied from
a TwinCAT installation), the CMake files and the tests.
