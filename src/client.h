/*
 *  Copyright (C) 2010-2020 Team Kodi
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "kodi/libXBMC_addon.h"
#include "kodi/libXBMC_pvr.h"
#include "kodi/libKODI_guilib.h"

#define DEFAULT_HOST          "127.0.0.1"
#define DEFAULT_PORT          34890
#define DEFAULT_CHARCONV      false
#define DEFAULT_HANDLE_MSG    true
#define DEFAULT_PRIORITY      0
#define DEFAULT_TIMEOUT       3
#define DEFAULT_AUTOGROUPS    false
#define DEFAULT_CHUNKSIZE     65536

extern bool         m_bCreated;
extern std::string  g_szHostname;         ///< hostname or ip-address of the server
extern int          g_iPort;              ///< TCP port of the vnsi server
extern int          g_iConnectTimeout;    ///< Network connection / read timeout in seconds
extern int          g_iPriority;          ///< The Priority this client have in response to other clients
extern bool         g_bCharsetConv;       ///< Convert VDR's incoming strings to UTF8 character set
extern int          g_iTimeshift;
extern std::string  g_szIconPath;         ///< path to channel icons

extern ADDON::CHelper_libXBMC_addon *XBMC;
extern CHelper_libKODI_guilib *GUI;
extern CHelper_libXBMC_pvr   *PVR;
