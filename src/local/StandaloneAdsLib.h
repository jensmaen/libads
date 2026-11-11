// SPDX-License-Identifier: MIT
/** @file
   Copyright (c) 2015 - 2022 Beckhoff Automation GmbH & Co. KG
 */

#pragma once

#include "AdsDef.h"

#include <type_traits>

#ifdef BHF_ADS_EXPORT_C
extern "C" {
#endif

/**
 * The connection (communication port) to the message router is
 * closed. The port to be closed must previously have been opened via
 * an AdsPortOpenEx() call.
 * @param[in] port port number of an Ads port that had previously been opened with AdsPortOpenEx().
 * @return [ADS Return Code](https://infosys.beckhoff.com/content/1031/tcadscommon/html/ads_returncodes.htm?id=1666172286265530469)
 */
ads_i32 AdsPortCloseEx(ads_i32 port);

/**
 * Establishes a connection (communication port) to the local TwinCAT
 * router (TCP 127.0.0.1:48898), like the TcAdsDll. The router assigns the
 * port number and the local AmsNetId. The port number returned by
 * AdsPortOpenEx() is required as parameter for further AdsLib function calls.
 * Unlike Beckhoff's standalone AdsLib, this does not use ADS routes; the
 * route way is AdsPortOpenRouteEx() of libads-client.
 * @return port number of a new Ads port or 0 if the local router is not reachable
 */
ads_i32 AdsPortOpenEx();

/**
 * Returns the local NetId and port number.
 * @param[in] port port number of an Ads port that had previously been opened with AdsPortOpenEx().
 * @param[out] pAddr Pointer to the structure of type AmsAddr.
 * @return [ADS Return Code](https://infosys.beckhoff.com/content/1031/tcadscommon/html/ads_returncodes.htm?id=1666172286265530469)
 */
ads_i32 AdsGetLocalAddressEx(ads_i32 port, AmsAddr *pAddr);

/**
 * Alters the timeout for the ADS functions. The standard value is 5000 ms.
 * @param[in] port port number of an Ads port that had previously been opened with AdsPortOpenEx().
 * @param[in] timeout Timeout in ms.
 * @return [ADS Return Code](https://infosys.beckhoff.com/content/1031/tcadscommon/html/ads_returncodes.htm?id=1666172286265530469)
 */
ads_i32 AdsSyncSetTimeoutEx(ads_i32 port, ads_i32 timeout);

/**
 * @name Functions of the TcAdsDll
 * The functions without port parameter work on one default port per process,
 * opened with AdsPortOpen() at the local router, as with the TcAdsDll. They
 * return ADSERR_CLIENT_PORTNOTOPEN while it is not open.
 */
///@{

/** Version of this library, packed like an AdsVersion. */
ads_i32 AdsGetDllVersion(void);

/** Error code of the last call of this interface in the calling thread. */
ads_i32 AdsGetLastError(void);

/**
 * Opens the default port at the local router, or returns it if it is
 * already open.
 * @return port number, 0 if the local router is not reachable
 */
ads_i32 AdsPortOpen(void);

/** Closes the default port. */
ads_i32 AdsPortClose(void);

ads_i32 AdsGetLocalAddress(AmsAddr *pAddr);
ads_i32 AdsSyncReadReq(const AmsAddr *pAddr, ads_ui32 indexGroup,
		       ads_ui32 indexOffset, ads_ui32 length, void *pData);
ads_i32 AdsSyncReadReqEx(const AmsAddr *pAddr, ads_ui32 indexGroup,
			 ads_ui32 indexOffset, ads_ui32 length, void *pData,
			 ads_ui32 *pcbReturn);
ads_i32 AdsSyncWriteReq(const AmsAddr *pAddr, ads_ui32 indexGroup,
			ads_ui32 indexOffset, ads_ui32 length,
			const void *pData);
ads_i32 AdsSyncReadWriteReq(const AmsAddr *pAddr, ads_ui32 indexGroup,
			    ads_ui32 indexOffset, ads_ui32 cbReadLength,
			    void *pReadData, ads_ui32 cbWriteLength,
			    const void *pWriteData);
ads_i32 AdsSyncReadWriteReqEx(const AmsAddr *pAddr, ads_ui32 indexGroup,
			      ads_ui32 indexOffset, ads_ui32 cbReadLength,
			      void *pReadData, ads_ui32 cbWriteLength,
			      const void *pWriteData, ads_ui32 *pcbReturn);
ads_i32 AdsSyncReadDeviceInfoReq(const AmsAddr *pAddr, char *pDevName,
				 AdsVersion *pVersion);
ads_i32 AdsSyncWriteControlReq(const AmsAddr *pAddr, ads_ui16 adsState,
			       ads_ui16 deviceState, ads_ui32 length,
			       const void *pData);
ads_i32 AdsSyncReadStateReq(const AmsAddr *pAddr, ads_ui16 *pAdsState,
			    ads_ui16 *pDeviceState);
ads_i32 AdsSyncAddDeviceNotificationReq(const AmsAddr *pAddr,
					ads_ui32 indexGroup,
					ads_ui32 indexOffset,
					const AdsNotificationAttrib *pNoteAttrib,
					PAdsNotificationFuncEx pNoteFunc,
					ads_ui32 hUser, ads_ui32 *pNotification);
ads_i32 AdsSyncDelDeviceNotificationReq(const AmsAddr *pAddr,
					ads_ui32 hNotification);
ads_i32 AdsSyncSetTimeout(ads_i32 nMs);
ads_i32 AdsSyncGetTimeout(ads_i32 *pnMs);

/**
 * Tells whether the port can reach the router: open and, for a port at the
 * local router, still connected.
 */
ads_i32 AdsAmsPortEnabled(ads_bool *pbEnabled);
ads_i32 AdsAmsPortEnabledEx(ads_i32 nPort, ads_bool *pbEnabled);

/**
 * Calls pNoteFunc with an AmsRouterEvent when the local router stops, starts
 * or goes away. The call comes from a thread of the library, the functions of
 * this interface may be used in it. Only for ports at the local router.
 */
ads_i32 AdsAmsRegisterRouterNotification(PAmsRouterNotificationFuncEx pNoteFunc);
ads_i32 AdsAmsUnRegisterRouterNotification(void);
ads_i32 AdsAmsRegisterRouterNotificationEx2(ads_i32 nPort,
					    PAmsRouterNotificationFuncEx3 pNoteFunc,
					    void *hUser);
ads_i32 AdsAmsUnRegisterRouterNotificationEx2(ads_i32 nPort);
///@}

/**
 * Reads data synchronously from an ADS server.
 * @param[in] port port number of an Ads port that had previously been opened with AdsPortOpenEx().
 * @param[in] pAddr Structure with NetId and port number of the ADS server.
 * @param[in] indexGroup Index Group.
 * @param[in] indexOffset Index Offset.
 * @param[in] bufferLength Length of the data in bytes.
 * @param[out] buffer Pointer to a data buffer that will receive the data.
 * @param[out] bytesRead pointer to a variable. If successful, this variable will return the number of actually read data bytes.
 * @return [ADS Return Code](https://infosys.beckhoff.com/content/1031/tcadscommon/html/ads_returncodes.htm?id=1666172286265530469)
 */
ads_i32 AdsSyncReadReqEx2(ads_i32 port, const AmsAddr *pAddr, ads_ui32 indexGroup,
		       ads_ui32 indexOffset, ads_ui32 bufferLength,
		       void *buffer, ads_ui32 *bytesRead);

/**
 * Reads the identification and version number of an ADS server.
 * @param[in] port port number of an Ads port that had previously been opened with AdsPortOpenEx().
 * @param[in] pAddr Structure with NetId and port number of the ADS server.
 * @param[out] devName Pointer to a character string of at least 16 bytes, that will receive the name of the ADS device.
 * @param[out] version Address of a variable of type AdsVersion, which will receive the version number, revision number and the build number.
 * @return [ADS Return Code](https://infosys.beckhoff.com/content/1031/tcadscommon/html/ads_returncodes.htm?id=1666172286265530469)
 */
ads_i32 AdsSyncReadDeviceInfoReqEx(ads_i32 port, const AmsAddr *pAddr, char *devName,
				AdsVersion *version);

/**
 * Reads the ADS status and the device status from an ADS server.
 * @param[in] port port number of an Ads port that had previously been opened with AdsPortOpenEx().
 * @param[in] pAddr Structure with NetId and port number of the ADS server.
 * @param[out] adsState Address of a variable that will receive the ADS status (see data type [ADSSTATE](https://infosys.beckhoff.com/content/1031/tcadsdll2/html/tcadsdll_enumadsstate.htm?id=2714257434501002224).
 * @param[out] devState Address of a variable that will receive the device status.
 * @return [ADS Return Code](https://infosys.beckhoff.com/content/1031/tcadscommon/html/ads_returncodes.htm?id=1666172286265530469)
 */
ads_i32 AdsSyncReadStateReqEx(ads_i32 port, const AmsAddr *pAddr, uint16_t *adsState,
			   uint16_t *devState);

/**
 * Writes data synchronously into an ADS server and receives data back from the ADS server.
 * @param[in] port  port number of an Ads port that had previously been opened with AdsPortOpenEx().
 * @param[in] pAddr Structure with NetId and port number of the ADS server.
 * @param[in] indexGroup Index Group.
 * @param[in] indexOffset Index Offset.
 * @param[in] readLength Length, in bytes, of the read buffer readData.
 * @param[out] readData Buffer for data read from the ADS server.
 * @param[in] writeLength Length of the data, in bytes, send to the ADS server.
 * @param[in] writeData Buffer with data send to the ADS server.
 * @param[out] bytesRead pointer to a variable. If successful, this variable will return the number of actually read data bytes.
 * @return [ADS Return Code](https://infosys.beckhoff.com/content/1031/tcadscommon/html/ads_returncodes.htm?id=1666172286265530469)
 */
ads_i32 AdsSyncReadWriteReqEx2(ads_i32 port, const AmsAddr *pAddr,
			    ads_ui32 indexGroup, ads_ui32 indexOffset,
			    ads_ui32 readLength, void *readData,
			    ads_ui32 writeLength, const void *writeData,
			    ads_ui32 *bytesRead);

/**
 * Writes data synchronously to an ADS server.
 * @param[in] port port number of an Ads port that had previously been opened with AdsPortOpenEx().
 * @param[in] pAddr Structure with NetId and port number of the ADS server.
 * @param[in] indexGroup Index Group.
 * @param[in] indexOffset Index Offset.
 * @param[in] bufferLength Length of the data, in bytes, send to the ADS server.
 * @param[in] buffer Buffer with data send to the ADS server.
 * @return [ADS Return Code](https://infosys.beckhoff.com/content/1031/tcadscommon/html/ads_returncodes.htm?id=1666172286265530469)
 */
ads_i32 AdsSyncWriteReqEx(ads_i32 port, const AmsAddr *pAddr, ads_ui32 indexGroup,
		       ads_ui32 indexOffset, ads_ui32 bufferLength,
		       const void *buffer);

/**
 * Changes the ADS status and the device status of an ADS server.
 * @param[in] port port number of an Ads port that had previously been opened with AdsPortOpenEx().
 * @param[in] pAddr Structure with NetId and port number of the ADS server.
 * @param[in] adsState New ADS status.
 * @param[in] devState New device status.
 * @param[in] bufferLength Length of the additional data, in bytes, send to the ADS server.
 * @param[in] buffer Buffer with additional data send to the ADS server.
 * @return [ADS Return Code](https://infosys.beckhoff.com/content/1031/tcadscommon/html/ads_returncodes.htm?id=1666172286265530469)
 */
ads_i32 AdsSyncWriteControlReqEx(ads_i32 port, const AmsAddr *pAddr,
			      uint16_t adsState, uint16_t devState,
			      ads_ui32 bufferLength, const void *buffer);

/**
 * A notification is defined within an ADS server (e.g. PLC). When a
 * certain event occurs a function (the callback function) is invoked in
 * the ADS client (C program).
 * @param[in] port port number of an Ads port that had previously been opened with AdsPortOpenEx().
 * @param[in] pAddr Structure with NetId and port number of the ADS server.
 * @param[in] indexGroup Index Group.
 * @param[in] indexOffset Index Offset.
 * @param[in] pAttrib Pointer to the structure that contains further information.
 * @param[in] pFunc Pointer to the structure describing the callback function.
 * @param[in] hUser 32-bit value that is passed to the callback function.
 * @param[out] pNotification Address of the variable that will receive the handle of the notification.
 * @return [ADS Return Code](https://infosys.beckhoff.com/content/1031/tcadscommon/html/ads_returncodes.htm?id=1666172286265530469)
 */
ads_i32 AdsSyncAddDeviceNotificationReqEx(ads_i32 port, const AmsAddr *pAddr,
				       ads_ui32 indexGroup,
				       ads_ui32 indexOffset,
				       const AdsNotificationAttrib *pAttrib,
				       PAdsNotificationFuncEx pFunc,
				       ads_ui32 hUser, ads_ui32 *pNotification);

/**
 * A notification defined previously is deleted from an ADS server.
 * @param[in] port port number of an Ads port that had previously been opened with AdsPortOpenEx().
 * @param[in] pAddr Structure with NetId and port number of the ADS server.
 * @param[in] hNotification Address of the variable that contains the handle of the notification.
 * @return [ADS Return Code](https://infosys.beckhoff.com/content/1031/tcadscommon/html/ads_returncodes.htm?id=1666172286265530469)
 */
ads_i32 AdsSyncDelDeviceNotificationReqEx(ads_i32 port, const AmsAddr *pAddr,
				       ads_ui32 hNotification);

/**
 * Read the configured timeout for the ADS functions. The standard value is 5000 ms.
 * @param[in] port port number of an Ads port that had previously been opened with AdsPortOpenEx().
 * @param[out] timeout Buffer to store timeout value in ms.
 * @return [ADS Return Code](https://infosys.beckhoff.com/content/1031/tcadscommon/html/ads_returncodes.htm?id=1666172286265530469)
 */
ads_i32 AdsSyncGetTimeoutEx(ads_i32 port, ads_i32 *timeout);
#ifdef BHF_ADS_EXPORT_C
}
#endif

