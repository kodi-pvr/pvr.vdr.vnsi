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

#include "responsepacket.h"
#include "vnsicommand.h"
#include "tools.h"
#include "p8-platform/sockets/tcp.h"

#include <stdexcept>
#include <stdlib.h>
#include <string.h>

cResponsePacket::cResponsePacket()
{
  userDataLength  = 0;
  packetPos       = 0;
  userData        = NULL;
  channelID       = 0;
  requestID       = 0;
  streamID        = 0;
}

cResponsePacket::~cResponsePacket()
{
  if (userData)
  {
    if (channelID == VNSI_CHANNEL_STREAM && opcodeID == VNSI_STREAM_MUXPKT)
      PVR->FreeDemuxPacket((DemuxPacket*)userData); 
    else
      free(userData);
  }
}

void cResponsePacket::getOSDData(uint32_t &wnd, uint32_t &color, uint32_t &x0, uint32_t &y0, uint32_t &x1, uint32_t &y1)
{
  wnd = osdWnd;
  color = osdColor;
  x0 = osdX0;
  y0 = osdY0;
  x1 = osdX1;
  y1 = osdY1;
}

void cResponsePacket::setResponse(uint8_t* tuserData, size_t tuserDataLength)
{
  channelID       = VNSI_CHANNEL_REQUEST_RESPONSE;
  userData        = tuserData;
  userDataLength  = tuserDataLength;
  packetPos       = 0;
}

void cResponsePacket::setStatus(uint8_t* tuserData, size_t tuserDataLength)
{
  channelID       = VNSI_CHANNEL_STATUS;
  userData        = tuserData;
  userDataLength  = tuserDataLength;
  packetPos       = 0;
}

void cResponsePacket::setStream(uint8_t* tuserData, size_t tuserDataLength)
{
  channelID       = VNSI_CHANNEL_STREAM;
  // set pointer to user data
  userData        = tuserData;
  userDataLength  = tuserDataLength;
  packetPos       = 0;
}

void cResponsePacket::setOSD(uint8_t* tuserData, size_t tuserDataLength)
{
  channelID       = VNSI_CHANNEL_OSD;
  // set pointer to user data
  userData        = tuserData;
  userDataLength  = tuserDataLength;
  packetPos       = 0;
}

void cResponsePacket::extractHeader()
{
  // set data pointers to header first
  userData = header;
  userDataLength = sizeof(header);
  packetPos = 0;

  requestID = extract_U32();
  userDataLength = extract_U32();

  userData = NULL;
}

void cResponsePacket::extractStreamHeader()
{
  channelID = VNSI_CHANNEL_STREAM;

  // set data pointers to header first
  userData = header;
  userDataLength = sizeof(header);
  packetPos = 0;

  opcodeID = extract_U32();
  streamID = extract_U32();
  duration = extract_U32();
  pts      = extract_U64();
  dts      = extract_U64();
  muxSerial= extract_U32();

  userDataLength = extract_U32();

  userData = NULL;
}

void cResponsePacket::extractOSDHeader()
{
  channelID = VNSI_CHANNEL_OSD;

  // set data pointers to header first
  userData = header;
  userDataLength = sizeof(header);
  packetPos = 0;

  opcodeID = extract_U32();
  osdWnd   = extract_S32();
  osdColor = extract_S32();
  osdX0    = extract_S32();
  osdY0    = extract_S32();
  osdX1    = extract_S32();
  osdY1    = extract_S32();
  userDataLength = extract_U32();
}

char* cResponsePacket::extract_String()
{
  char *p = (char *)&userData[packetPos];
  const char *end = (const char *)memchr(p, '\0', userDataLength - packetPos);
  if (end == NULL)
    /* string is not terminated - fail */
    throw std::out_of_range("Malformed VNSI packet");

  int length = end - p;
  packetPos += length + 1;
  return p;
}

uint8_t cResponsePacket::extract_U8()
{
  if ((packetPos + sizeof(uint8_t)) > userDataLength)
    throw std::out_of_range("Malformed VNSI packet");
  uint8_t uc = userData[packetPos];
  packetPos += sizeof(uint8_t);
  return uc;
}

uint32_t cResponsePacket::extract_U32()
{
  if ((packetPos + sizeof(uint32_t)) > userDataLength)
    throw std::out_of_range("Malformed VNSI packet");
  uint32_t ul;
  memcpy(&ul, &userData[packetPos], sizeof(uint32_t));
  ul = ntohl(ul);
  packetPos += sizeof(uint32_t);
  return ul;
}

uint64_t cResponsePacket::extract_U64()
{
  if ((packetPos + sizeof(uint64_t)) > userDataLength)
    throw std::out_of_range("Malformed VNSI packet");
  uint64_t ull;
  memcpy(&ull, &userData[packetPos], sizeof(uint64_t));
  ull = ntohll(ull);
  packetPos += sizeof(uint64_t);
  return ull;
}

double cResponsePacket::extract_Double()
{
  if ((packetPos + sizeof(uint64_t)) > userDataLength)
    throw std::out_of_range("Malformed VNSI packet");
  uint64_t ull;
  memcpy(&ull, &userData[packetPos], sizeof(uint64_t));
  ull = ntohll(ull);
  double d;
  memcpy(&d,&ull,sizeof(double));
  packetPos += sizeof(uint64_t);
  return d;
}

int32_t cResponsePacket::extract_S32()
{
  if ((packetPos + sizeof(int32_t)) > userDataLength)
    throw std::out_of_range("Malformed VNSI packet");
  int32_t l;
  memcpy(&l, &userData[packetPos], sizeof(int32_t));
  l = ntohl(l);
  packetPos += sizeof(int32_t);
  return l;
}

int64_t cResponsePacket::extract_S64()
{
  if ((packetPos + sizeof(int64_t)) > userDataLength)
    throw std::out_of_range("Malformed VNSI packet");
  int64_t ll;
  memcpy(&ll, &userData[packetPos], sizeof(int64_t));
  ll = ntohll(ll);
  packetPos += sizeof(int64_t);
  return ll;
}

uint8_t* cResponsePacket::getUserData()
{
  return userData;
}

uint8_t* cResponsePacket::stealUserData()
{
  uint8_t *result = userData;
  userData = NULL;
  return result;
}
