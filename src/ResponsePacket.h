/*
 *  Copyright (C) 2010-2021 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <kodi/addon-instance/PVR.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

class ATTRIBUTE_HIDDEN cResponsePacket
{
public:
  cResponsePacket(kodi::addon::CInstancePVRClient& instance);
  ~cResponsePacket();

  void setResponse(uint8_t* packet, size_t packetLength);
  void setStatus(uint8_t* packet, size_t packetLength);
  void setStream(uint8_t* packet, size_t packetLength);
  void setOSD(uint8_t* packet, size_t packetLength);

  void extractHeader();
  void extractStreamHeader();
  void extractOSDHeader();

  bool noResponse() { return (m_userData == NULL); };

  size_t getUserDataLength() const { return m_userDataLength; }
  uint32_t getChannelID() const { return m_channelID; }
  uint32_t getRequestID() const { return m_requestID; }
  uint32_t getStreamID() const { return m_streamID; }
  uint32_t getOpCodeID() const { return m_opcodeID; }
  uint32_t getDuration() const { return m_duration; }
  int64_t getDTS() const { return m_dts; }
  int64_t getPTS() const { return m_pts; }
  uint32_t getMuxSerial() const { return m_muxSerial; }
  void getOSDData(
      uint32_t& wnd, uint32_t& color, uint32_t& x0, uint32_t& y0, uint32_t& x1, uint32_t& y1);

  size_t getPacketPos() const { return m_packetPos; }
  size_t getRemainingLength() const { return m_userDataLength - m_packetPos; }

  char* extract_String();
  uint8_t extract_U8();
  uint32_t extract_U32();
  uint64_t extract_U64();
  int32_t extract_S32();
  int64_t extract_S64();
  double extract_Double();

  // If you call this, the memory becomes yours. Free with free()
  uint8_t* stealUserData();

  uint8_t* getUserData();

  uint8_t* getHeader() { return m_header; };
  size_t getStreamHeaderLength() const { return 36; };
  size_t getHeaderLength() const { return 8; };
  size_t getOSDHeaderLength() const { return 32; };

private:
  uint8_t m_header[40];
  uint8_t* m_userData = nullptr;
  size_t m_userDataLength = 0;
  size_t m_packetPos = 0;

  uint32_t m_channelID = 0;

  uint32_t m_requestID = 0;
  uint32_t m_streamID = 0;
  uint32_t m_opcodeID;
  uint32_t m_duration;
  int64_t m_dts;
  int64_t m_pts;
  uint32_t m_muxSerial;

  int32_t m_osdWnd;
  int32_t m_osdColor;

  int32_t m_osdX0;
  int32_t m_osdY0;
  int32_t m_osdX1;
  int32_t m_osdY1;

  kodi::addon::CInstancePVRClient& m_instance;
};
