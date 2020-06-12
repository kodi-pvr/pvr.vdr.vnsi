/*
 *  Copyright (C) 2010-2020 Team Kodi
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "Session.h"

#include <kodi/addon-instance/pvr/Stream.h>
#include <map>
#include <string>

class cResponsePacket;

class ATTRIBUTE_HIDDEN CVNSIDemuxStatus : public cVNSISession
{
public:
  CVNSIDemuxStatus(kodi::addon::CInstancePVRClient& instance);
  ~CVNSIDemuxStatus() override = default;

  int GetSocket();
  void ReleaseServerClient();
  std::unique_ptr<cResponsePacket> ReadStatus();
  bool IsConnected();
};

class ATTRIBUTE_HIDDEN cVNSIDemux : public cVNSISession
{
public:
  cVNSIDemux(kodi::addon::CInstancePVRClient& instance);
  ~cVNSIDemux() override;

  void Close() override;
  bool OpenChannel(const kodi::addon::PVRChannel& channelinfo);
  void Abort();
  bool GetStreamProperties(std::vector<kodi::addon::PVRStreamProperties>& props);
  DemuxPacket* Read();
  bool SwitchChannel(const kodi::addon::PVRChannel& channelinfo);
  int CurrentChannel() { return m_channelinfo.GetChannelNumber(); }
  bool GetSignalStatus(kodi::addon::PVRSignalStatus& qualityinfo);
  bool IsTimeshift() { return m_bTimeshift; }
  bool SeekTime(int time, bool backwards, double& startpts);
  bool GetStreamTimes(kodi::addon::PVRStreamTimes& times);

protected:
  void StreamChange(cResponsePacket* resp);
  void StreamStatus(cResponsePacket* resp);
  void StreamSignalInfo(cResponsePacket* resp);
  bool StreamContentInfo(cResponsePacket* resp);
  void ReadStatus();

  std::vector<kodi::addon::PVRStreamProperties> m_streams;
  kodi::addon::PVRChannel m_channelinfo;
  kodi::addon::PVRSignalStatus m_Quality;
  bool m_bTimeshift;
  uint32_t m_MuxPacketSerial;
  time_t m_ReferenceTime;
  double m_ReferenceDTS;
  double m_minPTS;
  double m_maxPTS;
  CVNSIDemuxStatus m_statusCon;
  time_t m_lastStatus = 0;

  kodi::addon::CInstancePVRClient& m_instance;
};
