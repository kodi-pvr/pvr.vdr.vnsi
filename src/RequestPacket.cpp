/*
 *  Copyright (C) 2010-2021 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "RequestPacket.h"

#include "TCPSocket.h"
#include "Tools.h"
#include "vnsicommand.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

uint32_t cRequestPacket::m_serialNumberCounter = 1;

cRequestPacket::~cRequestPacket()
{
  free(m_buffer);
}

void cRequestPacket::init(uint32_t topcode,
                          bool stream,
                          bool setUserDataLength,
                          size_t userDataLength)
{
  assert(m_buffer == nullptr);

  if (setUserDataLength)
  {
    m_bufSize = m_headerLength + userDataLength;
    lengthSet = true;
  }
  else
  {
    m_bufSize = 512;
    userDataLength = 0; // so the below will write a zero
  }

  m_buffer = static_cast<uint8_t*>(malloc(m_bufSize));
  if (!m_buffer)
    throw std::bad_alloc();

  if (!stream)
    m_channel = VNSI_CHANNEL_REQUEST_RESPONSE;
  else
    m_channel = VNSI_CHANNEL_STREAM;
  m_serialNumber = m_serialNumberCounter++;
  m_opcode = topcode;

  uint32_t ul;
  ul = htonl(m_channel);
  memcpy(&m_buffer[0], &ul, sizeof(uint32_t));
  ul = htonl(m_serialNumber);
  memcpy(&m_buffer[4], &ul, sizeof(uint32_t));
  ul = htonl(m_opcode);
  memcpy(&m_buffer[8], &ul, sizeof(uint32_t));
  ul = htonl(userDataLength);
  memcpy(&m_buffer[m_userDataLenPos], &ul, sizeof(uint32_t));
  m_bufUsed = m_headerLength;
}

void cRequestPacket::add_String(const char* string)
{
  size_t len = strlen(string) + 1;
  checkExtend(len);
  memcpy(m_buffer + m_bufUsed, string, len);
  m_bufUsed += len;
  if (!lengthSet)
  {
    uint32_t tmp = htonl(m_bufUsed - m_headerLength);
    memcpy(&m_buffer[m_userDataLenPos], &tmp, sizeof(uint32_t));
  }
}

void cRequestPacket::add_U8(uint8_t c)
{
  checkExtend(sizeof(uint8_t));
  m_buffer[m_bufUsed] = c;
  m_bufUsed += sizeof(uint8_t);
  if (!lengthSet)
  {
    uint32_t tmp = htonl(m_bufUsed - m_headerLength);
    memcpy(&m_buffer[m_userDataLenPos], &tmp, sizeof(uint32_t));
  }
}

void cRequestPacket::add_S32(int32_t l)
{
  checkExtend(sizeof(int32_t));
  int32_t tmp = htonl(l);
  memcpy(&m_buffer[m_bufUsed], &tmp, sizeof(int32_t));
  m_bufUsed += sizeof(int32_t);
  if (!lengthSet)
  {
    uint32_t tmp = htonl(m_bufUsed - m_headerLength);
    memcpy(&m_buffer[m_userDataLenPos], &tmp, sizeof(uint32_t));
  }
}

void cRequestPacket::add_U32(uint32_t ul)
{
  checkExtend(sizeof(uint32_t));
  uint32_t tmp = htonl(ul);
  memcpy(&m_buffer[m_bufUsed], &tmp, sizeof(uint32_t));
  m_bufUsed += sizeof(uint32_t);
  if (!lengthSet)
  {
    uint32_t tmp = htonl(m_bufUsed - m_headerLength);
    memcpy(&m_buffer[m_userDataLenPos], &tmp, sizeof(uint32_t));
  }
}

void cRequestPacket::add_U64(uint64_t ull)
{
  checkExtend(sizeof(uint64_t));
  uint64_t tmp = htonll(ull);
  memcpy(&m_buffer[m_bufUsed], &tmp, sizeof(uint64_t));
  m_bufUsed += sizeof(uint64_t);
  if (!lengthSet)
  {
    uint32_t tmp = htonl(m_bufUsed - m_headerLength);
    memcpy(&m_buffer[m_userDataLenPos], &tmp, sizeof(uint32_t));
  }
}

void cRequestPacket::add_S64(int64_t ll)
{
  checkExtend(sizeof(int64_t));
  int64_t tmp = htonll(ll);
  memcpy(&m_buffer[m_bufUsed], &tmp, sizeof(int64_t));
  m_bufUsed += sizeof(int64_t);
  if (!lengthSet)
  {
    uint32_t tmp = htonl(m_bufUsed - m_headerLength);
    memcpy(&m_buffer[m_userDataLenPos], &tmp, sizeof(uint32_t));
  }
}

void cRequestPacket::checkExtend(size_t by)
{
  if (lengthSet)
    return;
  if ((m_bufUsed + by) <= m_bufSize)
    return;
  uint8_t* newBuf = static_cast<uint8_t*>(realloc(m_buffer, m_bufUsed + by));
  if (!newBuf)
  {
    newBuf = static_cast<uint8_t*>(malloc(m_bufUsed + by));
    if (!newBuf)
    {
      throw std::bad_alloc();
    }
    memcpy(newBuf, m_buffer, m_bufUsed);
    free(m_buffer);
  }
  m_buffer = newBuf;
  m_bufSize = m_bufUsed + by;
}
