/*
 *  Copyright (C) 2010-2021 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2020 Alwin Esch (Team Kodi)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <kodi/AddonBase.h>

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 34890
#define DEFAULT_CHARCONV false
#define DEFAULT_HANDLE_MSG true
#define DEFAULT_PRIORITY 0
#define DEFAULT_TIMEOUT 3
#define DEFAULT_AUTOGROUPS false
#define DEFAULT_CHUNKSIZE 65536

class ATTRIBUTE_HIDDEN CVNSISettings
{
public:
  static CVNSISettings& Get();

  bool Load();
  ADDON_STATUS SetSetting(const std::string& settingName, const kodi::CSettingValue& settingValue);

  const std::string& GetHostname() const { return m_szHostname; }
  const std::string& GetWolMac() const { return m_szWolMac; }
  int GetPort() const { return m_iPort; }
  int GetConnectTimeout() const { return m_iConnectTimeout; }
  int GetPriority() const { return m_iPriority; }
  bool GetCharsetConv() const { return m_bCharsetConv; }
  bool GetAutoChannelGroups() const { return m_bAutoChannelGroups; }
  int GetTimeshift() const { return m_iTimeshift; }
  const std::string& GetIconPath() const { return m_szIconPath; }
  int GetChunkSize() const { return m_iChunkSize; }

private:
  CVNSISettings() = default;
  CVNSISettings(const CVNSISettings&) = delete;
  CVNSISettings& operator=(const CVNSISettings&) = delete;

  std::string m_szHostname = DEFAULT_HOST; /*!< hostname or ip-address of the server */
  std::string m_szWolMac;
  int m_iPort = DEFAULT_PORT; /*!< TCP port of the vnsi server */
  int m_iConnectTimeout = DEFAULT_TIMEOUT; /*!< Network connection / read timeout in seconds */
  int m_iPriority =
      DEFAULT_PRIORITY; /*!< The Priority this client have in response to other clients */
  bool m_bCharsetConv =
      DEFAULT_CHARCONV; /*!< Convert VDR's incoming strings to UTF8 character set */
  bool m_bAutoChannelGroups = DEFAULT_AUTOGROUPS;
  int m_iTimeshift = 1;
  std::string m_szIconPath; /*!< path to channel icons */
  int m_iChunkSize = DEFAULT_CHUNKSIZE;
};
