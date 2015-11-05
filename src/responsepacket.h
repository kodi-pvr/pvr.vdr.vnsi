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

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

class cResponsePacket
{
  public:
    cResponsePacket();
    ~cResponsePacket();

    void setResponse(uint8_t* packet, size_t packetLength);
    void setStatus(uint8_t* packet, size_t packetLength);
    void setStream(uint8_t* packet, size_t packetLength);
    void setOSD(uint8_t* packet, size_t packetLength);

    void extractHeader();
    void extractStreamHeader();
    void extractOSDHeader();

    bool noResponse() { return (userData == NULL); };

    size_t    getUserDataLength() const { return userDataLength; }
    uint32_t  getChannelID() const { return channelID; }
    uint32_t  getRequestID() const { return requestID; }
    uint32_t  getStreamID() const { return streamID; }
    uint32_t  getOpCodeID() const { return opcodeID; }
    uint32_t  getDuration() const { return duration; }
    int64_t   getDTS() const { return dts; }
    int64_t   getPTS() const { return pts; }
    uint32_t  getMuxSerial() const { return muxSerial; }
    void      getOSDData(uint32_t &wnd, uint32_t &color, uint32_t &x0, uint32_t &y0, uint32_t &x1, uint32_t &y1);

    size_t    getPacketPos() const { return packetPos; }

    size_t getRemainingLength() const {
      return userDataLength - packetPos;
    }

    char*     extract_String();
    uint8_t   extract_U8();
    uint32_t  extract_U32();
    uint64_t  extract_U64();
    int32_t   extract_S32();
    int64_t   extract_S64();
    double    extract_Double();

    // If you call this, the memory becomes yours. Free with free()
    uint8_t* stealUserData();

    uint8_t* getUserData();

    uint8_t* getHeader() { return header; };
    size_t getStreamHeaderLength() const { return 36; };
    size_t getHeaderLength() const { return 8; };
    size_t getOSDHeaderLength() const { return 32; } ;

  private:
    uint8_t  header[40];
    uint8_t* userData;
    size_t   userDataLength;
    size_t   packetPos;

    uint32_t channelID;

    uint32_t requestID;
    uint32_t streamID;
    uint32_t opcodeID;
    uint32_t duration;
    int64_t  dts;
    int64_t  pts;
    uint32_t muxSerial;

    int32_t osdWnd;
    int32_t osdColor;
    int32_t osdX0,osdY0,osdX1,osdY1;
};
