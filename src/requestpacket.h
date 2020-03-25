/*
 *  Copyright (C) 2010-2020 Team Kodi
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

class cRequestPacket
{
  public:
    cRequestPacket();
    ~cRequestPacket();

    void init(uint32_t opcode, bool stream = false, bool setUserDataLength = false, size_t userDataLength = 0);
    void add_String(const char* string);
    void add_U8(uint8_t c);
    void add_U32(uint32_t ul);
    void add_S32(int32_t l);
    void add_U64(uint64_t ull);
    void add_S64(int64_t ll);

    uint8_t* getPtr() const { return buffer; }
    size_t getLen() const { return bufUsed; }
    uint32_t getChannel() const { return channel; }
    uint32_t getSerial() const { return serialNumber; }

    uint32_t getOpcode() const { return opcode; }

  private:
    static uint32_t serialNumberCounter;

    uint8_t* buffer;
    size_t bufSize;
    size_t bufUsed;
    bool lengthSet;

    uint32_t channel;
    uint32_t serialNumber;

    uint32_t opcode;

    void checkExtend(size_t by);

    const static size_t headerLength = 16;
    const static size_t userDataLenPos = 12;
};
