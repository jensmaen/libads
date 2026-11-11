// SPDX-License-Identifier: MIT
/**
   Copyright (c) 2015 - 2022 Beckhoff Automation GmbH & Co. KG
 */

#include "StandaloneAdsLib.h"
#include "AdsBackend.h"
#include "LocalRouter.h"
#include "Log.h"

#include <cstring>
#include <map>
#include <mutex>
#include <vector>

#ifndef LIBADS_VERSION_MAJOR
#define LIBADS_VERSION_MAJOR 0
#define LIBADS_VERSION_MINOR 0
#define LIBADS_VERSION_PATCH 0
#endif

/*
 * The local router is always there. Further backends (the route way of
 * libads-client) register themselves with bhf::ads::RegisterBackend() when
 * they are used first; every port belongs to exactly one backend.
 */
static LocalRouter &GetLocalRouter()
{
	static LocalRouter router;
	return router;
}

namespace
{
std::mutex &BackendsMutex()
{
	static std::mutex mutex;
	return mutex;
}

std::vector<AdsBackend *> &Backends()
{
	static std::vector<AdsBackend *> backends;
	return backends;
}
}

namespace bhf
{
namespace ads
{
void RegisterBackend(AdsBackend &backend)
{
	std::lock_guard<std::mutex> lock(BackendsMutex());
	auto &backends = Backends();
	for (const auto b : backends) {
		if (b == &backend) {
			return;
		}
	}
	backends.push_back(&backend);
}
}
}

/** true if the local router owns the port */
static bool IsLocalPort(uint16_t port)
{
	return GetLocalRouter().OwnsPort(port);
}

/** Registered backend that owns the port, nullptr if none */
static AdsBackend *GetOtherBackend(uint16_t port)
{
	std::lock_guard<std::mutex> lock(BackendsMutex());
	for (const auto b : Backends()) {
		if (b->OwnsPort(port)) {
			return b;
		}
	}
	return nullptr;
}

/** Backend that owns the port, nullptr if none */
static AdsBackend *GetBackend(long port)
{
	if (IsLocalPort((uint16_t)port)) {
		return &GetLocalRouter();
	}
	return GetOtherBackend((uint16_t)port);
}

#define GET_BACKEND(port)                                      	AdsBackend *const backend = GetBackend(port);          	if (!backend) {                                        		return ADSERR_CLIENT_PORTNOTOPEN;              	}

#define ASSERT_PORT(port)                                 	do {                                              		if ((port) <= 0 || (port) > UINT16_MAX) { 			return ADSERR_CLIENT_PORTNOTOPEN; 		}                                         	} while (false)

#define ASSERT_PORT_AND_AMSADDR(port, pAddr)            	do {                                            		ASSERT_PORT(port);                      		if (!(pAddr)) {                         			return ADSERR_CLIENT_NOAMSADDR; 		}                                       	} while (false)

