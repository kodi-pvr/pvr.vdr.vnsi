/*
 *  Copyright (C) 2010-2020 Team Kodi
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "VNSIData.h"

class CProvider
{
public:
  CProvider();
  CProvider(std::string name, int caid);
  bool operator==(const CProvider &rhs) const;
  std::string m_name;
  int m_caid;
  bool m_whitelist;
};

class CChannel
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

class CVNSIChannels
{
public:
  CVNSIChannels();
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
  bool m_loaded;
  bool m_radio;

  enum
  {
    NONE,
    PROVIDER,
    CHANNEL
  }m_mode;
};
