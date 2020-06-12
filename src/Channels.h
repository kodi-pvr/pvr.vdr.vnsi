/*
 *  Copyright (C) 2010-2020 Team Kodi
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "ClientInstance.h"

#include <map>
#include <string>
#include <vector>

class ATTRIBUTE_HIDDEN CProvider
{
public:
  CProvider() = default;
  CProvider(std::string name, int caid);

  bool operator==(const CProvider &rhs) const;

  std::string m_name;
  int m_caid = 0;
  bool m_whitelist = false;
};

class ATTRIBUTE_HIDDEN CChannel
{
public:
  void SetCaids(const char *caids);

  unsigned int m_id;
  unsigned int m_number;
  std::string m_name;
  std::string m_provider;
  bool m_radio;
  std::vector<int> m_caids;
  bool m_blacklist;
};

class ATTRIBUTE_HIDDEN CVNSIChannels
{
public:
  CVNSIChannels() = default;

  void CreateProviders();
  void LoadProviderWhitelist();
  void LoadChannelBlacklist();
  void ExtractProviderWhitelist();
  void ExtractChannelBlacklist();
  bool IsWhitelist(const CChannel &channel) const;

  std::vector<CChannel> m_channels;
  std::map<int, int> m_channelsMap;
  std::vector<CProvider> m_providers;
  std::vector<CProvider> m_providerWhitelist;
  std::vector<int> m_channelBlacklist;
  bool m_loaded = false;
  bool m_radio = false;

  enum
  {
    NONE,
    PROVIDER,
    CHANNEL
  } m_mode = NONE;
};
