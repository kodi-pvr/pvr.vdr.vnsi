/*
 *      Copyright (C) 2010 Alwin Esch (Team XBMC)
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <stdint.h>
#include <limits.h>
#include <string.h>
#include "VNSIDemux.h"
#include "responsepacket.h"
#include "requestpacket.h"
#include "vnsicommand.h"
#include "tools.h"

using namespace ADDON;

cVNSIDemux::cVNSIDemux()
{
}

cVNSIDemux::~cVNSIDemux()
{
}

void cVNSIDemux::Close()
{
  if (IsOpen() && GetProtocol() >= 9)
  {
    XBMC->Log(LOG_DEBUG, "closing demuxer");

    cRequestPacket vrp;
    vrp.init(VNSI_CHANNELSTREAM_CLOSE);

    try {
      auto resp = ReadResult(&vrp);
      if (!resp)
      {
          XBMC->Log(LOG_ERROR, "%s - failed to close streaming", __FUNCTION__);
      }
    } catch (std::exception e) {
      XBMC->Log(LOG_ERROR, "%s - %s", __FUNCTION__, e.what());
    }
  }

  cVNSISession::Close();
}

bool cVNSIDemux::OpenChannel(const PVR_CHANNEL &channelinfo)
{
  m_channelinfo = channelinfo;
  if(!cVNSISession::Open(g_szHostname, g_iPort))
    return false;

  if(!cVNSISession::Login())
    return false;

  return SwitchChannel(m_channelinfo);
}

bool cVNSIDemux::GetStreamProperties(PVR_STREAM_PROPERTIES* props)
{
  for (int i=0; i<m_streams.iStreamCount; i++)
  {
    memcpy(&props->stream[i], &m_streams.stream[i], sizeof(PVR_STREAM_PROPERTIES::PVR_STREAM));
  }

  props->iStreamCount = m_streams.iStreamCount;
  return true;
}

void cVNSIDemux::Abort()
{
  m_streams.iStreamCount = 0;
}

DemuxPacket* cVNSIDemux::Read()
{
  if (m_connectionLost)
  {
    return NULL;
  }

  auto resp = ReadMessage(1000, g_iConnectTimeout * 1000);

  if(resp == NULL)
    return PVR->AllocateDemuxPacket(0);

  if (resp->getChannelID() != VNSI_CHANNEL_STREAM)
  {
    return NULL;
  }

  if (resp->getOpCodeID() == VNSI_STREAM_CHANGE)
  {
    StreamChange(resp.get());
    DemuxPacket* pkt = PVR->AllocateDemuxPacket(0);
    pkt->iStreamId  = DMX_SPECIALID_STREAMCHANGE;
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
    if(StreamContentInfo(resp.get()))
    {
      DemuxPacket* pkt = PVR->AllocateDemuxPacket(0);
      pkt->iStreamId  = DMX_SPECIALID_STREAMCHANGE;
      return pkt;
    }
  }
  else if (resp->getOpCodeID() == VNSI_STREAM_MUXPKT)
  {
    // figure out the stream id for this packet
    int pid = resp->getStreamID();

    // stream found ?
    if(pid >= 0 && resp->getMuxSerial() == m_MuxPacketSerial)
    {
      DemuxPacket* p = (DemuxPacket*)resp->stealUserData();
      p->iSize = resp->getUserDataLength();
      p->duration = (double)resp->getDuration() * DVD_TIME_BASE / 1000000;
      p->dts = (double)resp->getDTS() * DVD_TIME_BASE / 1000000;
      p->pts = (double)resp->getPTS() * DVD_TIME_BASE / 1000000;
      p->iStreamId = pid;

      int idx = -1;
      for (int i=0; i<m_streams.iStreamCount; i++)
      {
        if (m_streams.stream[i].iPID == pid)
          idx = i;
      }
      xbmc_codec_type_t type = XBMC_CODEC_TYPE_UNKNOWN;
      if (idx >= 0)
        type = m_streams.stream[idx].iCodecType;
      if (type == XBMC_CODEC_TYPE_VIDEO || type == XBMC_CODEC_TYPE_AUDIO)
      {
        if (p->dts != DVD_NOPTS_VALUE)
          m_CurrentDTS = p->dts;
        else if (p->pts != DVD_NOPTS_VALUE)
          m_CurrentDTS = p->pts;
      }
      return p;
    }
    else if (pid >= 0 && resp->getMuxSerial() != m_MuxPacketSerial)
    {
      // ignore silently, may happen after a seek
    }
    else
    {
      XBMC->Log(LOG_DEBUG, "stream id %i not found", resp->getStreamID());
    }
  }
  else if (resp->getOpCodeID() == VNSI_STREAM_BUFFERSTATS)
  {
    m_bTimeshift = resp->extract_U8();
    m_BufferTimeStart = resp->extract_U32();
    m_BufferTimeEnd = resp->extract_U32();
  }
  else if (resp->getOpCodeID() == VNSI_STREAM_REFTIME)
  {
    m_ReferenceTime = resp->extract_U32();
    m_ReferenceDTS = (double)resp->extract_U64() * DVD_TIME_BASE / 1000000;
  }

  return PVR->AllocateDemuxPacket(0);
}

bool cVNSIDemux::SeekTime(int time, bool backwards, double *startpts)
{
  cRequestPacket vrp;

  int64_t seek_pts = (int64_t)time * 1000;
  if (startpts)
    *startpts = seek_pts;

  vrp.init(VNSI_CHANNELSTREAM_SEEK);
  vrp.add_S64(seek_pts);
  vrp.add_U8(backwards);

  auto resp = ReadResult(&vrp);
  if (!resp)
  {
    XBMC->Log(LOG_ERROR, "%s - failed to seek2", __FUNCTION__);
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

bool cVNSIDemux::SwitchChannel(const PVR_CHANNEL &channelinfo)
{
  XBMC->Log(LOG_DEBUG, "changing to channel %d", channelinfo.iChannelNumber);

  cRequestPacket vrp1;
  vrp1.init(VNSI_GETSETUP);
  vrp1.add_String(CONFNAME_TIMESHIFT);

  auto resp = ReadResult(&vrp1);
  if (!resp)
  {
    XBMC->Log(LOG_ERROR, "%s - failed to get timeshift mode", __FUNCTION__);
    return false;
  }
  m_bTimeshift = resp->extract_U32();

  cRequestPacket vrp2;
  vrp2.init(VNSI_CHANNELSTREAM_OPEN);
  vrp2.add_U32(channelinfo.iUniqueId);
  vrp2.add_S32(g_iPriority);
  vrp2.add_U8(g_iTimeshift);

  if (!ReadSuccess(&vrp2))
  {
    XBMC->Log(LOG_ERROR, "%s - failed to set channel", __FUNCTION__);
    return false;
  }

  m_channelinfo = channelinfo;
  m_streams.iStreamCount = 0;
  m_MuxPacketSerial = 0;
  m_ReferenceTime = 0;
  m_BufferTimeStart = 0;
  m_BufferTimeEnd = 0;

  return true;
}

bool cVNSIDemux::GetSignalStatus(PVR_SIGNAL_STATUS &qualityinfo)
{
  if (m_Quality.fe_name.empty())
    return true;

  strncpy(qualityinfo.strAdapterName, m_Quality.fe_name.c_str(), sizeof(qualityinfo.strAdapterName));
  strncpy(qualityinfo.strAdapterStatus, m_Quality.fe_status.c_str(), sizeof(qualityinfo.strAdapterStatus));
  qualityinfo.iSignal = (uint16_t)m_Quality.fe_signal;
  qualityinfo.iSNR = (uint16_t)m_Quality.fe_snr;
  qualityinfo.iBER = (uint32_t)m_Quality.fe_ber;
  qualityinfo.iUNC = (uint32_t)m_Quality.fe_unc;

  return true;
}

time_t cVNSIDemux::GetPlayingTime()
{
  time_t ret = 0;
  if (m_ReferenceTime)
    ret = m_ReferenceTime + (m_CurrentDTS - m_ReferenceDTS) / DVD_TIME_BASE;
  return ret;
}

time_t cVNSIDemux::GetBufferTimeStart()
{
  return m_BufferTimeStart;
}

time_t cVNSIDemux::GetBufferTimeEnd()
{
  return m_BufferTimeEnd;
}

void cVNSIDemux::StreamChange(cResponsePacket *resp)
{
  int count = 0;
  m_streams.iStreamCount = 0;

  while (resp->getRemainingLength() >= 4 + 1)
  {
    uint32_t    pid = resp->extract_U32();
    const char* type  = resp->extract_String();

    memset(&m_streams.stream[count], 0, sizeof(PVR_STREAM_PROPERTIES::PVR_STREAM));

    CodecDescriptor codecId = CodecDescriptor::GetCodecByName(type);
    if (codecId.Codec().codec_type != XBMC_CODEC_TYPE_UNKNOWN)
    {
      m_streams.stream[count].iPID = pid;
      m_streams.stream[count].iCodecType = codecId.Codec().codec_type;
      m_streams.stream[count].iCodecId = codecId.Codec().codec_id;
    }
    else
    {
      return;
    }

    if (codecId.Codec().codec_type == XBMC_CODEC_TYPE_AUDIO)
    {
      const char *language = resp->extract_String();

      m_streams.stream[count].iChannels = resp->extract_U32();
      m_streams.stream[count].iSampleRate = resp->extract_U32();
      m_streams.stream[count].iBlockAlign = resp->extract_U32();
      m_streams.stream[count].iBitRate = resp->extract_U32();
      m_streams.stream[count].iBitsPerSample = resp->extract_U32();
      m_streams.stream[count].strLanguage[0] = language[0];
      m_streams.stream[count].strLanguage[1] = language[1];
      m_streams.stream[count].strLanguage[2] = language[2];
      m_streams.stream[count].strLanguage[3] = 0;
    }
    else if (codecId.Codec().codec_type == XBMC_CODEC_TYPE_VIDEO)
    {
      m_streams.stream[count].iFPSScale = resp->extract_U32();
      m_streams.stream[count].iFPSRate = resp->extract_U32();
      m_streams.stream[count].iHeight = resp->extract_U32();
      m_streams.stream[count].iWidth = resp->extract_U32();
      m_streams.stream[count].fAspect = (float)resp->extract_Double();
      m_streams.stream[count].strLanguage[0] = 0;
      m_streams.stream[count].strLanguage[1] = 0;
      m_streams.stream[count].strLanguage[2] = 0;
      m_streams.stream[count].strLanguage[3] = 0;
    }
    else if (codecId.Codec().codec_type == XBMC_CODEC_TYPE_SUBTITLE)
    {
      const char *language = resp->extract_String();
      uint32_t composition_id = resp->extract_U32();
      uint32_t ancillary_id = resp->extract_U32();
      m_streams.stream[count].strLanguage[0] = language[0];
      m_streams.stream[count].strLanguage[1] = language[1];
      m_streams.stream[count].strLanguage[2] = language[2];
      m_streams.stream[count].strLanguage[3] = 0;
      m_streams.stream[count].iSubtitleInfo = (composition_id & 0xffff) | ((ancillary_id & 0xffff) << 16);
    }
    else if (codecId.Codec().codec_type == XBMC_CODEC_TYPE_RDS)
    {
      const char *language = resp->extract_String();
      uint32_t rel_channel_pid = resp->extract_U32();
      m_streams.stream[count].strLanguage[0] = language[0];
      m_streams.stream[count].strLanguage[1] = language[1];
      m_streams.stream[count].strLanguage[2] = language[2];
      m_streams.stream[count].strLanguage[3] = 0;
    }
    else
    {
      m_streams.iStreamCount = 0;
      return;
    }
    count++;
  }
  m_streams.iStreamCount = count;
}

void cVNSIDemux::StreamStatus(cResponsePacket *resp)
{
  const char* status = resp->extract_String();
  if(status != NULL)
  {
    XBMC->Log(LOG_DEBUG, "%s - %s", __FUNCTION__, status);
    XBMC->QueueNotification(QUEUE_INFO, status);
  }
}

void cVNSIDemux::StreamSignalInfo(cResponsePacket *resp)
{
  const char* name = resp->extract_String();
  const char* status = resp->extract_String();

  m_Quality.fe_name   = name;
  m_Quality.fe_status = status;
  m_Quality.fe_snr    = resp->extract_U32();
  m_Quality.fe_signal = resp->extract_U32();
  m_Quality.fe_ber    = resp->extract_U32();
  m_Quality.fe_unc    = resp->extract_U32();
}

bool cVNSIDemux::StreamContentInfo(cResponsePacket *resp)
{
  while (resp->getRemainingLength() >= 4)
  {
    uint32_t pid = resp->extract_U32();

    PVR_STREAM_PROPERTIES::PVR_STREAM* props = nullptr;
    for (int i=0; i<m_streams.iStreamCount; i++)
    {
      if (m_streams.stream[i].iPID == pid)
      {
        props = &m_streams.stream[i];
        break;
      }
    }
    if (props)
    {
      if (props->iCodecType == XBMC_CODEC_TYPE_AUDIO)
      {
        const char *language = resp->extract_String();
          
        props->iChannels = resp->extract_U32();
        props->iSampleRate = resp->extract_U32();
        props->iBlockAlign = resp->extract_U32();
        props->iBitRate = resp->extract_U32();
        props->iBitsPerSample = resp->extract_U32();
        props->strLanguage[0] = language[0];
        props->strLanguage[1] = language[1];
        props->strLanguage[2] = language[2];
        props->strLanguage[3] = 0;
      }
      else if (props->iCodecType == XBMC_CODEC_TYPE_VIDEO)
      {
        props->iFPSScale = resp->extract_U32();
        props->iFPSRate = resp->extract_U32();
        props->iHeight = resp->extract_U32();
        props->iWidth = resp->extract_U32();
        props->fAspect = (float)resp->extract_Double();
      }
      else if (props->iCodecType == XBMC_CODEC_TYPE_SUBTITLE)
      {
        const char *language = resp->extract_String();
        uint32_t composition_id = resp->extract_U32();
        uint32_t ancillary_id = resp->extract_U32();
          
        props->iSubtitleInfo = (composition_id & 0xffff) | ((ancillary_id & 0xffff) << 16);
        props->strLanguage[0] = language[0];
        props->strLanguage[1] = language[1];
        props->strLanguage[2] = language[2];
        props->strLanguage[3] = 0;
      }
    }
    else
    {
      XBMC->Log(LOG_ERROR, "%s - unknown stream id: %d", __FUNCTION__, pid);
      break;
    }
  }
  return true;
}
