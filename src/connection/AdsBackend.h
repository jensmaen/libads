// SPDX-License-Identifier: MIT
/**
   Copyright (c) 2026 Jens Fresenborg
 */

#pragma once

#include "AmsConnection.h"

#include <memory>

/**
 * One way to reach ADS devices: the local TwinCAT router (LocalRouter, in
 * libads) or an ADS route directly to the target (AmsRouter, in
 * libads-client). The C-API forwards each call to the backend that owns the
 * port.
 */
struct AdsBackend {
	virtual ~AdsBackend()
	{
	}

	virtual bool OwnsPort(uint16_t port) = 0;
	virtual long ClosePort(uint16_t port) = 0;
	virtual long GetLocalAddress(uint16_t port, AmsAddr *pAddr) = 0;
	virtual long GetTimeout(uint16_t port, uint32_t &timeout) = 0;
	virtual long SetTimeout(uint16_t port, uint32_t timeout) = 0;
	virtual long AdsRequest(AmsRequest &request) = 0;
	virtual long AddNotification(AmsRequest &request,
				     uint32_t *pNotification,
				     std::shared_ptr<Notification> notify) = 0;
	virtual long DelNotification(uint16_t port, const AmsAddr *pAddr,
				     uint32_t hNotification) = 0;
};

namespace bhf
{
namespace ads
{
/**
 * Makes the ports of a further backend reachable through the C-API, after
 * the local router. Registering the same backend again does nothing. The
 * backend has to live as long as the C-API is used (a static object).
 */
void RegisterBackend(AdsBackend &backend);

/** Remembers status for AdsGetLastError() and returns it. */
ads_i32 SetLastError(long status);
}
}
