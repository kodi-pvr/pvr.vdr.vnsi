/*
 *  Copyright (C) 2010-2021 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2020 Alwin Esch (Team Kodi)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Settings.h"

#include <kodi/General.h>

namespace
{
int prioVals[] = {0,  5,  10, 15, 20, 25, 30, 35, 40, 45, 50,
                  55, 60, 65, 70, 75, 80, 85, 90, 95, 99, 100};
}

CVNSISettings& CVNSISettings::Get()
{
  static CVNSISettings settings;
  return settings;
}

bool CVNSISettings::Load()
{
  // Read setting "host" from settings.xml
  if (!kodi::addon::CheckSettingString("host", m_szHostname))
  {
    // If setting is unknown fallback to defaults
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'host' setting, falling back to '%s' as default",
              DEFAULT_HOST);
    m_szHostname = DEFAULT_HOST;
  }

  // Read setting "wol_mac" from settings.xml
  if (!kodi::addon::CheckSettingString("wol_mac", m_szWolMac))
  {
    // If setting is unknown fallback to empty default
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'wol_mac' setting, falling back to default");
    m_szWolMac = "";
  }

  // Read setting "port" from settings.xml
  if (!kodi::addon::CheckSettingInt("port", m_iPort))
  {
    // If setting is unknown fallback to defaults
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'port' setting, falling back to '%i' as default",
              DEFAULT_PORT);
    m_iPort = DEFAULT_PORT;
  }

  // Read setting "priority" from settings.xml
  int prio = DEFAULT_PRIORITY;
  if (kodi::addon::CheckSettingInt("priority", prio))
  {
    m_iPriority = prioVals[prio];
  }
  else
  {
    // If setting is unknown fallback to defaults
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'priority' setting, falling back to %i as default",
              -1);
    m_iPriority = DEFAULT_PRIORITY;
  }

  /* Read setting "timeshift" from settings.xml */
  if (!kodi::addon::CheckSettingInt("timeshift", m_iTimeshift))
  {
    // If setting is unknown fallback to defaults
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'timeshift' setting, falling back to %i as default",
              1);
    m_iTimeshift = 1;
  }

  // Read setting "convertchar" from settings.xml
  if (!kodi::addon::CheckSettingBoolean("convertchar", m_bCharsetConv))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'convertchar' setting, falling back to 'false' as default");
    m_bCharsetConv = DEFAULT_CHARCONV;
  }

  // Read setting "timeout" from settings.xml
  if (!kodi::addon::CheckSettingInt("timeout", m_iConnectTimeout))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'timeout' setting, falling back to %i seconds as default",
              DEFAULT_TIMEOUT);
    m_iConnectTimeout = DEFAULT_TIMEOUT;
  }

  // Read setting "autochannelgroups" from settings.xml
  if (!kodi::addon::CheckSettingBoolean("autochannelgroups", m_bAutoChannelGroups))
  {
    // If setting is unknown fallback to defaults
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'autochannelgroups' setting, falling back to 'false' as default");
    m_bAutoChannelGroups = DEFAULT_AUTOGROUPS;
  }

  // Read setting "iconpath" from settings.xml
  if (!kodi::addon::CheckSettingString("iconpath", m_szIconPath))
  {
    // If setting is unknown fallback to defaults
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'iconpath' setting");
    m_szIconPath = "";
  }

  // Read setting "chunksize" from settings.xml
  if (!kodi::addon::CheckSettingInt("chunksize", m_iChunkSize))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'chunksize' setting, falling back to %i as default",
              DEFAULT_CHUNKSIZE);
    m_iChunkSize = DEFAULT_CHUNKSIZE;
  }

  return true;
}

ADDON_STATUS CVNSISettings::SetSetting(const std::string& settingName,
                                       const kodi::addon::CSettingValue& settingValue)
{
  if (settingName == "host")
  {
    std::string tmp_sHostname;
    kodi::Log(ADDON_LOG_INFO, "Changed Setting 'host' from %s to %s", m_szHostname.c_str(),
              settingValue.GetString().c_str());
    tmp_sHostname = m_szHostname;
    m_szHostname = settingValue.GetString();
    if (tmp_sHostname != m_szHostname)
      return ADDON_STATUS_NEED_RESTART;
  }
  else if (settingName == "wol_mac")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed Setting 'wol_mac'");
    std::string tmp_sWol_mac;
    kodi::Log(ADDON_LOG_INFO, "Changed Setting 'wol_mac' from %s to %s", m_szWolMac.c_str(),
              settingValue.GetString().c_str());
    tmp_sWol_mac = m_szWolMac;
    m_szWolMac = settingValue.GetString();
    if (tmp_sWol_mac != m_szWolMac)
      return ADDON_STATUS_NEED_RESTART;
  }
  else if (settingName == "port")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed Setting 'port' from %u to %u", m_iPort,
              settingValue.GetInt());
    if (m_iPort != settingValue.GetInt())
    {
      m_iPort = settingValue.GetInt();
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  else if (settingName == "priority")
  {
    int newPrio = prioVals[settingValue.GetInt()];
    kodi::Log(ADDON_LOG_INFO, "Changed Setting 'priority' from %u to %u", m_iPriority, newPrio);
    m_iPriority = newPrio;
  }
  else if (settingName == "timeshift")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed Setting 'timeshift' from %u to %u", m_iTimeshift,
              settingValue.GetInt());
    m_iTimeshift = settingValue.GetInt();
  }
  else if (settingName == "convertchar")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed Setting 'convertchar' from %u to %u", m_bCharsetConv,
              settingValue.GetBoolean());
    m_bCharsetConv = settingValue.GetBoolean();
  }
  else if (settingName == "timeout")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed Setting 'timeout' from %u to %u", m_iConnectTimeout,
              settingValue.GetInt());
    m_iConnectTimeout = settingValue.GetInt();
  }
  else if (settingName == "autochannelgroups")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed Setting 'autochannelgroups' from %u to %u",
              m_bAutoChannelGroups, settingValue.GetBoolean());
    if (m_bAutoChannelGroups != settingValue.GetBoolean())
    {
      m_bAutoChannelGroups = settingValue.GetBoolean();
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  else if (settingName == "chunksize")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed Setting 'chunksize' from %u to %u", m_iChunkSize,
              settingValue.GetInt());
    m_iChunkSize = settingValue.GetInt();
  }

  return ADDON_STATUS_OK;
}
