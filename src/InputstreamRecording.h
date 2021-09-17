/*
 *  Copyright (C) 2010-2021 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "Session.h"

#include <kodi/addon-instance/pvr/Stream.h>

class ATTRIBUTE_HIDDEN cVNSIRecording : public cVNSISession
{
public:
  cVNSIRecording(kodi::addon::CInstancePVRClient& instance);
  ~cVNSIRecording();

  bool OpenRecording(const kodi::addon::PVRRecording& recinfo);
  void Close() override;

  int Read(unsigned char* buf, uint32_t buf_size);
  long long Seek(long long pos, uint32_t whence);
  long long Length(void);
  bool GetStreamTimes(kodi::addon::PVRStreamTimes& times);

protected:
  void OnReconnect() override;
  void GetLength();

private:
  kodi::addon::PVRRecording m_recinfo;
  uint64_t m_currentPlayingRecordBytes = 0;
  uint64_t m_currentPlayingRecordLengthMSec = 0;
  uint32_t m_currentPlayingRecordFrames = 0;
  uint64_t m_currentPlayingRecordPosition = 0;

  kodi::addon::CInstancePVRClient& m_instance;
};