namespace
{
static long AdsPortCloseExImpl(long port)
{
	ASSERT_PORT(port);
	GET_BACKEND(port);
	return backend->ClosePort((uint16_t)port);
}

static long AdsPortOpenExImpl()
{
	return GetLocalRouter().OpenPort();
}

static long AdsGetLocalAddressExImpl(long port, AmsAddr *pAddr)
{
	ASSERT_PORT_AND_AMSADDR(port, pAddr);
	GET_BACKEND(port);
	return backend->GetLocalAddress((uint16_t)port, pAddr);
}

static long AdsSyncReadReqEx2Impl(long port, const AmsAddr *pAddr, uint32_t indexGroup,
		       uint32_t indexOffset, uint32_t bufferLength,
		       void *buffer, uint32_t *bytesRead)
{
	ASSERT_PORT_AND_AMSADDR(port, pAddr);
	GET_BACKEND(port);
	if (!buffer) {
		return ADSERR_CLIENT_INVALIDPARM;
	}

	try {
		AmsRequest request{ *pAddr,
				    (uint16_t)port,
				    AoEHeader::READ,
				    bufferLength,
				    buffer,
				    bytesRead,
				    sizeof(AoERequestHeader) };
		request.frame.prepend(AoERequestHeader{ indexGroup, indexOffset,
							bufferLength });
		return backend->AdsRequest(request);
	} catch (const std::bad_alloc &) {
		return GLOBALERR_NO_MEMORY;
	}
}

static long AdsSyncReadDeviceInfoReqExImpl(long port, const AmsAddr *pAddr, char *devName,
				AdsVersion *version)
{
	ASSERT_PORT_AND_AMSADDR(port, pAddr);
	GET_BACKEND(port);
	if (!devName || !version) {
		return ADSERR_CLIENT_INVALIDPARM;
	}

	try {
		static const size_t NAME_LENGTH = 16;
		uint8_t buffer[sizeof(*version) + NAME_LENGTH];
		AmsRequest request{ *pAddr, (uint16_t)port,
				    AoEHeader::READ_DEVICE_INFO, sizeof(buffer),
				    buffer };
		const auto status = backend->AdsRequest(request);
		if (!status) {
			version->version = buffer[0];
			version->revision = buffer[1];
			version->build = bhf::ads::letoh<uint16_t>(
				buffer + offsetof(AdsVersion, build));
			memcpy(devName, buffer + sizeof(*version), NAME_LENGTH);
		}
		return status;
	} catch (const std::bad_alloc &) {
		return GLOBALERR_NO_MEMORY;
	}
}

static long AdsSyncReadStateReqExImpl(long port, const AmsAddr *pAddr, uint16_t *adsState,
			   uint16_t *devState)
{
	ASSERT_PORT_AND_AMSADDR(port, pAddr);
	GET_BACKEND(port);
	if (!adsState || !devState) {
		return ADSERR_CLIENT_INVALIDPARM;
	}

	try {
		uint16_t buffer[2];
		AmsRequest request{ *pAddr, (uint16_t)port,
				    AoEHeader::READ_STATE, sizeof(buffer),
				    buffer };
		const auto status = backend->AdsRequest(request);
		if (!status) {
			*adsState = bhf::ads::letoh(buffer[0]);
			*devState = bhf::ads::letoh(buffer[1]);
		}
		return status;
	} catch (const std::bad_alloc &) {
		return GLOBALERR_NO_MEMORY;
	}
}

static long AdsSyncReadWriteReqEx2Impl(long port, const AmsAddr *pAddr,
			    uint32_t indexGroup, uint32_t indexOffset,
			    uint32_t readLength, void *readData,
			    uint32_t writeLength, const void *writeData,
			    uint32_t *bytesRead)
{
	ASSERT_PORT_AND_AMSADDR(port, pAddr);
	GET_BACKEND(port);
	if ((readLength && !readData) || (writeLength && !writeData)) {
		return ADSERR_CLIENT_INVALIDPARM;
	}

	try {
		AmsRequest request{ *pAddr,
				    (uint16_t)port,
				    AoEHeader::READ_WRITE,
				    readLength,
				    readData,
				    bytesRead,
				    sizeof(AoEReadWriteReqHeader) +
					    writeLength };
		request.frame.prepend(writeData, writeLength);
		request.frame.prepend(AoEReadWriteReqHeader{
			indexGroup, indexOffset, readLength, writeLength });
		return backend->AdsRequest(request);
	} catch (const std::bad_alloc &) {
		return GLOBALERR_NO_MEMORY;
	}
}

static long AdsSyncWriteReqExImpl(long port, const AmsAddr *pAddr, uint32_t indexGroup,
		       uint32_t indexOffset, uint32_t bufferLength,
		       const void *buffer)
{
	ASSERT_PORT_AND_AMSADDR(port, pAddr);
	GET_BACKEND(port);
	if (bufferLength && !buffer) {
		return ADSERR_CLIENT_INVALIDPARM;
	}

	try {
		AmsRequest request{
			*pAddr,
			(uint16_t)port,
			AoEHeader::WRITE,
			0,
			nullptr,
			nullptr,
			sizeof(AoERequestHeader) + bufferLength,
		};
		request.frame.prepend(buffer, bufferLength);
		request.frame.prepend<AoERequestHeader>(
			{ indexGroup, indexOffset, bufferLength });
		return backend->AdsRequest(request);
	} catch (const std::bad_alloc &) {
		return GLOBALERR_NO_MEMORY;
	}
}

static long AdsSyncWriteControlReqExImpl(long port, const AmsAddr *pAddr,
			      uint16_t adsState, uint16_t devState,
			      uint32_t bufferLength, const void *buffer)
{
	ASSERT_PORT_AND_AMSADDR(port, pAddr);
	GET_BACKEND(port);
	try {
		AmsRequest request{ *pAddr,
				    (uint16_t)port,
				    AoEHeader::WRITE_CONTROL,
				    0,
				    nullptr,
				    nullptr,
				    sizeof(AdsWriteCtrlRequest) +
					    bufferLength };
		request.frame.prepend(buffer, bufferLength);
		request.frame.prepend<AdsWriteCtrlRequest>(
			{ adsState, devState, bufferLength });
		return backend->AdsRequest(request);
	} catch (const std::bad_alloc &) {
		return GLOBALERR_NO_MEMORY;
	}
}

static long AdsSyncAddDeviceNotificationReqExImpl(long port, const AmsAddr *pAddr,
				       uint32_t indexGroup,
				       uint32_t indexOffset,
				       const AdsNotificationAttrib *pAttrib,
				       NotificationCallback pFunc,
				       uint32_t hUser, uint32_t *pNotification)
{
	ASSERT_PORT_AND_AMSADDR(port, pAddr);
	GET_BACKEND(port);
	if (!pAttrib || !pFunc || !pNotification) {
		return ADSERR_CLIENT_INVALIDPARM;
	}

	try {
		uint8_t buffer[sizeof(*pNotification)];
		AmsRequest request{ *pAddr,
				    (uint16_t)port,
				    AoEHeader::ADD_DEVICE_NOTIFICATION,
				    sizeof(buffer),
				    buffer,
				    nullptr,
				    sizeof(AdsAddDeviceNotificationRequest) };
		request.frame.prepend(AdsAddDeviceNotificationRequest{
			indexGroup, indexOffset, pAttrib->cbLength,
			pAttrib->nTransMode, pAttrib->nMaxDelay,
			pAttrib->nCycleTime });
		auto notify = std::make_shared<Notification>(pFunc, hUser,
							     pAttrib->cbLength,
							     *pAddr,
							     (uint16_t)port);
		return backend->AddNotification(request, pNotification,
						   notify);
	} catch (const std::bad_alloc &) {
		return GLOBALERR_NO_MEMORY;
	}
}

static long AdsSyncDelDeviceNotificationReqExImpl(long port, const AmsAddr *pAddr,
				       uint32_t hNotification)
{
	ASSERT_PORT_AND_AMSADDR(port, pAddr);
	GET_BACKEND(port);
	return backend->DelNotification((uint16_t)port, pAddr,
					   hNotification);
}

static long AdsSyncGetTimeoutExImpl(long port, uint32_t *timeout)
{
	ASSERT_PORT(port);
	GET_BACKEND(port);
	if (!timeout) {
		return ADSERR_CLIENT_INVALIDPARM;
	}
	return backend->GetTimeout((uint16_t)port, *timeout);
}

static long AdsSyncSetTimeoutExImpl(long port, uint32_t timeout)
{
	ASSERT_PORT(port);
	GET_BACKEND(port);
	return backend->SetTimeout((uint16_t)port, timeout);
}
}