/**
 * Same as above for a callback function as in Beckhoff's standalone AdsLib
 * (const parameters).
 */
ads_i32 AdsSyncAddDeviceNotificationReqEx(ads_i32 port, const AmsAddr *pAddr,
					  ads_ui32 indexGroup,
					  ads_ui32 indexOffset,
					  const AdsNotificationAttrib *pAttrib,
					  PAdsNotificationFuncExConst pFunc,
					  ads_ui32 hUser,
					  ads_ui32 *pNotification);

#if defined(_WIN32) && !defined(POSIX)
/**
 * @name uint32_t outputs as in Beckhoff's standalone AdsLib
 * On Windows ads_ui32 is unsigned long, not uint32_t. These templates take
 * uint32_t pointers as well; for nullptr or NULL the functions above are
 * chosen, since a template cannot deduce from them.
 */
///@{
namespace bhf
{
namespace ads
{
template <class T> using IfUint32 = typename std::enable_if<
	std::is_same<T, uint32_t>::value, ads_i32>::type;
}
}

template <class T>
bhf::ads::IfUint32<T>
AdsSyncReadReqEx2(ads_i32 port, const AmsAddr *pAddr, ads_ui32 indexGroup,
		  ads_ui32 indexOffset, ads_ui32 bufferLength, void *buffer,
		  T *bytesRead)
{
	ads_ui32 n = 0;
	const auto status = AdsSyncReadReqEx2(port, pAddr, indexGroup,
					      indexOffset, bufferLength,
					      buffer, &n);
	if (bytesRead) {
		*bytesRead = n;
	}
	return status;
}

