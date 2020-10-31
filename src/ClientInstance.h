/*
 *  Copyright (C) 2010-2020 Team Kodi
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "Session.h"

#include <atomic>
#include <kodi/addon-instance/PVR.h>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>

class CPVRAddon;
class cResponsePacket;
class cRequestPacket;
class cVNSIDemux;
class cVNSIRecording;

class ATTRIBUTE_HIDDEN CVNSIClientInstance : public kodi::addon::CInstancePVRClient,
                                             public cVNSISession
{
public:
  CVNSIClientInstance(const CPVRAddon& base, KODI_HANDLE instance, const std::string& kodiVersion);
  ~CVNSIClientInstance() override;

  bool Start(const std::string& hostname,
             int port,
             const char* name = nullptr,
             const std::string& mac = "");

  PVR_ERROR GetCapabilities(kodi::addon::PVRCapabilities& capabilities) override;

  PVR_ERROR GetBackendName(std::string& name) override;
  PVR_ERROR GetBackendVersion(std::string& version) override;
  PVR_ERROR GetBackendHostname(std::string& hostname) override;
  PVR_ERROR GetConnectionString(std::string& connection) override;
  PVR_ERROR GetDriveSpace(uint64_t& total, uint64_t& used) override;
  PVR_ERROR CallSettingsMenuHook(const kodi::addon::PVRMenuhook& menuhook) override;
  PVR_ERROR OpenDialogChannelScan() override;

  //--==----==----==----==----==----==----==----==----==----==----==----==----==

  PVR_ERROR OnSystemSleep() override;
  PVR_ERROR OnSystemWake() override;
  PVR_ERROR OnPowerSavingActivated() override;
  PVR_ERROR OnPowerSavingDeactivated() override;

  //--==----==----==----==----==----==----==----==----==----==----==----==----==

  PVR_ERROR GetChannelsAmount(int& amount) override;
  PVR_ERROR GetChannels(bool radio, kodi::addon::PVRChannelsResultSet& results) override;
  PVR_ERROR GetSignalStatus(int channelUid, kodi::addon::PVRSignalStatus& signalStatus) override;

  //--==----==----==----==----==----==----==----==----==----==----==----==----==

  PVR_ERROR GetChannelGroupsAmount(int& amount) override;
  PVR_ERROR GetChannelGroups(bool radio, kodi::addon::PVRChannelGroupsResultSet& results) override;
  PVR_ERROR GetChannelGroupMembers(const kodi::addon::PVRChannelGroup& group,
                                   kodi::addon::PVRChannelGroupMembersResultSet& results) override;

  //--==----==----==----==----==----==----==----==----==----==----==----==----==

  PVR_ERROR GetEPGForChannel(int channelUid,
                             time_t start,
                             time_t end,
                             kodi::addon::PVREPGTagsResultSet& results) override;

  //--==----==----==----==----==----==----==----==----==----==----==----==----==

  PVR_ERROR GetTimerTypes(std::vector<kodi::addon::PVRTimerType>& types) override;
  PVR_ERROR GetTimersAmount(int& amount) override;
  PVR_ERROR GetTimers(kodi::addon::PVRTimersResultSet& results) override;
  PVR_ERROR AddTimer(const kodi::addon::PVRTimer& timer) override;
  PVR_ERROR DeleteTimer(const kodi::addon::PVRTimer& timer, bool forceDelete) override;
  PVR_ERROR UpdateTimer(const kodi::addon::PVRTimer& timer) override;

  //--==----==----==----==----==----==----==----==----==----==----==----==----==

  PVR_ERROR GetRecordingsAmount(bool deleted, int& amount) override;
  PVR_ERROR GetRecordings(bool deleted, kodi::addon::PVRRecordingsResultSet& results) override;
  PVR_ERROR DeleteRecording(const kodi::addon::PVRRecording& recording) override;
  PVR_ERROR UndeleteRecording(const kodi::addon::PVRRecording& recording) override;
  PVR_ERROR DeleteAllRecordingsFromTrash() override;
  PVR_ERROR RenameRecording(const kodi::addon::PVRRecording& recording) override;
  PVR_ERROR GetRecordingEdl(const kodi::addon::PVRRecording& recording,
                            std::vector<kodi::addon::PVREDLEntry>& edl) override;

  //--==----==----==----==----==----==----==----==----==----==----==----==----==

  bool OpenLiveStream(const kodi::addon::PVRChannel& channel) override;
  void CloseLiveStream() override;
  PVR_ERROR GetStreamProperties(std::vector<kodi::addon::PVRStreamProperties>& properties) override;
  DEMUX_PACKET* DemuxRead() override;
  void DemuxAbort() override;
  bool SeekTime(double time, bool backwards, double& startpts) override;
  bool CanPauseStream() override;
  bool CanSeekStream() override;
  bool IsRealTimeStream() override;
  PVR_ERROR GetStreamTimes(kodi::addon::PVRStreamTimes& times) override;

  //--==----==----==----==----==----==----==----==----==----==----==----==----==

  bool OpenRecordedStream(const kodi::addon::PVRRecording& recording) override;
  void CloseRecordedStream() override;
  int ReadRecordedStream(unsigned char* buffer, unsigned int size) override;
  int64_t SeekRecordedStream(int64_t position, int whence) override;
  int64_t LengthRecordedStream() override;
  PVR_ERROR GetStreamReadChunkSize(int& chunksize) override;

  //--==----==----==----==----==----==----==----==----==----==----==----==----==

protected:
  void Process();
  virtual bool OnResponsePacket(cResponsePacket* pkt);

  void OnDisconnect() override;
  void OnReconnect() override;

private:
  bool SupportChannelScan();
  bool SupportRecordingsUndelete();
  bool EnableStatusInterface(bool onOff, bool wait = true);
  PVR_ERROR GetAvailableRecordings(kodi::addon::PVRRecordingsResultSet& results);
  PVR_ERROR GetDeletedRecordings(kodi::addon::PVRRecordingsResultSet& results);
  bool GenTimerChildren(const kodi::addon::PVRTimer& timer,
                        kodi::addon::PVRTimersResultSet& results);
  std::string GenTimerFolder(std::string directory, std::string title);
  PVR_ERROR GetTimerInfo(unsigned int timernumber, kodi::addon::PVRTimer& tag);
  PVR_ERROR RenameTimer(const kodi::addon::PVRTimer& timerinfo, const std::string& newname);

  std::unique_ptr<cResponsePacket> ReadResult(cRequestPacket* vrp);

  struct SMessage
  {
    std::condition_variable_any m_condition;
    std::recursive_mutex m_messageMutex;
    std::unique_ptr<cResponsePacket> pkt;
  };

  class Queue
  {
    typedef std::map<int, SMessage> SMessages;
    SMessages m_queue;

  public:
    std::recursive_mutex m_mutex;
    SMessage& Enqueue(uint32_t serial);
    std::unique_ptr<cResponsePacket> Dequeue(uint32_t serial, SMessage& message);
    void Set(std::unique_ptr<cResponsePacket>&& vresp);
  };

  Queue m_queue;

  std::string m_videodir;
  std::string m_wolMac;

  bool m_isTimeshift = false;
  bool m_isRealtime = false;
  int64_t m_ptsBufferEnd = 0;
  std::recursive_mutex m_timeshiftMutex;

  cVNSIDemux* m_demuxer = nullptr;
  cVNSIRecording* m_recording = nullptr;

  const CPVRAddon& m_base;

  std::atomic<bool> m_running = {false};
  std::thread m_thread;
};