/* Interface with the types of the TcAdsDll. The wrappers convert between
 * ads_i32/ads_ui32 and the types of the implementations above and remember
 * the result for AdsGetLastError(). */
namespace
{
thread_local ads_i32 lastError = 0;

ads_i32 Done(long status)
{
	lastError = static_cast<ads_i32>(status);
	return lastError;
}
}

namespace bhf
{
namespace ads
{
ads_i32 SetLastError(long status)
{
	return Done(status);
}
}
}

namespace
{

/** Default port of the functions without port parameter */
std::mutex defaultPortMutex;
ads_i32 defaultPort = 0;

ads_i32 DefaultPort()
{
	std::lock_guard<std::mutex> lock(defaultPortMutex);
	return defaultPort;
}

struct RouterNotification {
	PAmsRouterNotificationFuncEx func = nullptr;
	PAmsRouterNotificationFuncEx3 func3 = nullptr;
	void *hUser = nullptr;
};
std::mutex routerNotificationMutex;
std::map<uint16_t, RouterNotification> routerNotifications;

// router events come only from the local router
void OnRouterEvent(uint16_t port, uint32_t event)
{
	RouterNotification n;
	{
		std::lock_guard<std::mutex> lock(routerNotificationMutex);
		const auto it = routerNotifications.find(port);
		if (it == routerNotifications.end()) {
			return;
		}
		n = it->second;
	}
	if (n.func) {
		n.func(static_cast<ads_i32>(event));
	}
	if (n.func3) {
		n.func3(static_cast<ads_i32>(event), n.hUser);
	}
}

long RegisterRouterNotification(ads_i32 port, const RouterNotification &n)
{
	ASSERT_PORT(port);
	if (!n.func && !n.func3) {
		return ADSERR_CLIENT_INVALIDPARM;
	}
	if (!IsLocalPort((uint16_t)port)) {
		return GetOtherBackend((uint16_t)port) ?
			       ADSERR_DEVICE_SRVNOTSUPP :
			       ADSERR_CLIENT_PORTNOTOPEN;
	}
	static std::once_flag once;
	std::call_once(once, [] {
		GetLocalRouter().SetRouterEventHandler(OnRouterEvent);
	});
	std::lock_guard<std::mutex> lock(routerNotificationMutex);
	routerNotifications[(uint16_t)port] = n;
	return 0;
}

long UnRegisterRouterNotification(ads_i32 port)
{
	ASSERT_PORT(port);
	std::lock_guard<std::mutex> lock(routerNotificationMutex);
	return routerNotifications.erase((uint16_t)port) ?
		       0 :
		       ADSERR_CLIENT_PORTNOTOPEN;
}
}

