/*
 *  Copyright (C) 2010-2020 Team Kodi
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "InputstreamRecording.h"

#include "RequestPacket.h"
#include "ResponsePacket.h"
#include "Settings.h"
#include "vnsicommand.h"

#include <limits.h>

#define SEEK_POSSIBLE 0x10 // flag used to check if protocol allows seeks

cVNSIRecording::cVNSIRecording(kodi::addon::CInstancePVRClient& instance)
  : cVNSISession(instance), m_instance(instance)
{
}

cVNSIRecording::~cVNSIRecording()
{
  Close();
}

bool cVNSIRecording::OpenRecording(const kodi::addon::PVRRecording& recinfo)
{
  m_recinfo = recinfo;

  if (!cVNSISession::Open(CVNSISettings::Get().GetHostname(), CVNSISettings::Get().GetPort(),
                          "XBMC RecordingStream Receiver"))
    return false;

  if (!cVNSISession::Login())
    return false;

  cRequestPacket vrp;
  vrp.init(VNSI_RECSTREAM_OPEN);
  vrp.add_U32(std::stoi(recinfo.GetRecordingId()));

  auto vresp = ReadResult(&vrp);
  if (!vresp)
    return false;

  uint32_t returnCode = vresp->extract_U32();
  if (returnCode == VNSI_RET_OK)
  {
    m_currentPlayingRecordFrames = vresp->extract_U32();
    m_currentPlayingRecordBytes = vresp->extract_U64();
    m_currentPlayingRecordPosition = 0;
  }
  else
    kodi::Log(ADDON_LOG_ERROR, "%s - Can't open recording '%s'", __func__,
              recinfo.GetTitle().c_str());

  return (returnCode == VNSI_RET_OK);
}

void cVNSIRecording::Close()
{
  if (IsOpen())
  {
    try
    {
      cRequestPacket vrp;
      vrp.init(VNSI_RECSTREAM_CLOSE);
      ReadSuccess(&vrp);
    }
    catch (std::exception e)
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    }
  }

  cVNSISession::Close();
}

int cVNSIRecording::Read(unsigned char* buf, uint32_t buf_size)
{
  if (m_connectionLost && TryReconnect() != cVNSISession::CONN_ESABLISHED)
  {
    *buf = 0;
    SleepMs(100);
    return 1;
  }

  if (m_currentPlayingRecordPosition >= m_currentPlayingRecordBytes)
  {
    GetLength();
    if (m_currentPlayingRecordPosition >= m_currentPlayingRecordBytes)
      return 0;
  }

  cRequestPacket vrp;
  vrp.init(VNSI_RECSTREAM_GETBLOCK);
  vrp.add_U64(m_currentPlayingRecordPosition);
  vrp.add_U32(buf_size);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
    return -1;

  uint32_t length = vresp->getUserDataLength();
  uint8_t* data = vresp->getUserData();
  if (length > buf_size)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s: PANIC - Received more bytes as requested", __func__);
    return 0;
  }

  memcpy(buf, data, length);
  m_currentPlayingRecordPosition += length;
  return length;
}

bool cVNSIRecording::GetStreamTimes(kodi::addon::PVRStreamTimes& times)
{
  GetLength();
  if (m_currentPlayingRecordLengthMSec == 0)
    return false;

  times.SetStartTime(0);
  times.SetPTSStart(0);
  times.SetPTSBegin(0);
  times.SetPTSEnd(m_currentPlayingRecordLengthMSec * 1000);

  return true;
}

long long cVNSIRecording::Seek(long long pos, uint32_t whence)
{
  uint64_t nextPos = m_currentPlayingRecordPosition;

  switch (whence)
  {
    case SEEK_SET:
      nextPos = pos;
      break;

    case SEEK_CUR:
      nextPos += pos;
      break;

    case SEEK_END:
      if (m_currentPlayingRecordBytes)
        nextPos = m_currentPlayingRecordBytes - pos;
      else
        return -1;

      break;

    case SEEK_POSSIBLE:
      return 1;

    default:
      return -1;
  }

  if (nextPos >= m_currentPlayingRecordBytes)
  {
    return 0;
  }

  m_currentPlayingRecordPosition = nextPos;

  return m_currentPlayingRecordPosition;
}

long long cVNSIRecording::Length(void)
{
  return m_currentPlayingRecordBytes;
}

void cVNSIRecording::OnReconnect()
{
  OpenRecording(m_recinfo);
}

void cVNSIRecording::GetLength()
{
  cRequestPacket vrp;
  vrp.init(VNSI_RECSTREAM_GETLENGTH);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
    return;

  m_currentPlayingRecordBytes = vresp->extract_U64();

  if (GetProtocol() >= 12)
    m_currentPlayingRecordLengthMSec = vresp->extract_U64();
}
