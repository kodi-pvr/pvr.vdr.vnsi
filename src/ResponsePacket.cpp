/*
 *  Copyright (C) 2010-2020 Team Kodi
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "ResponsePacket.h"

#include "ClientInstance.h"
#include "Tools.h"
#include "vnsicommand.h"

#include <p8-platform/sockets/tcp.h>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>

cResponsePacket::cResponsePacket(kodi::addon::CInstancePVRClient& instance) : m_instance(instance)
{
}

cResponsePacket::~cResponsePacket()
{
  if (m_userData)
  {
    if (m_channelID == VNSI_CHANNEL_STREAM && m_opcodeID == VNSI_STREAM_MUXPKT)
      m_instance.FreeDemuxPacket(reinterpret_cast<DemuxPacket*>(m_userData));
    else
      free(m_userData);
  }
}

void cResponsePacket::getOSDData(
    uint32_t& wnd, uint32_t& color, uint32_t& x0, uint32_t& y0, uint32_t& x1, uint32_t& y1)
{
  wnd = m_osdWnd;
  color = m_osdColor;
  x0 = m_osdX0;
  y0 = m_osdY0;
  x1 = m_osdX1;
  y1 = m_osdY1;
}

void cResponsePacket::setResponse(uint8_t* tuserData, size_t tuserDataLength)
{
  m_channelID = VNSI_CHANNEL_REQUEST_RESPONSE;
  m_userData = tuserData;
  m_userDataLength = tuserDataLength;
  m_packetPos = 0;
}

void cResponsePacket::setStatus(uint8_t* tuserData, size_t tuserDataLength)
{
  m_channelID = VNSI_CHANNEL_STATUS;
  m_userData = tuserData;
  m_userDataLength = tuserDataLength;
  m_packetPos = 0;
}

void cResponsePacket::setStream(uint8_t* tuserData, size_t tuserDataLength)
{
  m_channelID = VNSI_CHANNEL_STREAM;
  // set pointer to user data
  m_userData = tuserData;
  m_userDataLength = tuserDataLength;
  m_packetPos = 0;
}

void cResponsePacket::setOSD(uint8_t* tuserData, size_t tuserDataLength)
{
  m_channelID = VNSI_CHANNEL_OSD;
  // set pointer to user data
  m_userData = tuserData;
  m_userDataLength = tuserDataLength;
  m_packetPos = 0;
}

void cResponsePacket::extractHeader()
{
  // set data pointers to header first
  m_userData = m_header;
  m_userDataLength = sizeof(m_header);
  m_packetPos = 0;

  m_requestID = extract_U32();
  m_userDataLength = extract_U32();

  m_userData = nullptr;
}

void cResponsePacket::extractStreamHeader()
{
  m_channelID = VNSI_CHANNEL_STREAM;

  // set data pointers to header first
  m_userData = m_header;
  m_userDataLength = sizeof(m_header);
  m_packetPos = 0;

  m_opcodeID = extract_U32();
  m_streamID = extract_U32();
  m_duration = extract_U32();
  m_pts = extract_U64();
  m_dts = extract_U64();
  m_muxSerial = extract_U32();

  m_userDataLength = extract_U32();

  m_userData = nullptr;
}

void cResponsePacket::extractOSDHeader()
{
  m_channelID = VNSI_CHANNEL_OSD;

  // set data pointers to header first
  m_userData = m_header;
  m_userDataLength = sizeof(m_header);
  m_packetPos = 0;

  m_opcodeID = extract_U32();
  m_osdWnd = extract_S32();
  m_osdColor = extract_S32();
  m_osdX0 = extract_S32();
  m_osdY0 = extract_S32();
  m_osdX1 = extract_S32();
  m_osdY1 = extract_S32();
  m_userDataLength = extract_U32();
}

char* cResponsePacket::extract_String()
{
  char* p = (char*)&m_userData[m_packetPos];
  const char* end = static_cast<const char*>(memchr(p, '\0', m_userDataLength - m_packetPos));
  if (end == nullptr)
    /* string is not terminated - fail */
    throw std::out_of_range("Malformed VNSI packet");

  int length = end - p;
  m_packetPos += length + 1;
  return p;
}

uint8_t cResponsePacket::extract_U8()
{
  if ((m_packetPos + sizeof(uint8_t)) > m_userDataLength)
    throw std::out_of_range("Malformed VNSI packet");
  uint8_t uc = m_userData[m_packetPos];
  m_packetPos += sizeof(uint8_t);
  return uc;
}

uint32_t cResponsePacket::extract_U32()
{
  if ((m_packetPos + sizeof(uint32_t)) > m_userDataLength)
    throw std::out_of_range("Malformed VNSI packet");
  uint32_t ul;
  memcpy(&ul, &m_userData[m_packetPos], sizeof(uint32_t));
  ul = ntohl(ul);
  m_packetPos += sizeof(uint32_t);
  return ul;
}

uint64_t cResponsePacket::extract_U64()
{
  if ((m_packetPos + sizeof(uint64_t)) > m_userDataLength)
    throw std::out_of_range("Malformed VNSI packet");
  uint64_t ull;
  memcpy(&ull, &m_userData[m_packetPos], sizeof(uint64_t));
  ull = ntohll(ull);
  m_packetPos += sizeof(uint64_t);
  return ull;
}

double cResponsePacket::extract_Double()
{
  if ((m_packetPos + sizeof(uint64_t)) > m_userDataLength)
    throw std::out_of_range("Malformed VNSI packet");
  uint64_t ull;
  memcpy(&ull, &m_userData[m_packetPos], sizeof(uint64_t));
  ull = ntohll(ull);
  double d;
  memcpy(&d, &ull, sizeof(double));
  m_packetPos += sizeof(uint64_t);
  return d;
}

int32_t cResponsePacket::extract_S32()
{
  if ((m_packetPos + sizeof(int32_t)) > m_userDataLength)
    throw std::out_of_range("Malformed VNSI packet");
  int32_t l;
  memcpy(&l, &m_userData[m_packetPos], sizeof(int32_t));
  l = ntohl(l);
  m_packetPos += sizeof(int32_t);
  return l;
}

int64_t cResponsePacket::extract_S64()
{
  if ((m_packetPos + sizeof(int64_t)) > m_userDataLength)
    throw std::out_of_range("Malformed VNSI packet");
  int64_t ll;
  memcpy(&ll, &m_userData[m_packetPos], sizeof(int64_t));
  ll = ntohll(ll);
  m_packetPos += sizeof(int64_t);
  return ll;
}

uint8_t* cResponsePacket::getUserData()
{
  return m_userData;
}

uint8_t* cResponsePacket::stealUserData()
{
  uint8_t* result = m_userData;
  m_userData = nullptr;
  return result;
}
