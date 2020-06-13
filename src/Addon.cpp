/*
 *  Copyright (C) 2010-2020 Team Kodi
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Addon.h"

#include "ClientInstance.h"
#include "Settings.h"

ADDON_STATUS CPVRAddon::Create()
{
  if (!CVNSISettings::Get().Load())
  {
    kodi::Log(ADDON_LOG_ERROR, "%s: Failed to load addon settings", __func__);
    return ADDON_STATUS_UNKNOWN;
  }

  return ADDON_STATUS_OK;
}

ADDON_STATUS CPVRAddon::SetSetting(const std::string& settingName,
                                   const kodi::CSettingValue& settingValue)
{
  return CVNSISettings::Get().SetSetting(settingName, settingValue);
}

ADDON_STATUS CPVRAddon::CreateInstance(int instanceType,
                                       const std::string& instanceID,
                                       KODI_HANDLE instance,
                                       const std::string& version,
                                       KODI_HANDLE& addonInstance)
{
  kodi::Log(ADDON_LOG_DEBUG, "%s: Creating VDR VNSI PVR-Client", __func__);

  if (instanceID.empty())
  {
    kodi::Log(ADDON_LOG_ERROR, "%s: Instance creation called without id", __func__);
    return ADDON_STATUS_UNKNOWN;
  }

  if (instanceType == ADDON_INSTANCE_PVR)
  {
    CVNSIClientInstance* client = nullptr;
    try
    {
      client = new CVNSIClientInstance(*this, instance, version);
      if (client->Start(CVNSISettings::Get().GetHostname(), CVNSISettings::Get().GetPort(), nullptr,
                        CVNSISettings::Get().GetWolMac()))
      {
        addonInstance = client;
        m_usedInstances.emplace(instanceID, client);
        return ADDON_STATUS_OK;
      }
    }
    catch (std::exception e)
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
      delete client;
    }
  }

  return ADDON_STATUS_UNKNOWN;
}

void CPVRAddon::DestroyInstance(int instanceType,
                                const std::string& instanceID,
                                KODI_HANDLE addonInstance)
{
  if (instanceType == ADDON_INSTANCE_PVR)
  {
    const auto& it = m_usedInstances.find(instanceID);
    if (it != m_usedInstances.end())
    {
      m_usedInstances.erase(it);
    }
    else
    {
      kodi::Log(ADDON_LOG_ERROR, "%s: DestroyInstance call with not known instance", __func__);
    }
  }
}

ADDONCREATOR(CPVRAddon)
