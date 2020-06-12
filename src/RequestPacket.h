/*
 *  Copyright (C) 2010-2020 Team Kodi
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <kodi/AddonBase.h>
#include <stddef.h>
#include <stdint.h>

class ATTRIBUTE_HIDDEN cRequestPacket
{
public:
  cRequestPacket() = default;
  ~cRequestPacket();

  void init(uint32_t opcode,
            bool stream = false,
            bool setUserDataLength = false,
            size_t userDataLength = 0);
  void add_String(const char* string);
  void add_U8(uint8_t c);
  void add_U32(uint32_t ul);
  void add_S32(int32_t l);
  void add_U64(uint64_t ull);
  void add_S64(int64_t ll);

  uint8_t* getPtr() const { return m_buffer; }
  size_t getLen() const { return m_bufUsed; }
  uint32_t getChannel() const { return m_channel; }
  uint32_t getSerial() const { return m_serialNumber; }
  uint32_t getOpcode() const { return m_opcode; }

private:
  static uint32_t m_serialNumberCounter;

  uint8_t* m_buffer = nullptr;
  size_t m_bufSize = 0;
  size_t m_bufUsed = 0;
  bool lengthSet = false;

  uint32_t m_channel = 0;
  uint32_t m_serialNumber = 0;

  uint32_t m_opcode = 0;

  void checkExtend(size_t by);

  const static size_t m_headerLength = 16;
  const static size_t m_userDataLenPos = 12;
};