#ifdef BHF_ADS_EXPORT_C
extern "C" {
#endif
ads_i32 AdsPortCloseEx(ads_i32 port)
{
	UnRegisterRouterNotification(port);
	return Done(AdsPortCloseExImpl(port));
}

ads_i32 AdsPortOpenEx()
{
	const auto port = AdsPortOpenExImpl();
	Done(port ? 0 : GLOBALERR_TARGET_PORT);
	return static_cast<ads_i32>(port);
}

ads_i32 AdsGetLocalAddressEx(ads_i32 port, AmsAddr *pAddr)
{
	return Done(AdsGetLocalAddressExImpl(port, pAddr));
}

ads_i32 AdsSyncReadReqEx2(ads_i32 port, const AmsAddr *pAddr,
			  ads_ui32 indexGroup, ads_ui32 indexOffset,
			  ads_ui32 bufferLength, void *buffer,
			  ads_ui32 *bytesRead)
{
	uint32_t n = 0;
	const auto status = AdsSyncReadReqEx2Impl(port, pAddr, indexGroup,
						  indexOffset, bufferLength,
						  buffer, &n);
	if (bytesRead) {
		*bytesRead = n;
	}
	return Done(status);
}

ads_i32 AdsSyncReadDeviceInfoReqEx(ads_i32 port, const AmsAddr *pAddr,
				   char *devName, AdsVersion *version)
{
	return Done(AdsSyncReadDeviceInfoReqExImpl(port, pAddr, devName,
						   version));
}

ads_i32 AdsSyncReadStateReqEx(ads_i32 port, const AmsAddr *pAddr,
			      uint16_t *adsState, uint16_t *devState)
{
	return Done(AdsSyncReadStateReqExImpl(port, pAddr, adsState,
					      devState));
}

ads_i32 AdsSyncReadWriteReqEx2(ads_i32 port, const AmsAddr *pAddr,
			       ads_ui32 indexGroup, ads_ui32 indexOffset,
			       ads_ui32 readLength, void *readData,
			       ads_ui32 writeLength, const void *writeData,
			       ads_ui32 *bytesRead)
{
	uint32_t n = 0;
	const auto status = AdsSyncReadWriteReqEx2Impl(
		port, pAddr, indexGroup, indexOffset, readLength, readData,
		writeLength, writeData, &n);
	if (bytesRead) {
		*bytesRead = n;
	}
	return Done(status);
}

ads_i32 AdsSyncWriteReqEx(ads_i32 port, const AmsAddr *pAddr,
			  ads_ui32 indexGroup, ads_ui32 indexOffset,
			  ads_ui32 bufferLength, const void *buffer)
{
	return Done(AdsSyncWriteReqExImpl(port, pAddr, indexGroup, indexOffset,
					  bufferLength, buffer));
}

ads_i32 AdsSyncWriteControlReqEx(ads_i32 port, const AmsAddr *pAddr,
				 uint16_t adsState, uint16_t devState,
				 ads_ui32 bufferLength, const void *buffer)
{
	return Done(AdsSyncWriteControlReqExImpl(port, pAddr, adsState,
						 devState, bufferLength,
						 buffer));
}

ads_i32 AdsSyncAddDeviceNotificationReqEx(ads_i32 port, const AmsAddr *pAddr,
					  ads_ui32 indexGroup,
					  ads_ui32 indexOffset,
					  const AdsNotificationAttrib *pAttrib,
					  PAdsNotificationFuncEx pFunc,
					  ads_ui32 hUser,
					  ads_ui32 *pNotification)
{
	uint32_t h = 0;
	const auto status = AdsSyncAddDeviceNotificationReqExImpl(
		port, pAddr, indexGroup, indexOffset, pAttrib,
		NotificationCallback{ pFunc }, hUser, pNotification ? &h : nullptr);
	if (pNotification) {
		*pNotification = h;
	}
	return Done(status);
}

ads_i32 AdsSyncDelDeviceNotificationReqEx(ads_i32 port, const AmsAddr *pAddr,
					  ads_ui32 hNotification)
{
	return Done(AdsSyncDelDeviceNotificationReqExImpl(port, pAddr,
							  hNotification));
}

ads_i32 AdsSyncGetTimeoutEx(ads_i32 port, ads_i32 *timeout)
{
	uint32_t t = 0;
	const auto status =
		AdsSyncGetTimeoutExImpl(port, timeout ? &t : nullptr);
	if (timeout && !status) {
		*timeout = static_cast<ads_i32>(t);
	}
	return Done(status);
}

ads_i32 AdsSyncSetTimeoutEx(ads_i32 port, ads_i32 timeout)
{
	if (timeout < 0) {
		return Done(ADSERR_CLIENT_INVALIDPARM);
	}
	return Done(AdsSyncSetTimeoutExImpl(port, static_cast<uint32_t>(timeout)));
}

ads_i32 AdsGetDllVersion(void)
{
	// version.revision.build = major.minor.patch of libads, set by the build
	const AdsVersion version{ LIBADS_VERSION_MAJOR, LIBADS_VERSION_MINOR,
				  LIBADS_VERSION_PATCH };
	ads_i32 packed = 0;
	memcpy(&packed, &version, sizeof(version));
	return packed;
}

ads_i32 AdsGetLastError(void)
{
	return lastError;
}

ads_i32 AdsPortOpen(void)
{
	std::lock_guard<std::mutex> lock(defaultPortMutex);
	if (!defaultPort) {
		defaultPort = AdsPortOpenEx();
	} else {
		Done(0);
	}
	return defaultPort;
}

ads_i32 AdsPortClose(void)
{
	ads_i32 port;
	{
		std::lock_guard<std::mutex> lock(defaultPortMutex);
		port = defaultPort;
		defaultPort = 0;
	}
	if (!port) {
		return Done(ADSERR_CLIENT_PORTNOTOPEN);
	}
	return AdsPortCloseEx(port);
}

ads_i32 AdsGetLocalAddress(AmsAddr *pAddr)
{
	return AdsGetLocalAddressEx(DefaultPort(), pAddr);
}

ads_i32 AdsSyncReadReq(const AmsAddr *pAddr, ads_ui32 indexGroup,
		       ads_ui32 indexOffset, ads_ui32 length, void *pData)
{
	return AdsSyncReadReqEx2(DefaultPort(), pAddr, indexGroup, indexOffset,
				 length, pData, nullptr);
}

ads_i32 AdsSyncReadReqEx(const AmsAddr *pAddr, ads_ui32 indexGroup,
			 ads_ui32 indexOffset, ads_ui32 length, void *pData,
			 ads_ui32 *pcbReturn)
{
	return AdsSyncReadReqEx2(DefaultPort(), pAddr, indexGroup, indexOffset,
				 length, pData, pcbReturn);
}

ads_i32 AdsSyncWriteReq(const AmsAddr *pAddr, ads_ui32 indexGroup,
			ads_ui32 indexOffset, ads_ui32 length,
			const void *pData)
{
	return AdsSyncWriteReqEx(DefaultPort(), pAddr, indexGroup, indexOffset,
				 length, pData);
}

ads_i32 AdsSyncReadWriteReq(const AmsAddr *pAddr, ads_ui32 indexGroup,
			    ads_ui32 indexOffset, ads_ui32 cbReadLength,
			    void *pReadData, ads_ui32 cbWriteLength,
			    const void *pWriteData)
{
	return AdsSyncReadWriteReqEx2(DefaultPort(), pAddr, indexGroup,
				      indexOffset, cbReadLength, pReadData,
				      cbWriteLength, pWriteData, nullptr);
}

ads_i32 AdsSyncReadWriteReqEx(const AmsAddr *pAddr, ads_ui32 indexGroup,
			      ads_ui32 indexOffset, ads_ui32 cbReadLength,
			      void *pReadData, ads_ui32 cbWriteLength,
			      const void *pWriteData, ads_ui32 *pcbReturn)
{
	return AdsSyncReadWriteReqEx2(DefaultPort(), pAddr, indexGroup,
				      indexOffset, cbReadLength, pReadData,
				      cbWriteLength, pWriteData, pcbReturn);
}

ads_i32 AdsSyncReadDeviceInfoReq(const AmsAddr *pAddr, char *pDevName,
				 AdsVersion *pVersion)
{
	return AdsSyncReadDeviceInfoReqEx(DefaultPort(), pAddr, pDevName,
					  pVersion);
}

ads_i32 AdsSyncWriteControlReq(const AmsAddr *pAddr, ads_ui16 adsState,
			       ads_ui16 deviceState, ads_ui32 length,
			       const void *pData)
{
	return AdsSyncWriteControlReqEx(DefaultPort(), pAddr, adsState,
					deviceState, length, pData);
}

ads_i32 AdsSyncReadStateReq(const AmsAddr *pAddr, ads_ui16 *pAdsState,
			    ads_ui16 *pDeviceState)
{
	return AdsSyncReadStateReqEx(DefaultPort(), pAddr, pAdsState,
				     pDeviceState);
}

ads_i32 AdsSyncAddDeviceNotificationReq(const AmsAddr *pAddr,
					ads_ui32 indexGroup,
					ads_ui32 indexOffset,
					const AdsNotificationAttrib *pNoteAttrib,
					PAdsNotificationFuncEx pNoteFunc,
					ads_ui32 hUser, ads_ui32 *pNotification)
{
	return AdsSyncAddDeviceNotificationReqEx(DefaultPort(), pAddr,
						 indexGroup, indexOffset,
						 pNoteAttrib, pNoteFunc, hUser,
						 pNotification);
}

ads_i32 AdsSyncDelDeviceNotificationReq(const AmsAddr *pAddr,
					ads_ui32 hNotification)
{
	return AdsSyncDelDeviceNotificationReqEx(DefaultPort(), pAddr,
						 hNotification);
}

ads_i32 AdsSyncSetTimeout(ads_i32 nMs)
{
	return AdsSyncSetTimeoutEx(DefaultPort(), nMs);
}

ads_i32 AdsSyncGetTimeout(ads_i32 *pnMs)
{
	return AdsSyncGetTimeoutEx(DefaultPort(), pnMs);
}

ads_i32 AdsAmsPortEnabledEx(ads_i32 nPort, ads_bool *pbEnabled)
{
	if (!pbEnabled) {
		return Done(ADSERR_CLIENT_INVALIDPARM);
	}
	*pbEnabled = 0;
	if ((nPort <= 0) || (nPort > UINT16_MAX)) {
		return Done(ADSERR_CLIENT_PORTNOTOPEN);
	}
	const auto port = (uint16_t)nPort;
	if (IsLocalPort(port)) {
		*pbEnabled = GetLocalRouter().IsConnected(port);
	} else if (GetOtherBackend(port)) {
		*pbEnabled = 1;
	} else {
		return Done(ADSERR_CLIENT_PORTNOTOPEN);
	}
	return Done(0);
}

ads_i32 AdsAmsPortEnabled(ads_bool *pbEnabled)
{
	return AdsAmsPortEnabledEx(DefaultPort(), pbEnabled);
}

ads_i32 AdsAmsRegisterRouterNotificationEx2(ads_i32 nPort,
					    PAmsRouterNotificationFuncEx3 pNoteFunc,
					    void *hUser)
{
	RouterNotification n;
	n.func3 = pNoteFunc;
	n.hUser = hUser;
	return Done(RegisterRouterNotification(nPort, n));
}

ads_i32 AdsAmsUnRegisterRouterNotificationEx2(ads_i32 nPort)
{
	return Done(UnRegisterRouterNotification(nPort));
}

ads_i32 AdsAmsRegisterRouterNotification(PAmsRouterNotificationFuncEx pNoteFunc)
{
	RouterNotification n;
	n.func = pNoteFunc;
	return Done(RegisterRouterNotification(DefaultPort(), n));
}

ads_i32 AdsAmsUnRegisterRouterNotification(void)
{
	return Done(UnRegisterRouterNotification(DefaultPort()));
}
#ifdef BHF_ADS_EXPORT_C
}
#endif

ads_i32 AdsSyncAddDeviceNotificationReqEx(ads_i32 port, const AmsAddr *pAddr,
					  ads_ui32 indexGroup,
					  ads_ui32 indexOffset,
					  const AdsNotificationAttrib *pAttrib,
					  PAdsNotificationFuncExConst pFunc,
					  ads_ui32 hUser,
					  ads_ui32 *pNotification)
{
	uint32_t h = 0;
	const auto status = AdsSyncAddDeviceNotificationReqExImpl(
		port, pAddr, indexGroup, indexOffset, pAttrib,
		NotificationCallback{ pFunc }, hUser, pNotification ? &h : nullptr);
	if (pNotification) {
		*pNotification = h;
	}
	return Done(status);
}
