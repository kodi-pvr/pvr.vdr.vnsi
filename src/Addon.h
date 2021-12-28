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

class ATTR_DLL_LOCAL CPVRAddon : public kodi::addon::CAddonBase
{
public:
  CPVRAddon() = default;

  ADDON_STATUS Create() override;
  ADDON_STATUS SetSetting(const std::string& settingName,
                          const kodi::addon::CSettingValue& settingValue) override;
  ADDON_STATUS CreateInstance(const kodi::addon::IInstanceInfo& instance,
                              KODI_ADDON_INSTANCE_HDL& hdl) override;
  void DestroyInstance(const kodi::addon::IInstanceInfo& instance,
                       const KODI_ADDON_INSTANCE_HDL hdl) override;

private:
  std::unordered_map<std::string, CVNSIClientInstance*> m_usedInstances;
};
