/*
 *  Copyright (C) 2010-2020 Team Kodi
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "ClientInstance.h"

#include "GUIWindowAdmin.h"
#include "GUIWindowChannelScan.h"
#include "InputstreamDemux.h"
#include "InputstreamRecording.h"
#include "RequestPacket.h"
#include "ResponsePacket.h"
#include "Settings.h"
#include "vnsicommand.h"

#include <algorithm>
#include <kodi/General.h>
#include <kodi/Network.h>
#include <p8-platform/util/StringUtils.h>
#include <string.h>
#include <time.h>

// helper functions (taken from VDR)

namespace
{

time_t IncDay(time_t t, int days)
{
  struct tm tm = *localtime(&t);
  tm.tm_mday += days;
  int h = tm.tm_hour;
  tm.tm_isdst = -1;
  t = mktime(&tm);
  tm.tm_hour = h;
  return mktime(&tm);
}

int GetWDay(time_t t)
{
  int weekday = localtime(&t)->tm_wday;
  return weekday == 0 ? 6 : weekday - 1; // we start with Monday==0!
}

bool DayMatches(time_t t, unsigned int weekdays)
{
  return (weekdays & (1 << GetWDay(t))) != 0;
}

time_t SetTime(time_t t, int secondsFromMidnight)
{
  struct tm tm = *localtime(&t);
  tm.tm_hour = secondsFromMidnight / 3600;
  tm.tm_min = (secondsFromMidnight % 3600) / 60;
  tm.tm_sec = secondsFromMidnight % 60;
  tm.tm_isdst = -1; // makes sure mktime() will determine the correct DST setting
  return mktime(&tm);
}

} /* namespace */

CVNSIClientInstance::SMessage& CVNSIClientInstance::Queue::Enqueue(uint32_t serial)
{
  const P8PLATFORM::CLockObject lock(m_mutex);
  return m_queue[serial];
}

std::unique_ptr<cResponsePacket> CVNSIClientInstance::Queue::Dequeue(uint32_t serial,
                                                                     SMessage& message)
{
  const P8PLATFORM::CLockObject lock(m_mutex);
  auto vresp = std::move(message.pkt);
  m_queue.erase(serial);
  return vresp;
}

void CVNSIClientInstance::Queue::Set(std::unique_ptr<cResponsePacket>&& vresp)
{
  P8PLATFORM::CLockObject lock(m_mutex);
  SMessages::iterator it = m_queue.find(vresp->getRequestID());
  if (it != m_queue.end())
  {
    it->second.pkt = std::move(vresp);
    it->second.event.Broadcast();
  }
}

CVNSIClientInstance::CVNSIClientInstance(const CPVRAddon& base,
                                         KODI_HANDLE instance,
                                         const std::string& kodiVersion)
  : kodi::addon::CInstancePVRClient(instance, kodiVersion),
    cVNSISession(*dynamic_cast<kodi::addon::CInstancePVRClient*>(this)),
    m_base(base)
{
}

CVNSIClientInstance::~CVNSIClientInstance()
{
  m_abort = true;
  StopThread(0);
  Close();
}

void CVNSIClientInstance::OnDisconnect()
{
  kodi::addon::CInstancePVRClient::ConnectionStateChange(
      "vnsi connection lost", PVR_CONNECTION_STATE_DISCONNECTED, kodi::GetLocalizedString(30044));
}

void CVNSIClientInstance::OnReconnect()
{
  EnableStatusInterface(true, false);

  kodi::addon::CInstancePVRClient::ConnectionStateChange("vnsi connection established",
                                                         PVR_CONNECTION_STATE_CONNECTED,
                                                         kodi::GetLocalizedString(30045));

  kodi::addon::CInstancePVRClient::TriggerChannelUpdate();
  kodi::addon::CInstancePVRClient::TriggerTimerUpdate();
  kodi::addon::CInstancePVRClient::TriggerRecordingUpdate();
}

bool CVNSIClientInstance::Start(const std::string& hostname,
                                int port,
                                const char* name,
                                const std::string& mac)
{
  m_hostname = hostname;
  m_port = port;
  m_wolMac = mac;

  if (name != nullptr)
    m_name = name;

  kodi::addon::CInstancePVRClient::ConnectionStateChange(
      "VNSI started", PVR_CONNECTION_STATE_CONNECTING, "VNSI started");

  m_abort = false;
  m_connectionLost = true;
  CreateThread();

  kodi::addon::PVRMenuhook hook;
  hook.SetHookId(1);
  hook.SetCategory(PVR_MENUHOOK_SETTING);
  hook.SetLocalizedStringId(30107);
  kodi::addon::CInstancePVRClient::AddMenuHook(hook);

  return true;
}

bool CVNSIClientInstance::SupportChannelScan()
{
  cRequestPacket vrp;
  vrp.init(VNSI_SCAN_SUPPORTED);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
    return false;
  }

  uint32_t ret = vresp->extract_U32();
  return ret == VNSI_RET_OK ? true : false;
}

bool CVNSIClientInstance::SupportRecordingsUndelete()
{
  if (GetProtocol() > 7)
  {
    cRequestPacket vrp;
    vrp.init(VNSI_RECORDINGS_DELETED_ACCESS_SUPPORTED);

    auto vresp = ReadResult(&vrp);
    if (!vresp)
    {
      kodi::Log(ADDON_LOG_INFO, "%s - Can't get response packed", __func__);
      return false;
    }

    uint32_t ret = vresp->extract_U32();
    return ret == VNSI_RET_OK ? true : false;
  }

  kodi::Log(ADDON_LOG_INFO, "%s - Undelete not supported on backend (min. Ver. 1.3.0; Protocol 7)",
            __func__);
  return false;
}

