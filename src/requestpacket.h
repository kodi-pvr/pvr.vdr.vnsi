#pragma once
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
