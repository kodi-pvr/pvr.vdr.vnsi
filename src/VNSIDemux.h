/*
 *  Copyright (C) 2010-2020 Team Kodi
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "VNSISession.h"
#include "client.h"
#include <string>
#include <map>
#include "kodi/xbmc_pvr_types.h"

class cResponsePacket;

struct SQuality
{
  std::string fe_name;
  std::string fe_status;
  uint32_t    fe_snr;
  uint32_t    fe_signal;
  uint32_t    fe_ber;
  uint32_t    fe_unc;
};

class CVNSIDemuxStatus : public cVNSISession
{
public:

  CVNSIDemuxStatus() = default;
  virtual ~CVNSIDemuxStatus() = default;

  int GetSocket();
  void ReleaseServerClient();
  std::unique_ptr<cResponsePacket> ReadStatus();
  bool IsConnected();
};

class cVNSIDemux : public cVNSISession
{
public:

  cVNSIDemux();
  virtual ~cVNSIDemux();

  void Close();
  bool OpenChannel(const PVR_CHANNEL &channelinfo);
  void Abort();
  bool GetStreamProperties(PVR_STREAM_PROPERTIES* props);
  DemuxPacket* Read();
  bool SwitchChannel(const PVR_CHANNEL &channelinfo);
  int CurrentChannel() { return m_channelinfo.iChannelNumber; }
  bool GetSignalStatus(PVR_SIGNAL_STATUS *qualityinfo);
  bool IsTimeshift() { return m_bTimeshift; }
  bool SeekTime(int time, bool backwards, double *startpts);
  bool GetStreamTimes(PVR_STREAM_TIMES *times);

protected:

  void StreamChange(cResponsePacket *resp);
  void StreamStatus(cResponsePacket *resp);
  void StreamSignalInfo(cResponsePacket *resp);
  bool StreamContentInfo(cResponsePacket *resp);
  void ReadStatus();

  PVR_STREAM_PROPERTIES m_streams;
  PVR_CHANNEL m_channelinfo;
  SQuality m_Quality;
  bool m_bTimeshift;
  uint32_t m_MuxPacketSerial;
  time_t m_ReferenceTime;
  double m_ReferenceDTS = 0.;
  double m_minPTS;
  double m_maxPTS;
  CVNSIDemuxStatus m_statusCon;
  time_t m_lastStatus = 0;
};