template <class T>
bhf::ads::IfUint32<T>
AdsSyncReadWriteReqEx2(ads_i32 port, const AmsAddr *pAddr,
		       ads_ui32 indexGroup, ads_ui32 indexOffset,
		       ads_ui32 readLength, void *readData,
		       ads_ui32 writeLength, const void *writeData,
		       T *bytesRead)
{
	ads_ui32 n = 0;
	const auto status = AdsSyncReadWriteReqEx2(
		port, pAddr, indexGroup, indexOffset, readLength, readData,
		writeLength, writeData, &n);
	if (bytesRead) {
		*bytesRead = n;
	}
	return status;
}

template <class T, class Callback>
bhf::ads::IfUint32<T> AdsSyncAddDeviceNotificationReqEx(
	ads_i32 port, const AmsAddr *pAddr, ads_ui32 indexGroup,
	ads_ui32 indexOffset, const AdsNotificationAttrib *pAttrib,
	Callback pFunc, ads_ui32 hUser, T *pNotification)
{
	ads_ui32 h = 0;
	const auto status = AdsSyncAddDeviceNotificationReqEx(
		port, pAddr, indexGroup, indexOffset, pAttrib, pFunc, hUser,
		pNotification ? &h : nullptr);
	if (pNotification) {
		*pNotification = h;
	}
	return status;
}

template <class T>
bhf::ads::IfUint32<T> AdsSyncGetTimeoutEx(ads_i32 port, T *timeout)
{
	ads_i32 t = 0;
	const auto status = AdsSyncGetTimeoutEx(port, timeout ? &t : nullptr);
	if (timeout && !status) {
		*timeout = static_cast<uint32_t>(t);
	}
	return status;
}
///@}
#endif