PVR_ERROR CVNSIClientInstance::GetCapabilities(kodi::addon::PVRCapabilities& capabilities)
{
  capabilities.SetSupportsEPG(true);
  capabilities.SetSupportsRecordings(true);
  capabilities.SetSupportsRecordingEdl(true);
  capabilities.SetSupportsTimers(true);
  capabilities.SetSupportsTV(true);
  capabilities.SetSupportsRadio(true);
  capabilities.SetSupportsChannelGroups(true);
  capabilities.SetHandlesInputStream(true);
  capabilities.SetHandlesDemuxing(true);
  if (SupportChannelScan())
    capabilities.SetSupportsChannelScan(true);
  if (SupportRecordingsUndelete())
    capabilities.SetSupportsRecordingsUndelete(true);
  capabilities.SetSupportsRecordingsRename(true);
  capabilities.SetSupportsRecordingsLifetimeChange(false);
  capabilities.SetSupportsDescrambleInfo(false);

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CVNSIClientInstance::GetBackendName(std::string& name)
{
  name = GetServerName();
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CVNSIClientInstance::GetBackendVersion(std::string& version)
{
  version = GetVersion() + "(Protocol: " + std::to_string(GetProtocol()) + ")";
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CVNSIClientInstance::GetBackendHostname(std::string& hostname)
{
  hostname = m_hostname;
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CVNSIClientInstance::GetConnectionString(std::string& connection)
{
  connection = m_hostname + ":" + std::to_string(m_port);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CVNSIClientInstance::GetDriveSpace(uint64_t& total, uint64_t& used)
{
  cRequestPacket vrp;
  vrp.init(VNSI_RECORDINGS_DISKSIZE);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
    return PVR_ERROR_SERVER_ERROR;
  }

  uint32_t totalspace = vresp->extract_U32();
  uint32_t freespace = vresp->extract_U32();
  /* vresp->extract_U32()); percent not used */

  total = totalspace;
  used = (totalspace - freespace);

  /* Convert from kBytes to Bytes */
  total *= 1024;
  used *= 1024;

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CVNSIClientInstance::CallSettingsMenuHook(const kodi::addon::PVRMenuhook& menuhook)
{
  try
  {
    if (menuhook.GetHookId() == 1)
    {
      cVNSIAdmin osd(*this);
      osd.Open(m_hostname, m_port, m_wolMac);
    }
    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

PVR_ERROR CVNSIClientInstance::OpenDialogChannelScan()
{
  try
  {
    cVNSIChannelScan scanner(*this);
    scanner.Open(m_hostname, m_port, m_wolMac);
    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

PVR_ERROR CVNSIClientInstance::OnSystemSleep()
{
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CVNSIClientInstance::OnSystemWake()
{
  if (!m_wolMac.empty())
  {
    kodi::network::WakeOnLan(m_wolMac);
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CVNSIClientInstance::OnPowerSavingActivated()
{
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CVNSIClientInstance::OnPowerSavingDeactivated()
{
  return PVR_ERROR_NO_ERROR;
}

/*******************************************/
/** PVR Channel Functions                 **/

PVR_ERROR CVNSIClientInstance::GetChannelsAmount(int& amount)
{
  try
  {
    cRequestPacket vrp;
    vrp.init(VNSI_CHANNELS_GETCOUNT);

    auto vresp = ReadResult(&vrp);
    if (!vresp)
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
      return PVR_ERROR_SERVER_ERROR;
    }

    amount = vresp->extract_U32();
    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

PVR_ERROR CVNSIClientInstance::GetChannels(bool radio, kodi::addon::PVRChannelsResultSet& results)
{
  try
  {
    cRequestPacket vrp;
    vrp.init(VNSI_CHANNELS_GETCHANNELS);
    vrp.add_U32(radio);
    vrp.add_U8(1); // apply filter

    auto vresp = ReadResult(&vrp);
    if (!vresp)
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
      return PVR_ERROR_SERVER_ERROR;
    }

    while (vresp->getRemainingLength() >= 3 * 4 + 3)
    {
      kodi::addon::PVRChannel tag;

      tag.SetChannelNumber(vresp->extract_U32());
      tag.SetChannelName(vresp->extract_String());
      char* strProviderName = vresp->extract_String();
      tag.SetUniqueId(vresp->extract_U32());
      tag.SetEncryptionSystem(vresp->extract_U32());
      char* strCaids = vresp->extract_String();
      if (m_protocol >= 6)
      {
        std::string path = CVNSISettings::Get().GetIconPath();
        std::string ref = vresp->extract_String();
        if (!path.empty())
        {
          if (path[path.length() - 1] != '/')
            path += '/';
          path += ref;
          path += ".png";
          tag.SetIconPath(path);
        }
      }
      tag.SetIsRadio(radio);

      results.Add(tag);
    }

    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

PVR_ERROR CVNSIClientInstance::GetSignalStatus(int channelUid,
                                               kodi::addon::PVRSignalStatus& signalStatus)
{
  if (!m_demuxer)
    return PVR_ERROR_SERVER_ERROR;

  try
  {
    return (m_demuxer->GetSignalStatus(signalStatus) ? PVR_ERROR_NO_ERROR : PVR_ERROR_SERVER_ERROR);
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

/*******************************************/
/** PVR Channelgroups Functions           **/

PVR_ERROR CVNSIClientInstance::GetChannelGroupsAmount(int& amount)
{
  try
  {
    cRequestPacket vrp;
    vrp.init(VNSI_CHANNELGROUP_GETCOUNT);
    vrp.add_U32(CVNSISettings::Get().GetAutoChannelGroups());

    auto vresp = ReadResult(&vrp);
    if (vresp == nullptr || vresp->noResponse())
    {
      return PVR_ERROR_SERVER_ERROR;
    }

    amount = vresp->extract_U32();

    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

PVR_ERROR CVNSIClientInstance::GetChannelGroups(bool radio,
                                                kodi::addon::PVRChannelGroupsResultSet& results)
{
  try
  {
    int amount = 0;
    GetChannelGroupsAmount(amount);

    if (amount > 0)
    {
      cRequestPacket vrp;
      vrp.init(VNSI_CHANNELGROUP_LIST);
      vrp.add_U8(radio);

      auto vresp = ReadResult(&vrp);
      if (vresp == nullptr || vresp->noResponse())
      {
        return PVR_ERROR_SERVER_ERROR;
      }

      while (vresp->getRemainingLength() >= 1 + 1)
      {
        kodi::addon::PVRChannelGroup tag;

        tag.SetGroupName(vresp->extract_String());
        tag.SetIsRadio(vresp->extract_U8() != 0 ? true : false);
        tag.SetPosition(0);

        results.Add(tag);
      }
    }

    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

PVR_ERROR CVNSIClientInstance::GetChannelGroupMembers(
    const kodi::addon::PVRChannelGroup& group,
    kodi::addon::PVRChannelGroupMembersResultSet& results)
{
  try
  {
    cRequestPacket vrp;
    vrp.init(VNSI_CHANNELGROUP_MEMBERS);
    vrp.add_String(group.GetGroupName().c_str());
    vrp.add_U8(group.GetIsRadio());
    vrp.add_U8(1); // filter channels

    auto vresp = ReadResult(&vrp);
    if (vresp == nullptr || vresp->noResponse())
    {
      return PVR_ERROR_SERVER_ERROR;
    }

    while (vresp->getRemainingLength() >= 2 * 4)
    {
      kodi::addon::PVRChannelGroupMember tag;

      tag.SetGroupName(group.GetGroupName());
      tag.SetChannelUniqueId(vresp->extract_U32());
      tag.SetChannelNumber(vresp->extract_U32());

      results.Add(tag);
    }

    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

/*******************************************/
/** PVR EPG Functions                     **/

PVR_ERROR CVNSIClientInstance::GetEPGForChannel(int channelUid,
                                                time_t start,
                                                time_t end,
                                                kodi::addon::PVREPGTagsResultSet& results)
{
  try
  {
    cRequestPacket vrp;
    vrp.init(VNSI_EPG_GETFORCHANNEL);
    vrp.add_U32(channelUid);
    vrp.add_U32(start);
    vrp.add_U32(end - start);

    auto vresp = ReadResult(&vrp);
    if (!vresp)
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
      return PVR_ERROR_SERVER_ERROR;
    }

    while (vresp->getRemainingLength() >= 5 * 4 + 3)
    {
      kodi::addon::PVREPGTag tag;

      tag.SetUniqueChannelId(channelUid);
      tag.SetUniqueBroadcastId(vresp->extract_U32());
      tag.SetStartTime(vresp->extract_U32());
      tag.SetEndTime(tag.GetStartTime() + vresp->extract_U32());
      uint32_t content(vresp->extract_U32());
      tag.SetGenreType(content & 0xF0);
      tag.SetGenreSubType(content & 0x0F);
      tag.SetGenreDescription("");
      tag.SetParentalRating(vresp->extract_U32());
      tag.SetTitle(vresp->extract_String());
      tag.SetPlotOutline(vresp->extract_String());
      tag.SetPlot(vresp->extract_String());
      tag.SetOriginalTitle("");
      tag.SetCast("");
      tag.SetDirector("");
      tag.SetWriter("");
      tag.SetYear(0);
      tag.SetIMDBNumber("");
      if (!tag.GetPlotOutline().empty())
        tag.SetEpisodeName(tag.GetPlotOutline());
      tag.SetFlags(EPG_TAG_FLAG_UNDEFINED);
      tag.SetSeriesNumber(EPG_TAG_INVALID_SERIES_EPISODE);
      tag.SetEpisodeNumber(EPG_TAG_INVALID_SERIES_EPISODE);
      tag.SetEpisodePartNumber(EPG_TAG_INVALID_SERIES_EPISODE);

      results.Add(tag);
    }

    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

/*******************************************/
/** PVR Recording Functions               **/

PVR_ERROR CVNSIClientInstance::GetRecordingsAmount(bool deleted, int& amount)
{
  try
  {
    cRequestPacket vrp;
    vrp.init(!deleted ? VNSI_RECORDINGS_GETCOUNT : VNSI_RECORDINGS_DELETED_GETCOUNT);

    auto vresp = ReadResult(&vrp);
    if (!vresp)
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
      return PVR_ERROR_SERVER_ERROR;
    }

    amount = vresp->extract_U32();
    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

PVR_ERROR CVNSIClientInstance::GetRecordings(bool deleted,
                                             kodi::addon::PVRRecordingsResultSet& results)
{
  try
  {
    if (!deleted)
      return GetAvailableRecordings(results);
    else
      return GetDeletedRecordings(results);

    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

PVR_ERROR CVNSIClientInstance::GetAvailableRecordings(kodi::addon::PVRRecordingsResultSet& results)
{
  cRequestPacket vrp;
  vrp.init(VNSI_RECORDINGS_GETLIST);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
    return PVR_ERROR_UNKNOWN;
  }

  std::string strRecordingId;
  while (vresp->getRemainingLength() >= 5 * 4 + 5)
  {
    kodi::addon::PVRRecording tag;

    tag.SetRecordingTime(vresp->extract_U32());
    tag.SetDuration(vresp->extract_U32());
    tag.SetPriority(vresp->extract_U32());
    tag.SetLifetime(vresp->extract_U32());
    tag.SetIsDeleted(false);
    tag.SetSeriesNumber(PVR_RECORDING_INVALID_SERIES_EPISODE);
    tag.SetEpisodeNumber(PVR_RECORDING_INVALID_SERIES_EPISODE);
    tag.SetChannelName(vresp->extract_String());
    if (GetProtocol() >= 9)
    {
      tag.SetChannelUid(-1);
      uint32_t uuid = vresp->extract_U32();
      if (uuid > 0)
        tag.SetChannelUid(uuid);
      uint8_t type = vresp->extract_U8();
      if (type == 1)
        tag.SetChannelType(PVR_RECORDING_CHANNEL_TYPE_RADIO);
      else if (type == 2)
        tag.SetChannelType(PVR_RECORDING_CHANNEL_TYPE_TV);
      else
        tag.SetChannelType(PVR_RECORDING_CHANNEL_TYPE_UNKNOWN);
    }
    else
    {
      tag.SetChannelUid(PVR_CHANNEL_INVALID_UID);
      tag.SetChannelType(PVR_RECORDING_CHANNEL_TYPE_UNKNOWN);
    }

    tag.SetTitle(vresp->extract_String());
    tag.SetEpisodeName(vresp->extract_String());
    tag.SetPlotOutline(tag.GetEpisodeName());
    tag.SetPlot(vresp->extract_String());
    tag.SetDirectory(vresp->extract_String());
    tag.SetRecordingId(StringUtils::Format("%i", vresp->extract_U32()));

    results.Add(tag);
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CVNSIClientInstance::GetDeletedRecordings(kodi::addon::PVRRecordingsResultSet& results)
{
  cRequestPacket vrp;
  vrp.init(VNSI_RECORDINGS_DELETED_GETLIST);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
    return PVR_ERROR_UNKNOWN;
  }

  std::string strRecordingId;
  while (vresp->getRemainingLength() >= 5 * 4 + 5)
  {
    kodi::addon::PVRRecording tag;

    tag.SetRecordingTime(vresp->extract_U32());
    tag.SetDuration(vresp->extract_U32());
    tag.SetPriority(vresp->extract_U32());
    tag.SetLifetime(vresp->extract_U32());
    tag.SetIsDeleted(true);
    tag.SetChannelName(vresp->extract_String());
    if (GetProtocol() >= 9)
    {
      tag.SetChannelUid(vresp->extract_S32());
    }
    else
      tag.SetChannelUid(PVR_CHANNEL_INVALID_UID);

    tag.SetTitle(vresp->extract_String());
    tag.SetEpisodeName(vresp->extract_String());
    tag.SetPlotOutline(tag.GetEpisodeName());
    tag.SetPlot(vresp->extract_String());
    tag.SetDirectory(vresp->extract_String());
    tag.SetRecordingId(std::to_string(vresp->extract_U32()));

    results.Add(tag);
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CVNSIClientInstance::RenameRecording(const kodi::addon::PVRRecording& recording)
{
  try
  {
    cRequestPacket vrp;
    vrp.init(VNSI_RECORDINGS_RENAME);

    // add uid
    kodi::Log(ADDON_LOG_DEBUG, "%s - uid: %s", __func__, recording.GetRecordingId().c_str());
    vrp.add_U32(std::stoi(recording.GetRecordingId()));

    // add new title
    vrp.add_String(recording.GetTitle().c_str());

    auto vresp = ReadResult(&vrp);
    if (vresp == nullptr || vresp->noResponse())
    {
      return PVR_ERROR_SERVER_ERROR;
    }

    uint32_t returnCode = vresp->extract_U32();

    if (returnCode != 0)
      return PVR_ERROR_FAILED;

    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

PVR_ERROR CVNSIClientInstance::DeleteRecording(const kodi::addon::PVRRecording& recording)
{
  try
  {
    cRequestPacket vrp;
    vrp.init(recording.GetIsDeleted() ? VNSI_RECORDINGS_DELETED_DELETE : VNSI_RECORDINGS_DELETE);
    vrp.add_U32(std::stoi(recording.GetRecordingId()));

    auto vresp = ReadResult(&vrp);
    if (vresp == nullptr || vresp->noResponse())
    {
      return PVR_ERROR_UNKNOWN;
    }

    uint32_t returnCode = vresp->extract_U32();

    switch (returnCode)
    {
      case VNSI_RET_DATALOCKED:
        return PVR_ERROR_FAILED;

      case VNSI_RET_RECRUNNING:
        return PVR_ERROR_RECORDING_RUNNING;

      case VNSI_RET_DATAINVALID:
        return PVR_ERROR_INVALID_PARAMETERS;

      case VNSI_RET_ERROR:
        return PVR_ERROR_SERVER_ERROR;
    }

    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

PVR_ERROR CVNSIClientInstance::UndeleteRecording(const kodi::addon::PVRRecording& recording)
{
  try
  {
    cRequestPacket vrp;
    vrp.init(VNSI_RECORDINGS_DELETED_UNDELETE);
    vrp.add_U32(std::stoi(recording.GetRecordingId()));

    auto vresp = ReadResult(&vrp);
    if (vresp == nullptr || vresp->noResponse())
    {
      return PVR_ERROR_UNKNOWN;
    }

    uint32_t returnCode = vresp->extract_U32();
    switch (returnCode)
    {
      case VNSI_RET_DATALOCKED:
        return PVR_ERROR_FAILED;

      case VNSI_RET_RECRUNNING:
        return PVR_ERROR_RECORDING_RUNNING;

      case VNSI_RET_DATAINVALID:
        return PVR_ERROR_INVALID_PARAMETERS;

      case VNSI_RET_ERROR:
        return PVR_ERROR_SERVER_ERROR;
    }

    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

PVR_ERROR CVNSIClientInstance::DeleteAllRecordingsFromTrash()
{
  try
  {
    cRequestPacket vrp;
    vrp.init(VNSI_RECORDINGS_DELETED_DELETE_ALL);

    auto vresp = ReadResult(&vrp);
    if (vresp == nullptr || vresp->noResponse())
    {
      return PVR_ERROR_UNKNOWN;
    }

    uint32_t returnCode = vresp->extract_U32();
    switch (returnCode)
    {
      case VNSI_RET_DATALOCKED:
        return PVR_ERROR_FAILED;

      case VNSI_RET_RECRUNNING:
        return PVR_ERROR_RECORDING_RUNNING;

      case VNSI_RET_DATAINVALID:
        return PVR_ERROR_INVALID_PARAMETERS;

      case VNSI_RET_ERROR:
        return PVR_ERROR_SERVER_ERROR;
    }

    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

PVR_ERROR CVNSIClientInstance::GetRecordingEdl(const kodi::addon::PVRRecording& recording,
                                               std::vector<kodi::addon::PVREDLEntry>& edl)
{
  try
  {
    cRequestPacket vrp;
    vrp.init(VNSI_RECORDINGS_GETEDL);
    vrp.add_U32(std::stoi(recording.GetRecordingId()));

    int size = 0;
    auto vresp = ReadResult(&vrp);

    if (vresp == nullptr)
    {
      return PVR_ERROR_UNKNOWN;
    }
    else if (vresp->noResponse())
    {
      return PVR_ERROR_NO_ERROR;
    }

    while (vresp->getRemainingLength() >= 2 * 8 + 4 && size++ < PVR_ADDON_EDL_LENGTH)
    {
      kodi::addon::PVREDLEntry entry;

      entry.SetStart(vresp->extract_S64());
      entry.SetEnd(vresp->extract_S64());
      entry.SetType(static_cast<PVR_EDL_TYPE>(vresp->extract_S32()));

      edl.push_back(std::move(entry));
    }

    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}


/*******************************************/
/** PVR Timer Functions                   **/

PVR_ERROR CVNSIClientInstance::GetTimerTypes(std::vector<kodi::addon::PVRTimerType>& types)
{
  try
  {
    {
      // One-shot manual
      kodi::addon::PVRTimerType type;

      type.SetId(VNSI_TIMER_TYPE_MAN);
      type.SetDescription(kodi::GetLocalizedString(30200));
      type.SetAttributes(PVR_TIMER_TYPE_IS_MANUAL | PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE |
                         PVR_TIMER_TYPE_SUPPORTS_CHANNELS | PVR_TIMER_TYPE_SUPPORTS_START_TIME |
                         PVR_TIMER_TYPE_SUPPORTS_END_TIME | PVR_TIMER_TYPE_SUPPORTS_PRIORITY |
                         PVR_TIMER_TYPE_SUPPORTS_LIFETIME |
                         PVR_TIMER_TYPE_SUPPORTS_RECORDING_FOLDERS);

      types.push_back(std::move(type));
    }

    {
      // Repeating manual
      kodi::addon::PVRTimerType type;

      type.SetId(VNSI_TIMER_TYPE_MAN_REPEAT);
      type.SetDescription(kodi::GetLocalizedString(30201));
      type.SetAttributes(PVR_TIMER_TYPE_IS_MANUAL | PVR_TIMER_TYPE_IS_REPEATING |
                         PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE | PVR_TIMER_TYPE_SUPPORTS_CHANNELS |
                         PVR_TIMER_TYPE_SUPPORTS_START_TIME | PVR_TIMER_TYPE_SUPPORTS_END_TIME |
                         PVR_TIMER_TYPE_SUPPORTS_PRIORITY | PVR_TIMER_TYPE_SUPPORTS_LIFETIME |
                         PVR_TIMER_TYPE_SUPPORTS_FIRST_DAY | PVR_TIMER_TYPE_SUPPORTS_WEEKDAYS |
                         PVR_TIMER_TYPE_SUPPORTS_RECORDING_FOLDERS);

      types.push_back(std::move(type));
    }

    {
      // Repeating manual
      kodi::addon::PVRTimerType type;

      type.SetId(VNSI_TIMER_TYPE_MAN_REPEAT_CHILD);
      type.SetDescription(kodi::GetLocalizedString(30205));
      type.SetAttributes(PVR_TIMER_TYPE_IS_MANUAL | PVR_TIMER_TYPE_IS_READONLY |
                         PVR_TIMER_TYPE_SUPPORTS_CHANNELS | PVR_TIMER_TYPE_SUPPORTS_START_TIME |
                         PVR_TIMER_TYPE_SUPPORTS_END_TIME | PVR_TIMER_TYPE_SUPPORTS_PRIORITY |
                         PVR_TIMER_TYPE_SUPPORTS_LIFETIME |
                         PVR_TIMER_TYPE_SUPPORTS_RECORDING_FOLDERS);

      types.push_back(std::move(type));
    }

    {
      // One-shot epg-based
      kodi::addon::PVRTimerType type;

      type.SetId(VNSI_TIMER_TYPE_EPG);
      type.SetDescription(kodi::GetLocalizedString(30202));
      type.SetAttributes(
          PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE | PVR_TIMER_TYPE_REQUIRES_EPG_TAG_ON_CREATE |
          PVR_TIMER_TYPE_SUPPORTS_CHANNELS | PVR_TIMER_TYPE_SUPPORTS_START_TIME |
          PVR_TIMER_TYPE_SUPPORTS_END_TIME | PVR_TIMER_TYPE_SUPPORTS_PRIORITY |
          PVR_TIMER_TYPE_SUPPORTS_LIFETIME | PVR_TIMER_TYPE_SUPPORTS_RECORDING_FOLDERS);

      types.push_back(std::move(type));
    }

    // addition timer supported by backend
    if (GetProtocol() >= 9)
    {
      cRequestPacket vrp;
      vrp.init(VNSI_TIMER_GETTYPES);
      auto vresp = ReadResult(&vrp);
      if (!vresp)
      {
        kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
        return PVR_ERROR_NO_ERROR;
      }
      uint32_t vnsitimers = vresp->extract_U32();

      if (vnsitimers & VNSI_TIMER_TYPE_EPG_SEARCH)
      {
        // EPG search timer
        kodi::addon::PVRTimerType type;

        type.SetId(VNSI_TIMER_TYPE_EPG_SEARCH);
        type.SetDescription(kodi::GetLocalizedString(30204));
        type.SetAttributes(PVR_TIMER_TYPE_IS_REPEATING | PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE |
                           PVR_TIMER_TYPE_SUPPORTS_CHANNELS |
                           PVR_TIMER_TYPE_SUPPORTS_TITLE_EPG_MATCH |
                           PVR_TIMER_TYPE_SUPPORTS_PRIORITY | PVR_TIMER_TYPE_SUPPORTS_LIFETIME);
        types.push_back(std::move(type));
      }

      // VPS Timer
      kodi::addon::PVRTimerType type;

      type.SetId(VNSI_TIMER_TYPE_VPS);
      type.SetDescription(kodi::GetLocalizedString(30203));
      type.SetAttributes(PVR_TIMER_TYPE_IS_MANUAL | PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE |
                         PVR_TIMER_TYPE_SUPPORTS_CHANNELS | PVR_TIMER_TYPE_SUPPORTS_START_TIME |
                         PVR_TIMER_TYPE_SUPPORTS_END_TIME | PVR_TIMER_TYPE_SUPPORTS_PRIORITY |
                         PVR_TIMER_TYPE_SUPPORTS_LIFETIME |
                         PVR_TIMER_TYPE_SUPPORTS_RECORDING_FOLDERS);
      types.push_back(std::move(type));
    }

    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

PVR_ERROR CVNSIClientInstance::GetTimersAmount(int& amount)
{
  try
  {
    cRequestPacket vrp;
    vrp.init(VNSI_TIMER_GETCOUNT);

    auto vresp = ReadResult(&vrp);
    if (!vresp)
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
      return PVR_ERROR_SERVER_ERROR;
    }

    amount = vresp->extract_U32();
    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

PVR_ERROR CVNSIClientInstance::GetTimers(kodi::addon::PVRTimersResultSet& results)
{
  try
  {
    cRequestPacket vrp;
    vrp.init(VNSI_TIMER_GETLIST);

    auto vresp = ReadResult(&vrp);
    if (!vresp)
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
      return PVR_ERROR_SERVER_ERROR;
    }

    uint32_t numTimers = vresp->extract_U32();
    if (numTimers > 0)
    {
      while (vresp->getRemainingLength() >= 12 * 4 + 1)
      {
        kodi::addon::PVRTimer tag;

        if (GetProtocol() >= 9)
        {
          tag.SetTimerType(vresp->extract_U32());
        }

        tag.SetClientIndex(vresp->extract_U32());
        int iActive(vresp->extract_U32());
        int iRecording(vresp->extract_U32());
        int iPending(vresp->extract_U32());
        if (iRecording)
          tag.SetState(PVR_TIMER_STATE_RECORDING);
        else if (iPending || iActive)
          tag.SetState(PVR_TIMER_STATE_SCHEDULED);
        else
          tag.SetState(PVR_TIMER_STATE_DISABLED);
        tag.SetPriority(vresp->extract_U32());
        tag.SetLifetime(vresp->extract_U32());
        vresp->extract_U32(); // channel number - unused
        tag.SetClientChannelUid(vresp->extract_U32());
        tag.SetStartTime(vresp->extract_U32());
        tag.SetEndTime(vresp->extract_U32());
        tag.SetFirstDay(vresp->extract_U32());
        tag.SetWeekdays(vresp->extract_U32());
        tag.SetTitle(vresp->extract_String());
        tag.SetMarginStart(0);
        tag.SetMarginEnd(0);

        if (GetProtocol() >= 9)
        {
          tag.SetEPGSearchString(vresp->extract_String());

          if (tag.GetTimerType() == VNSI_TIMER_TYPE_MAN && tag.GetWeekdays())
            tag.SetTimerType(VNSI_TIMER_TYPE_MAN_REPEAT);
        }

        if (GetProtocol() >= 10)
        {
          tag.SetParentClientIndex(vresp->extract_U32());
        }

        if (tag.GetStartTime() == 0)
          tag.SetStartAnyTime(true);
        if (tag.GetEndTime() == 0)
          tag.SetEndAnyTime(true);

        results.Add(tag);

        if (tag.GetTimerType() == VNSI_TIMER_TYPE_MAN_REPEAT &&
            tag.GetState() != PVR_TIMER_STATE_DISABLED)
        {
          GenTimerChildren(tag, results);
        }
      }
    }
    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

PVR_ERROR CVNSIClientInstance::AddTimer(const kodi::addon::PVRTimer& timer)
{
  try
  {
    cRequestPacket vrp;
    vrp.init(VNSI_TIMER_ADD);

    // add directory in front of the title
    std::string path = GenTimerFolder(timer.GetDirectory(), timer.GetTitle());
    if (path.empty())
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - Empty filename !", __func__);
      return PVR_ERROR_UNKNOWN;
    }

    // use timer margin to calculate start/end times
    uint32_t starttime = timer.GetStartTime() - timer.GetMarginStart() * 60;
    uint32_t endtime = timer.GetEndTime() + timer.GetMarginEnd() * 60;

    if (GetProtocol() >= 9)
    {
      vrp.add_U32(timer.GetTimerType());
    }

    vrp.add_U32(timer.GetState() == PVR_TIMER_STATE_SCHEDULED);
    vrp.add_U32(timer.GetPriority());
    vrp.add_U32(timer.GetLifetime());
    vrp.add_U32(timer.GetClientChannelUid());
    vrp.add_U32(starttime);
    vrp.add_U32(endtime);
    vrp.add_U32(timer.GetWeekdays() != PVR_WEEKDAY_NONE ? timer.GetFirstDay() : 0);
    vrp.add_U32(timer.GetWeekdays());
    vrp.add_String(path.c_str());
    vrp.add_String(timer.GetTitle().c_str());

    if (GetProtocol() >= 9)
    {
      vrp.add_String(timer.GetEPGSearchString().c_str());
    }

    if (GetProtocol() >= 10)
    {
      vrp.add_U32(timer.GetMarginStart() * 60);
      vrp.add_U32(timer.GetMarginEnd() * 60);
    }

    auto vresp = ReadResult(&vrp);
    if (vresp == nullptr || vresp->noResponse())
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
      return PVR_ERROR_UNKNOWN;
    }
    uint32_t returnCode = vresp->extract_U32();
    if (returnCode == VNSI_RET_DATALOCKED)
      return PVR_ERROR_ALREADY_PRESENT;
    else if (returnCode == VNSI_RET_DATAINVALID)
      return PVR_ERROR_INVALID_PARAMETERS;
    else if (returnCode == VNSI_RET_ERROR)
      return PVR_ERROR_SERVER_ERROR;

    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

PVR_ERROR CVNSIClientInstance::DeleteTimer(const kodi::addon::PVRTimer& timer, bool forceDelete)
{
  try
  {
    cRequestPacket vrp;
    vrp.init(VNSI_TIMER_DELETE);
    vrp.add_U32(timer.GetClientIndex());
    vrp.add_U32(forceDelete);

    auto vresp = ReadResult(&vrp);
    if (vresp == nullptr || vresp->noResponse())
    {
      return PVR_ERROR_UNKNOWN;
    }
    uint32_t returnCode = vresp->extract_U32();

    if (returnCode == VNSI_RET_DATALOCKED)
      return PVR_ERROR_FAILED;
    else if (returnCode == VNSI_RET_RECRUNNING)
      return PVR_ERROR_RECORDING_RUNNING;
    else if (returnCode == VNSI_RET_DATAINVALID)
      return PVR_ERROR_INVALID_PARAMETERS;
    else if (returnCode == VNSI_RET_ERROR)
      return PVR_ERROR_SERVER_ERROR;
    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

PVR_ERROR CVNSIClientInstance::UpdateTimer(const kodi::addon::PVRTimer& timer)
{
  try
  {
    // use timer margin to calculate start/end times
    uint32_t starttime = timer.GetStartTime() - timer.GetMarginStart() * 60;
    uint32_t endtime = timer.GetEndTime() + timer.GetMarginEnd() * 60;

    // add directory in front of the title
    std::string path = GenTimerFolder(timer.GetDirectory(), timer.GetTitle());
    if (path.empty())
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - Empty filename !", __func__);
      return PVR_ERROR_UNKNOWN;
    }

    cRequestPacket vrp;
    vrp.init(VNSI_TIMER_UPDATE);

    vrp.add_U32(timer.GetClientIndex());
    if (GetProtocol() >= 9)
    {
      vrp.add_U32(timer.GetTimerType());
    }
    vrp.add_U32(timer.GetState() == PVR_TIMER_STATE_SCHEDULED);
    vrp.add_U32(timer.GetPriority());
    vrp.add_U32(timer.GetLifetime());
    vrp.add_U32(timer.GetClientChannelUid());
    vrp.add_U32(starttime);
    vrp.add_U32(endtime);
    vrp.add_U32(timer.GetWeekdays() != PVR_WEEKDAY_NONE ? timer.GetFirstDay() : 0);
    vrp.add_U32(timer.GetWeekdays());
    vrp.add_String(path.c_str());
    vrp.add_String(timer.GetTitle().c_str());

    if (GetProtocol() >= 9)
    {
      vrp.add_String(timer.GetEPGSearchString().c_str());
    }

    auto vresp = ReadResult(&vrp);
    if (vresp == nullptr || vresp->noResponse())
    {
      return PVR_ERROR_UNKNOWN;
    }
    uint32_t returnCode = vresp->extract_U32();
    if (returnCode == VNSI_RET_DATAUNKNOWN)
      return PVR_ERROR_FAILED;
    else if (returnCode == VNSI_RET_DATAINVALID)
      return PVR_ERROR_INVALID_PARAMETERS;
    else if (returnCode == VNSI_RET_ERROR)
      return PVR_ERROR_SERVER_ERROR;

    return PVR_ERROR_NO_ERROR;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return PVR_ERROR_SERVER_ERROR;
  }
}

bool CVNSIClientInstance::GenTimerChildren(const kodi::addon::PVRTimer& timer,
                                           kodi::addon::PVRTimersResultSet& results)
{
  time_t now = time(nullptr);
  time_t firstDay = timer.GetFirstDay();
  time_t startTime = timer.GetStartTime();
  time_t endTime = timer.GetEndTime();

  struct tm* loctime = localtime(&startTime);
  int startSec = loctime->tm_hour * 3600 + loctime->tm_min * 60;
  loctime = localtime(&endTime);
  int stopSec = loctime->tm_hour * 3600 + loctime->tm_min * 60;
  int length = stopSec - startSec;
  if (length < 0)
    length += 3600 * 24;

  for (int n = 0; n < 2; ++n)
  {
    for (int i = -1; i <= 7; i++)
    {
      time_t t0 = IncDay(firstDay ? std::max(firstDay, now) : now, i);
      if (DayMatches(t0, timer.GetWeekdays()))
      {
        time_t start = SetTime(t0, startSec);
        time_t stop = start + length;
        if ((!firstDay || start >= firstDay) && now < stop)
        {
          kodi::addon::PVRTimer child = timer;

          child.SetClientIndex(timer.GetClientIndex() + n | 0xF000);
          child.SetParentClientIndex(timer.GetClientIndex());
          child.SetTimerType(VNSI_TIMER_TYPE_MAN_REPEAT_CHILD);
          child.SetStartTime(start);
          child.SetEndTime(stop);
          child.SetWeekdays(0);

          results.Add(child);

          firstDay = start + length + 300;
          break;
        }
      }
    }
  }
  return true;
}

std::string CVNSIClientInstance::GenTimerFolder(std::string directory, std::string title)
{
  // add directory in front of the title
  std::string path;
  if (strlen(directory.c_str()) > 0)
  {
    path += directory;
    if (path == "/")
    {
      path.clear();
    }
    else if (path.size() > 1)
    {
      if (path[0] == '/')
      {
        path = path.substr(1);
      }
    }

    if (path.size() > 0 && path[path.size() - 1] != '/')
    {
      path += "/";
    }
  }

  // replace directory separators
  for (std::size_t i = 0; i < path.size(); i++)
  {
    if (path[i] == '/' || path[i] == '\\')
    {
      path[i] = '~';
    }
  }

  if (strlen(title.c_str()) > 0)
  {
    path += title;
  }

  // replace colons
  for (std::size_t i = 0; i < path.size(); i++)
  {
    if (path[i] == ':')
    {
      path[i] = '|';
    }
  }

  return path;
}

PVR_ERROR CVNSIClientInstance::GetTimerInfo(unsigned int timernumber, kodi::addon::PVRTimer& tag)
{
  cRequestPacket vrp;

  vrp.init(VNSI_TIMER_GET);
  vrp.add_U32(timernumber);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
    return PVR_ERROR_UNKNOWN;
  }

  uint32_t returnCode = vresp->extract_U32();
  if (returnCode != VNSI_RET_OK)
  {
    if (returnCode == VNSI_RET_DATAUNKNOWN)
      return PVR_ERROR_FAILED;
    else if (returnCode == VNSI_RET_ERROR)
      return PVR_ERROR_SERVER_ERROR;
  }

  if (GetProtocol() >= 9)
  {
    tag.SetTimerType(vresp->extract_U32());
  }

  tag.SetClientIndex(vresp->extract_U32());
  int iActive(vresp->extract_U32());
  int iRecording(vresp->extract_U32());
  int iPending(vresp->extract_U32());
  if (iRecording)
    tag.SetState(PVR_TIMER_STATE_RECORDING);
  else if (iPending || iActive)
    tag.SetState(PVR_TIMER_STATE_SCHEDULED);
  else
    tag.SetState(PVR_TIMER_STATE_DISABLED);
  tag.SetPriority(vresp->extract_U32());
  tag.SetLifetime(vresp->extract_U32());
  vresp->extract_U32(); // channel number - unused
  tag.SetClientChannelUid(vresp->extract_U32());
  tag.SetStartTime(vresp->extract_U32());
  tag.SetEndTime(vresp->extract_U32());
  tag.SetFirstDay(vresp->extract_U32());
  tag.SetWeekdays(vresp->extract_U32());
  tag.SetTitle(vresp->extract_String());

  if (GetProtocol() >= 9)
  {
    tag.SetEPGSearchString(vresp->extract_String());

    if (tag.GetTimerType() == VNSI_TIMER_TYPE_MAN && tag.GetWeekdays())
      tag.SetTimerType(VNSI_TIMER_TYPE_MAN_REPEAT);
  }

  if (GetProtocol() >= 10)
  {
    tag.SetParentClientIndex(vresp->extract_U32());
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CVNSIClientInstance::RenameTimer(const kodi::addon::PVRTimer& timerinfo,
                                           const std::string& newname)
{
  kodi::addon::PVRTimer timerinfo1;

  PVR_ERROR ret = GetTimerInfo(timerinfo.GetClientIndex(), timerinfo1);
  if (ret != PVR_ERROR_NO_ERROR)
    return ret;

  timerinfo1.SetTitle(newname);
  return UpdateTimer(timerinfo1);
}


/*******************************************/
/** PVR Live Stream Functions             **/

bool CVNSIClientInstance::OpenLiveStream(const kodi::addon::PVRChannel& channel)
{
  CloseLiveStream();

  try
  {
    m_demuxer = new cVNSIDemux(*this);
    m_isRealtime = true;
    if (!m_demuxer->OpenChannel(channel))
    {
      delete m_demuxer;
      m_demuxer = nullptr;
      return false;
    }

    return true;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    delete m_demuxer;
    m_demuxer = nullptr;
    return false;
  }
  return false;
}

void CVNSIClientInstance::CloseLiveStream()
{
  delete m_demuxer;
  m_demuxer = nullptr;
}


PVR_ERROR CVNSIClientInstance::GetStreamProperties(
    std::vector<kodi::addon::PVRStreamProperties>& properties)
{
  if (!m_demuxer)
    return PVR_ERROR_SERVER_ERROR;

  return (m_demuxer->GetStreamProperties(properties) ? PVR_ERROR_NO_ERROR : PVR_ERROR_SERVER_ERROR);
}

DemuxPacket* CVNSIClientInstance::DemuxRead()
{
  if (!m_demuxer)
    return nullptr;

  DemuxPacket* pkt;
  try
  {
    pkt = m_demuxer->Read();
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return nullptr;
  }

  if (pkt)
  {
    const P8PLATFORM::CLockObject lock(m_timeshiftMutex);
    m_isTimeshift = m_demuxer->IsTimeshift();
    if ((m_ptsBufferEnd - pkt->dts) / DVD_TIME_BASE > 10)
      m_isTimeshift = false;
    else
      m_isTimeshift = true;
  }
  return pkt;
}

void CVNSIClientInstance::DemuxAbort()
{
  if (m_demuxer)
    m_demuxer->Abort();
}

bool CVNSIClientInstance::SeekTime(double time, bool backwards, double& startpts)
{
  bool ret = false;
  try
  {
    if (m_demuxer)
      ret = m_demuxer->SeekTime(time, backwards, startpts);
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
  }
  return ret;
}

bool CVNSIClientInstance::CanPauseStream()
{
  bool ret = false;
  if (m_demuxer)
    ret = m_demuxer->IsTimeshift();
  return ret;
}

bool CVNSIClientInstance::CanSeekStream()
{
  bool ret = false;
  if (m_demuxer)
    ret = m_demuxer->IsTimeshift();
  return ret;
}

bool CVNSIClientInstance::IsRealTimeStream()
{
  if (m_demuxer)
  {
    const P8PLATFORM::CLockObject lock(m_timeshiftMutex);
    if (!m_isTimeshift)
      return true;
    if (m_isRealtime)
      return true;
  }
  return false;
}

PVR_ERROR CVNSIClientInstance::GetStreamTimes(kodi::addon::PVRStreamTimes& times)
{
  if (m_demuxer && m_demuxer->GetStreamTimes(times))
  {
    m_ptsBufferEnd = times.GetPTSEnd();
    return PVR_ERROR_NO_ERROR;
  }
  else if (m_recording && m_recording->GetStreamTimes(times))
  {
    m_ptsBufferEnd = times.GetPTSEnd();
    return PVR_ERROR_NO_ERROR;
  }
  else
    return PVR_ERROR_SERVER_ERROR;
}

/*******************************************/
/** PVR Recording Stream Functions        **/

bool CVNSIClientInstance::OpenRecordedStream(const kodi::addon::PVRRecording& recording)
{
  CloseRecordedStream();

  m_recording = new cVNSIRecording(*this);
  try
  {
    if (!m_recording->OpenRecording(recording))
    {
      delete m_recording;
      m_recording = nullptr;
      return false;
    }

    return true;
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    delete m_recording;
    m_recording = nullptr;
    return false;
  }
}

void CVNSIClientInstance::CloseRecordedStream()
{
  delete m_recording;
  m_recording = nullptr;
}

int CVNSIClientInstance::ReadRecordedStream(unsigned char* buffer, unsigned int size)
{
  if (!m_recording)
    return -1;

  try
  {
    return m_recording->Read(buffer, size);
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    return -1;
  }
}

int64_t CVNSIClientInstance::SeekRecordedStream(int64_t position, int whence)
{
  try
  {
    if (m_recording)
      return m_recording->Seek(position, whence);
  }
  catch (std::exception e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
  }

  return -1;
}

int64_t CVNSIClientInstance::LengthRecordedStream()
{
  if (m_recording)
    return m_recording->Length();
  return 0;
}

PVR_ERROR CVNSIClientInstance::GetStreamReadChunkSize(int& chunksize)
{
  chunksize = CVNSISettings::Get().GetChunkSize();
  return PVR_ERROR_NO_ERROR;
}

//--==----==----==----==----==----==----==----==----==----==----==----==----==

bool CVNSIClientInstance::EnableStatusInterface(bool onOff, bool wait)
{
  cRequestPacket vrp;
  vrp.init(VNSI_ENABLESTATUSINTERFACE);
  vrp.add_U8(onOff);

  if (!wait)
  {
    cVNSISession::TransmitMessage(&vrp);
    return true;
  }

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
    return false;
  }

  uint32_t ret = vresp->extract_U32();
  return ret == VNSI_RET_OK ? true : false;
}

bool CVNSIClientInstance::OnResponsePacket(cResponsePacket* pkt)
{
  return false;
}

std::unique_ptr<cResponsePacket> CVNSIClientInstance::ReadResult(cRequestPacket* vrp)
{
  SMessage& message = m_queue.Enqueue(vrp->getSerial());

  if (cVNSISession::TransmitMessage(vrp) &&
      !message.event.Wait(CVNSISettings::Get().GetConnectTimeout() * 1000))
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - request timed out after %d seconds", __func__,
              CVNSISettings::Get().GetConnectTimeout());
  }

  return m_queue.Dequeue(vrp->getSerial(), message);
}

void* CVNSIClientInstance::Process()
{
  std::unique_ptr<cResponsePacket> vresp;

  while (!IsStopped())
  {
    // try to reconnect
    if (m_connectionLost)
    {
      // First wake up the VDR server in case a MAC-Address is specified
      if (!m_wolMac.empty())
      {
        if (!kodi::network::WakeOnLan(m_wolMac))
        {
          kodi::Log(ADDON_LOG_ERROR, "Error waking up VNSI Server at MAC-Address %s",
                    m_wolMac.c_str());
        }
      }

      cVNSISession::eCONNECTIONSTATE state = TryReconnect();
      if (state != cVNSISession::CONN_ESABLISHED)
      {
        if (state == cVNSISession::CONN_HOST_NOT_REACHABLE)
        {
          kodi::addon::CInstancePVRClient::ConnectionStateChange(
              "vnsi server not reacheable", PVR_CONNECTION_STATE_SERVER_UNREACHABLE, "");
        }

        Sleep(1000);
        continue;
      }
    }

    // if there's anything in the buffer, read it
    if ((vresp = cVNSISession::ReadMessage(5, 10000)) == nullptr)
    {
      Sleep(5);
      continue;
    }

    // CHANNEL_REQUEST_RESPONSE
    if (vresp->getChannelID() == VNSI_CHANNEL_REQUEST_RESPONSE)
    {
      m_queue.Set(std::move(vresp));
    }

    // CHANNEL_STATUS

    else if (vresp->getChannelID() == VNSI_CHANNEL_STATUS)
    {
      if (vresp->getRequestID() == VNSI_STATUS_MESSAGE)
      {
        uint32_t type = vresp->extract_U32();
        char* msgstr = vresp->extract_String();

        std::string strMessageTranslated;
        if (CVNSISettings::Get().GetCharsetConv())
          kodi::UnknownToUTF8(msgstr, strMessageTranslated);
        else
          strMessageTranslated = msgstr;

        if (type == 2)
          kodi::QueueNotification(QUEUE_ERROR, "", strMessageTranslated);
        if (type == 1)
          kodi::QueueNotification(QUEUE_WARNING, "", strMessageTranslated);
        else
          kodi::QueueNotification(QUEUE_INFO, "", strMessageTranslated);
      }
      else if (vresp->getRequestID() == VNSI_STATUS_RECORDING)
      {
        vresp->extract_U32(); // device currently unused
        vresp->extract_U32(); // on (not used)
        char* str1 = vresp->extract_String();
        char* str2 = vresp->extract_String();

        //        kodi::addon::CInstancePVRClient::Recording(str1, str2, on!=0?true:false);
        kodi::addon::CInstancePVRClient::TriggerTimerUpdate();
      }
      else if (vresp->getRequestID() == VNSI_STATUS_TIMERCHANGE)
      {
        kodi::Log(ADDON_LOG_DEBUG, "Server requested timer update");
        kodi::addon::CInstancePVRClient::TriggerTimerUpdate();
      }
      else if (vresp->getRequestID() == VNSI_STATUS_CHANNELCHANGE)
      {
        kodi::Log(ADDON_LOG_DEBUG, "Server requested channel update");
        kodi::addon::CInstancePVRClient::TriggerChannelUpdate();
      }
      else if (vresp->getRequestID() == VNSI_STATUS_RECORDINGSCHANGE)
      {
        kodi::Log(ADDON_LOG_DEBUG, "Server requested recordings update");
        kodi::addon::CInstancePVRClient::TriggerRecordingUpdate();
      }
      else if (vresp->getRequestID() == VNSI_STATUS_EPGCHANGE)
      {
        uint32_t channel = vresp->extract_U32();
        kodi::Log(ADDON_LOG_DEBUG, "Server requested Epg update for channel: %d", channel);
        kodi::addon::CInstancePVRClient::TriggerEpgUpdate(channel);
      }
    }

    // UNKOWN CHANNELID

    else if (!OnResponsePacket(vresp.get()))
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - Rxd a response packet on channel %lu !!", __func__,
                vresp->getChannelID());
    }
  }
  return nullptr;
}
