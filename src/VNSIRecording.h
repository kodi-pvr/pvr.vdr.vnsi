/*
 *  Copyright (C) 2010-2020 Team Kodi
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "VNSISession.h"
#include "client.h"

class cVNSIRecording : public cVNSISession
{
public:

  cVNSIRecording();
  ~cVNSIRecording();

  bool OpenRecording(const PVR_RECORDING& recinfo);
  void Close() override;

  int Read(unsigned char* buf, uint32_t buf_size);
  long long Seek(long long pos, uint32_t whence);
  long long Length(void);
  bool GetStreamTimes(PVR_STREAM_TIMES *times);

protected:

  void OnReconnect() override;
  void GetLength();

private:

  PVR_RECORDING m_recinfo;
  uint64_t m_currentPlayingRecordBytes;
  uint64_t m_currentPlayingRecordLengthMSec;
  uint32_t m_currentPlayingRecordFrames;
  uint64_t m_currentPlayingRecordPosition;
};
