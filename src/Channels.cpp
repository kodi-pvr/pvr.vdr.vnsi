/*
 *  Copyright (C) 2010-2021 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Channels.h"

#include <algorithm>

CProvider::CProvider(std::string name, int caid) : m_name(name), m_caid(caid), m_whitelist(false)
{
}

bool CProvider::operator==(const CProvider& rhs) const
{
  if (rhs.m_caid != m_caid)
    return false;
  if (rhs.m_name.compare(m_name) != 0)
    return false;
  return true;
}

void CChannel::SetCaids(const char* caids)
{
  m_caids.clear();
  std::string strCaids = caids;
  size_t pos = strCaids.find("caids:");
  if (pos == strCaids.npos)
    return;

  strCaids.erase(0, 6);
  std::string token;
  int caid;
  char* pend;
  while ((pos = strCaids.find(";")) != strCaids.npos)
  {
    token = strCaids.substr(0, pos);
    caid = strtol(token.c_str(), &pend, 10);
    m_caids.push_back(caid);
    strCaids.erase(0, pos + 1);
  }
  if (strCaids.length() > 1)
  {
    caid = strtol(strCaids.c_str(), &pend, 10);
    m_caids.push_back(caid);
  }
}

void CVNSIChannels::CreateProviders()
{
  CProvider provider;
  m_providers.clear();

  for (const auto& channel : m_channels)
  {
    provider.m_name = channel.m_provider;
    for (auto caid : channel.m_caids)
    {
      provider.m_caid = caid;
      auto p_it = std::find(m_providers.begin(), m_providers.end(), provider);
      if (p_it == m_providers.end())
      {
        m_providers.push_back(provider);
      }
    }
    if (channel.m_caids.empty())
    {
      provider.m_caid = 0;
      auto p_it = std::find(m_providers.begin(), m_providers.end(), provider);
      if (p_it == m_providers.end())
      {
        m_providers.push_back(provider);
      }
    }
  }
}

void CVNSIChannels::LoadProviderWhitelist()
{
  bool select = m_providerWhitelist.empty();
  for (auto& provider : m_providers)
  {
    provider.m_whitelist = select;
  }

  for (auto& w : m_providerWhitelist)
  {
    auto p_it = std::find(m_providers.begin(), m_providers.end(), w);
    if (p_it != m_providers.end())
    {
      p_it->m_whitelist = true;
    }
  }
}

void CVNSIChannels::LoadChannelBlacklist()
{
  for (auto b : m_channelBlacklist)
  {
    auto it = m_channelsMap.find(b);
    if (it != m_channelsMap.end())
    {
      int idx = it->second;
      m_channels[idx].m_blacklist = true;
    }
  }
}

void CVNSIChannels::ExtractProviderWhitelist()
{
  m_providerWhitelist.clear();
  for (const auto& provider : m_providers)
  {
    if (provider.m_whitelist)
      m_providerWhitelist.push_back(provider);
  }
  if (m_providerWhitelist.size() == m_providers.size())
  {
    m_providerWhitelist.clear();
  }
  else if (m_providerWhitelist.empty())
  {
    m_providerWhitelist.clear();
    CProvider provider;
    provider.m_name = "no whitelist";
    provider.m_caid = 0;
    m_providerWhitelist.push_back(provider);
  }
}

void CVNSIChannels::ExtractChannelBlacklist()
{
  m_channelBlacklist.clear();
  for (const auto& channel : m_channels)
  {
    if (channel.m_blacklist)
      m_channelBlacklist.push_back(channel.m_id);
  }
}

bool CVNSIChannels::IsWhitelist(const CChannel& channel) const
{
  CProvider provider;
  provider.m_name = channel.m_provider;
  if (channel.m_caids.empty())
  {
    provider.m_caid = 0;
    auto p_it = std::find(m_providers.begin(), m_providers.end(), provider);
    if (p_it != m_providers.end() && p_it->m_whitelist)
      return true;
  }
  for (auto caid : channel.m_caids)
  {
    provider.m_caid = caid;
    auto p_it = std::find(m_providers.begin(), m_providers.end(), provider);
    if (p_it != m_providers.end() && p_it->m_whitelist)
      return true;
  }
  return false;
}
