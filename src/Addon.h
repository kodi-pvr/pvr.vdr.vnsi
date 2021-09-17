/*
 *  Copyright (C) 2010-2021 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <kodi/AddonBase.h>
#include <unordered_map>

class CVNSIClientInstance;

class ATTRIBUTE_HIDDEN CPVRAddon : public kodi::addon::CAddonBase
{
public:
  CPVRAddon() = default;

  ADDON_STATUS Create() override;
  ADDON_STATUS SetSetting(const std::string& settingName,
                          const kodi::CSettingValue& settingValue) override;
  ADDON_STATUS CreateInstance(int instanceType,
                              const std::string& instanceID,
                              KODI_HANDLE instance,
                              const std::string& version,
                              KODI_HANDLE& addonInstance) override;
  void DestroyInstance(int instanceType,
                       const std::string& instanceID,
                       KODI_HANDLE addonInstance) override;

private:
  std::unordered_map<std::string, CVNSIClientInstance*> m_usedInstances;
};
