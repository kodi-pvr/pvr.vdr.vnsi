/*
 *  Copyright (C) 2010-2021 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
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
                                   const kodi::addon::CSettingValue& settingValue)
{
  return CVNSISettings::Get().SetSetting(settingName, settingValue);
}

ADDON_STATUS CPVRAddon::CreateInstance(const kodi::addon::IInstanceInfo& instance,
                                       KODI_ADDON_INSTANCE_HDL& hdl)
{
  kodi::Log(ADDON_LOG_DEBUG, "%s: Creating VDR VNSI PVR-Client", __func__);

  if (instance.GetID().empty())
  {
    kodi::Log(ADDON_LOG_ERROR, "%s: Instance creation called without id", __func__);
    return ADDON_STATUS_UNKNOWN;
  }

  if (instance.IsType(ADDON_INSTANCE_PVR))
  {
    CVNSIClientInstance* client = nullptr;
    try
    {
      client = new CVNSIClientInstance(*this, instance);
      if (client->Start(CVNSISettings::Get().GetHostname(), CVNSISettings::Get().GetPort(), nullptr,
                        CVNSISettings::Get().GetWolMac()))
      {
        hdl = client;
        m_usedInstances.emplace(instance.GetID(), client);
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

void CPVRAddon::DestroyInstance(const kodi::addon::IInstanceInfo& instance,
                                const KODI_ADDON_INSTANCE_HDL hdl)
{
  if (instance.IsType(ADDON_INSTANCE_PVR))
  {
    const auto& it = m_usedInstances.find(instance.GetID());
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
