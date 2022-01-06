/*
 *  Copyright (C) 2010-2021 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "InputstreamDemux.h"

#include "ClientInstance.h"
#include "RequestPacket.h"
#include "ResponsePacket.h"
#include "Settings.h"
#include "Tools.h"
#include "vnsicommand.h"

#include <kodi/General.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

cVNSIDemux::cVNSIDemux(kodi::addon::CInstancePVRClient& instance)
  : cVNSISession(instance), m_statusCon(instance), m_instance(instance)
{
}

cVNSIDemux::~cVNSIDemux()
{
  Close();
}

void cVNSIDemux::Close()
{
  if (IsOpen() && GetProtocol() >= 9)
  {
    kodi::Log(ADDON_LOG_DEBUG, "closing demuxer");

    cRequestPacket vrp;
    vrp.init(VNSI_CHANNELSTREAM_CLOSE);

    try
    {
      auto resp = ReadResult(&vrp);
      if (!resp)
      {
        kodi::Log(ADDON_LOG_ERROR, "%s - failed to close streaming", __func__);
      }
    }
    catch (std::exception e)
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    }
  }

  cVNSISession::Close();
}

bool cVNSIDemux::OpenChannel(const kodi::addon::PVRChannel& channelinfo)
{
  m_channelinfo = channelinfo;
  if (!cVNSISession::Open(CVNSISettings::Get().GetHostname(), CVNSISettings::Get().GetPort()))
    return false;

  if (!cVNSISession::Login())
    return false;

  return SwitchChannel(m_channelinfo);
}

bool cVNSIDemux::GetStreamProperties(std::vector<kodi::addon::PVRStreamProperties>& props)
{
  props = m_streams;
  return true;
}

void cVNSIDemux::Abort()
{
  m_streams.clear();
}

DEMUX_PACKET* cVNSIDemux::Read()
{
  if (m_connectionLost)
  {
    return nullptr;
  }

  ReadStatus();

  auto resp = ReadMessage(1000, CVNSISettings::Get().GetConnectTimeout() * 1000);

  if (resp == nullptr)
    return m_instance.AllocateDemuxPacket(0);

  if (resp->getChannelID() != VNSI_CHANNEL_STREAM)
  {
    return nullptr;
  }

  if (resp->getOpCodeID() == VNSI_STREAM_CHANGE)
  {
    StreamChange(resp.get());
    DEMUX_PACKET* pkt = m_instance.AllocateDemuxPacket(0);
    pkt->iStreamId = DEMUX_SPECIALID_STREAMCHANGE;
    return pkt;
  }
  else if (resp->getOpCodeID() == VNSI_STREAM_STATUS)
  {
    StreamStatus(resp.get());
  }
  else if (resp->getOpCodeID() == VNSI_STREAM_SIGNALINFO)
  {
    StreamSignalInfo(resp.get());
  }
  else if (resp->getOpCodeID() == VNSI_STREAM_CONTENTINFO)
  {
    // send stream updates only if there are changes
    if (StreamContentInfo(resp.get()))
    {
      DEMUX_PACKET* pkt = m_instance.AllocateDemuxPacket(0);
      pkt->iStreamId = DEMUX_SPECIALID_STREAMCHANGE;
      return pkt;
    }
  }
  else if (resp->getOpCodeID() == VNSI_STREAM_MUXPKT)
  {
    // figure out the stream id for this packet
    int pid = resp->getStreamID();

    // stream found ?
    if (pid >= 0 && resp->getMuxSerial() == m_MuxPacketSerial)
    {
      DEMUX_PACKET* p = (DEMUX_PACKET*)resp->stealUserData();
      p->iSize = resp->getUserDataLength();
      p->duration = static_cast<double>(resp->getDuration() * STREAM_TIME_BASE / 1000000);
      p->dts = static_cast<double>(resp->getDTS() * STREAM_TIME_BASE / 1000000);
      p->pts = static_cast<double>(resp->getPTS() * STREAM_TIME_BASE / 1000000);
      p->iStreamId = pid;

      int idx = -1;
      for (size_t i = 0; i < m_streams.size(); i++)
      {
        if (m_streams[i].GetPID() == pid)
          idx = i;
      }
      PVR_CODEC_TYPE type = PVR_CODEC_TYPE_UNKNOWN;
      if (idx >= 0)
        type = m_streams[idx].GetCodecType();

      return p;
    }
    else if (pid >= 0 && resp->getMuxSerial() != m_MuxPacketSerial)
    {
      // ignore silently, may happen after a seek
    }
    else
    {
      kodi::Log(ADDON_LOG_DEBUG, "stream id %i not found", resp->getStreamID());
    }
  }
  else if (resp->getOpCodeID() == VNSI_STREAM_TIMES)
  {
    m_bTimeshift = resp->extract_U8();
    m_ReferenceTime = resp->extract_U32();
    m_ReferenceDTS = static_cast<double>(resp->extract_U64());
    m_minPTS = static_cast<double>(resp->extract_U64());
    m_maxPTS = static_cast<double>(resp->extract_U64());
  }
  else if (resp->getOpCodeID() == VNSI_STREAM_BUFFERSTATS)
  {
    m_bTimeshift = resp->extract_U8();
    m_minPTS = (resp->extract_U32() - m_ReferenceTime) * STREAM_TIME_BASE + m_ReferenceDTS;
    m_maxPTS = (resp->extract_U32() - m_ReferenceTime) * STREAM_TIME_BASE + m_ReferenceDTS;
  }
  else if (resp->getOpCodeID() == VNSI_STREAM_REFTIME)
  {
    m_ReferenceTime = resp->extract_U32();
    m_ReferenceDTS = static_cast<double>(resp->extract_U64());
  }

  return m_instance.AllocateDemuxPacket(0);
}

void cVNSIDemux::ReadStatus()
{
  if (m_connectionLost || !m_statusCon.IsConnected())
  {
    return;
  }

  while (true)
  {
    auto resp = m_statusCon.ReadStatus();
    if (!resp)
    {
      if ((time(nullptr) - m_lastStatus) > 2)
      {
        cRequestPacket vrp;
        vrp.init(VNSI_CHANNELSTREAM_STATUS_REQUEST);
        if (!TransmitMessage(&vrp))
        {
          SignalConnectionLost();
        }
      }
      break;
    }

    if (resp->getOpCodeID() == VNSI_STREAM_TIMES)
    {
      m_bTimeshift = resp->extract_U8();
      m_ReferenceTime = resp->extract_U32();
      m_ReferenceDTS = static_cast<double>(resp->extract_U64());
      m_minPTS = static_cast<double>(resp->extract_U64());
      m_maxPTS = static_cast<double>(resp->extract_U64());
    }
    else if (resp->getOpCodeID() == VNSI_STREAM_SIGNALINFO)
    {
      StreamSignalInfo(resp.get());
    }
    else if (resp->getOpCodeID() == VNSI_STREAM_STATUS)
    {
      StreamStatus(resp.get());
    }

    m_lastStatus = time(nullptr);
  }
}

bool cVNSIDemux::GetStreamTimes(kodi::addon::PVRStreamTimes& times)
{
  ReadStatus();

  times.SetStartTime(m_ReferenceTime);
  times.SetPTSStart(m_ReferenceDTS);
  times.SetPTSBegin(m_minPTS);
  times.SetPTSEnd(m_maxPTS);
  return true;
}

bool cVNSIDemux::SeekTime(int time, bool backwards, double& startpts)
{
  cRequestPacket vrp;

  int64_t seek_pts = static_cast<int64_t>(time) * 1000;
  startpts = seek_pts;

  vrp.init(VNSI_CHANNELSTREAM_SEEK);
  vrp.add_S64(seek_pts);
  vrp.add_U8(backwards);

  auto resp = ReadResult(&vrp);
  if (!resp)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - failed to seek2", __func__);
    return false;
  }
  uint32_t retCode = resp->extract_U32();
  uint32_t serial = resp->extract_U32();

  if (retCode == VNSI_RET_OK)
  {
    m_MuxPacketSerial = serial;
    return true;
  }
  else
    return false;
}

bool cVNSIDemux::SwitchChannel(const kodi::addon::PVRChannel& channelinfo)
{
  kodi::Log(ADDON_LOG_DEBUG, "changing to channel %d", channelinfo.GetChannelNumber());

  cRequestPacket vrp1;
  vrp1.init(VNSI_GETSETUP);
  vrp1.add_String(CONFNAME_TIMESHIFT);

  auto resp = ReadResult(&vrp1);
  if (!resp)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - failed to get timeshift mode", __func__);
    return false;
  }
  m_bTimeshift = resp->extract_U32();

  cRequestPacket vrp2;
  vrp2.init(VNSI_CHANNELSTREAM_OPEN);
  vrp2.add_U32(channelinfo.GetUniqueId());
  vrp2.add_S32(CVNSISettings::Get().GetPriority());
  vrp2.add_U8(CVNSISettings::Get().GetTimeshift());

  if (!ReadSuccess(&vrp2))
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - failed to set channel", __func__);
    return false;
  }

  if (GetProtocol() >= 13)
  {
    int fd = m_statusCon.GetSocket();
    if (fd >= 0)
    {
      cRequestPacket vrp;
      vrp.init(VNSI_CHANNELSTREAM_STATUS_SOCKET);
      vrp.add_S32(fd);
      if (ReadSuccess(&vrp))
      {
        m_statusCon.ReleaseServerClient();
        kodi::Log(ADDON_LOG_DEBUG, "%s - established status connection", __func__);
      }
    }
  }

  m_channelinfo = channelinfo;
  m_MuxPacketSerial = 0;
  m_ReferenceTime = 0;
  m_minPTS = 0;
  m_maxPTS = 0;

  m_streams.clear();

  return true;
}

bool cVNSIDemux::GetSignalStatus(kodi::addon::PVRSignalStatus& qualityinfo)
{
  qualityinfo = m_Quality;
  return true;
}

void cVNSIDemux::StreamChange(cResponsePacket* resp)
{
  m_streams.clear();

  while (resp->getRemainingLength() >= 4 + 1)
  {
    uint32_t pid = resp->extract_U32();
    const char* type = resp->extract_String();

    std::string codecName;
    if (!strcmp(type, "MPEG2AUDIO"))
      codecName = "MP2";
    else if (!strcmp(type, "MPEGTS"))
      codecName = "MPEG2VIDEO";
    else if (!strcmp(type, "TEXTSUB"))
      codecName = "TEXT";
    else
      codecName = type;

    kodi::addon::PVRStreamProperties props;
    kodi::addon::PVRCodec codecId = m_instance.GetCodecByName(codecName);

    if (codecId.GetCodecType() != PVR_CODEC_TYPE_UNKNOWN)
    {
      props.SetPID(pid);
      props.SetCodecType(codecId.GetCodecType());
      props.SetCodecId(codecId.GetCodecId());
    }
    else
    {
      return;
    }

    if (codecId.GetCodecType() == PVR_CODEC_TYPE_AUDIO)
    {
      const char* language = resp->extract_String();

      props.SetChannels(resp->extract_U32());
      props.SetSampleRate(resp->extract_U32());
      props.SetBlockAlign(resp->extract_U32());
      props.SetBitRate(resp->extract_U32());
      props.SetBitsPerSample(resp->extract_U32());
      props.SetLanguage(language);
    }
    else if (codecId.GetCodecType() == PVR_CODEC_TYPE_VIDEO)
    {
      props.SetFPSScale(resp->extract_U32());
      props.SetFPSRate(resp->extract_U32());
      props.SetHeight(resp->extract_U32());
      props.SetWidth(resp->extract_U32());
      props.SetAspect((float)resp->extract_Double());
    }
    else if (codecId.GetCodecType() == PVR_CODEC_TYPE_SUBTITLE)
    {
      const char* language = resp->extract_String();
      uint32_t composition_id(resp->extract_U32());
      uint32_t ancillary_id(resp->extract_U32());
      if (std::strlen(language) == 3)
      {
        props.SetLanguage(language);
        props.SetSubtitleInfo((composition_id & 0xffff) | ((ancillary_id & 0xffff) << 16));
      }
    }
    else if (codecId.GetCodecType() == PVR_CODEC_TYPE_RDS)
    {
      const char* language = resp->extract_String();
      uint32_t rel_channel_pid(resp->extract_U32());
      props.SetLanguage(language);
    }
    else
    {
      m_streams.clear();
      return;
    }
    m_streams.push_back(props);
  }
}

void cVNSIDemux::StreamStatus(cResponsePacket* resp)
{
  const char* status = resp->extract_String();
  if (status != nullptr)
  {
    kodi::Log(ADDON_LOG_DEBUG, "%s - %s", __func__, status);
    kodi::QueueNotification(QUEUE_INFO, "", status);
  }
}

void cVNSIDemux::StreamSignalInfo(cResponsePacket* resp)
{
  m_Quality.SetAdapterName(resp->extract_String());
  m_Quality.SetAdapterStatus(resp->extract_String());
  m_Quality.SetSNR(resp->extract_U32());
  m_Quality.SetSignal(resp->extract_U32());
  m_Quality.SetBER(resp->extract_U32());
  m_Quality.SetUNC(resp->extract_U32());
}

bool cVNSIDemux::StreamContentInfo(cResponsePacket* resp)
{
  while (resp->getRemainingLength() >= 4)
  {
    uint32_t pid = resp->extract_U32();

    kodi::addon::PVRStreamProperties* props = nullptr;
    for (auto& stream : m_streams)
    {
      if (stream.GetPID() == pid)
      {
        props = &stream;
        break;
      }
    }
    if (props)
    {
      if (props->GetCodecType() == PVR_CODEC_TYPE_AUDIO)
      {
        const char* language(resp->extract_String());

        props->SetChannels(resp->extract_U32());
        props->SetSampleRate(resp->extract_U32());
        props->SetBlockAlign(resp->extract_U32());
        props->SetBitRate(resp->extract_U32());
        props->SetBitsPerSample(resp->extract_U32());
        props->SetLanguage(language);
      }
      else if (props->GetCodecType() == PVR_CODEC_TYPE_VIDEO)
      {
        props->SetFPSScale(resp->extract_U32());
        props->SetFPSRate(resp->extract_U32());
        props->SetHeight(resp->extract_U32());
        props->SetWidth(resp->extract_U32());
        props->SetAspect(static_cast<float>(resp->extract_Double()));
      }
      else if (props->GetCodecType() == PVR_CODEC_TYPE_SUBTITLE)
      {
        const char* language(resp->extract_String());
        uint32_t composition_id(resp->extract_U32());
        uint32_t ancillary_id(resp->extract_U32());

        props->SetSubtitleInfo((composition_id & 0xffff) | ((ancillary_id & 0xffff) << 16));
        props->SetLanguage(language);
      }
    }
    else
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - unknown stream id: %d", __func__, pid);
      break;
    }
  }
  return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

CVNSIDemuxStatus::CVNSIDemuxStatus(kodi::addon::CInstancePVRClient& instance)
  : cVNSISession(instance)
{
}

int CVNSIDemuxStatus::GetSocket()
{
  Close();

  if (!Open(CVNSISettings::Get().GetHostname(), CVNSISettings::Get().GetPort()))
  {
    return -1;
  }

  if (!Login())
  {
    return -1;
  }

  cRequestPacket vrp;
  vrp.init(VNSI_GETSOCKET);
  auto resp = ReadResult(&vrp);
  if (!resp)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - failed to get socket", __func__);
    return -1;
  }
  int fd = resp->extract_S32();
  return fd;
}

void CVNSIDemuxStatus::ReleaseServerClient()
{
  cRequestPacket vrp;
  vrp.init(VNSI_INVALIDATESOCKET);
  if (!ReadSuccess(&vrp))
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - failed to release server client", __func__);
  }
}

std::unique_ptr<cResponsePacket> CVNSIDemuxStatus::ReadStatus()
{
  if (!IsOpen())
    return nullptr;

  return ReadMessage(1, 10000);
}

bool CVNSIDemuxStatus::IsConnected()
{
  return IsOpen();
}
