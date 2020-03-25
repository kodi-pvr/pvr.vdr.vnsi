/*
 *  Copyright (C) 2010-2020 Team Kodi
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "requestpacket.h"
#include "vnsicommand.h"
#include "tools.h"
#include "p8-platform/sockets/tcp.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

uint32_t cRequestPacket::serialNumberCounter = 1;

cRequestPacket::cRequestPacket()
{
  buffer        = NULL;
  bufSize       = 0;
  bufUsed       = 0;
  lengthSet     = false;
  serialNumber  = 0;
  opcode        = 0;
}

cRequestPacket::~cRequestPacket()
{
  free(buffer);
}

void cRequestPacket::init(uint32_t topcode, bool stream, bool setUserDataLength, size_t userDataLength)
{
  assert(buffer == NULL);

  if (setUserDataLength)
  {
    bufSize = headerLength + userDataLength;
    lengthSet = true;
  }
  else
  {
    bufSize = 512;
    userDataLength = 0; // so the below will write a zero
  }

  buffer = (uint8_t*)malloc(bufSize);
  if (!buffer) throw std::bad_alloc();

  if (!stream)
    channel     = VNSI_CHANNEL_REQUEST_RESPONSE;
  else
    channel     = VNSI_CHANNEL_STREAM;
  serialNumber  = serialNumberCounter++;
  opcode        = topcode;

  uint32_t ul;
  ul = htonl(channel);
  memcpy(&buffer[0], &ul, sizeof(uint32_t));
  ul = htonl(serialNumber);
  memcpy(&buffer[4], &ul, sizeof(uint32_t));
  ul = htonl(opcode);
  memcpy(&buffer[8], &ul, sizeof(uint32_t));
  ul = htonl(userDataLength);
  memcpy(&buffer[userDataLenPos], &ul, sizeof(uint32_t));
  bufUsed = headerLength;
}

void cRequestPacket::add_String(const char* string)
{
  size_t len = strlen(string) + 1;
  checkExtend(len);
  memcpy(buffer + bufUsed, string, len);
  bufUsed += len;
  if (!lengthSet)
  {
    uint32_t tmp = htonl(bufUsed - headerLength);
    memcpy(&buffer[userDataLenPos], &tmp, sizeof(uint32_t));
  }
}

void cRequestPacket::add_U8(uint8_t c)
{
  checkExtend(sizeof(uint8_t));
  buffer[bufUsed] = c;
  bufUsed += sizeof(uint8_t);
  if (!lengthSet)
  {
    uint32_t tmp = htonl(bufUsed - headerLength);
    memcpy(&buffer[userDataLenPos], &tmp, sizeof(uint32_t));
  }
}

void cRequestPacket::add_S32(int32_t l)
{
  checkExtend(sizeof(int32_t));
  int32_t tmp = htonl(l);
  memcpy(&buffer[bufUsed], &tmp, sizeof(int32_t));
  bufUsed += sizeof(int32_t);
  if (!lengthSet)
  {
    uint32_t tmp = htonl(bufUsed - headerLength);
    memcpy(&buffer[userDataLenPos], &tmp, sizeof(uint32_t));
  }
}

void cRequestPacket::add_U32(uint32_t ul)
{
  checkExtend(sizeof(uint32_t));
  uint32_t tmp = htonl(ul);
  memcpy(&buffer[bufUsed], &tmp, sizeof(uint32_t));
  bufUsed += sizeof(uint32_t);
  if (!lengthSet)
  {
    uint32_t tmp = htonl(bufUsed - headerLength);
    memcpy(&buffer[userDataLenPos], &tmp, sizeof(uint32_t));
  }
}

void cRequestPacket::add_U64(uint64_t ull)
{
  checkExtend(sizeof(uint64_t));
  uint64_t tmp = htonll(ull);
  memcpy(&buffer[bufUsed], &tmp, sizeof(uint64_t));
  bufUsed += sizeof(uint64_t);
  if (!lengthSet)
  {
    uint32_t tmp = htonl(bufUsed - headerLength);
    memcpy(&buffer[userDataLenPos], &tmp, sizeof(uint32_t));
  }
}

void cRequestPacket::add_S64(int64_t ll)
{
  checkExtend(sizeof(int64_t));
  int64_t tmp = htonll(ll);
  memcpy(&buffer[bufUsed], &tmp, sizeof(int64_t));
  bufUsed += sizeof(int64_t);
  if (!lengthSet)
  {
    uint32_t tmp = htonl(bufUsed - headerLength);
    memcpy(&buffer[userDataLenPos], &tmp, sizeof(uint32_t));
  }
}

void cRequestPacket::checkExtend(size_t by)
{
  if (lengthSet) return;
  if ((bufUsed + by) <= bufSize) return;
  uint8_t* newBuf = (uint8_t*)realloc(buffer, bufUsed + by);
  if (!newBuf)
  {
    newBuf = (uint8_t*)malloc(bufUsed + by);
    if (!newBuf) {
      throw std::bad_alloc();
    }
    memcpy(newBuf, buffer, bufUsed);
    free(buffer);
  }
  buffer = newBuf;
  bufSize = bufUsed + by;
}

