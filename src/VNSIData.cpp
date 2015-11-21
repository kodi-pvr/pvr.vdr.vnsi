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

#include "VNSIData.h"
#include "responsepacket.h"
#include "requestpacket.h"
#include "vnsicommand.h"
#include "platform/util/StdString.h"

using namespace ADDON;
using namespace PLATFORM;

cVNSIData::cVNSIData()
{
}

cVNSIData::~cVNSIData()
{
  StopThread();
  Close();
}

bool cVNSIData::Open(const std::string& hostname, int port, const char* name, const std::string& mac)
{
  /* First wake up the VDR server in case a MAC-Address is specified */
  if (!mac.empty()) {
    const char* temp_mac;
    temp_mac = mac.c_str();

    if (!XBMC->WakeOnLan(temp_mac)) {
      XBMC->Log(LOG_ERROR, "Error waking up VNSI Server at MAC-Address %s", temp_mac);
      return false;
    }
  }

  if(!cVNSISession::Open(hostname, port, name))
    return false;

  return true;
}

bool cVNSIData::Login()
{
  if(!cVNSISession::Login())
    return false;

  CreateThread();
  return true;
}

void cVNSIData::OnDisconnect()
{
  XBMC->QueueNotification(QUEUE_ERROR, XBMC->GetLocalizedString(30044));
  PVR->TriggerTimerUpdate();
}

void cVNSIData::OnReconnect()
{
  XBMC->QueueNotification(QUEUE_INFO, XBMC->GetLocalizedString(30045));

  EnableStatusInterface(g_bHandleMessages);

  PVR->TriggerChannelUpdate();
  PVR->TriggerTimerUpdate();
  PVR->TriggerRecordingUpdate();
}

std::unique_ptr<cResponsePacket> cVNSIData::ReadResult(cRequestPacket* vrp)
{
  m_mutex.Lock();

  SMessage &message(m_queue[vrp->getSerial()]);

  m_mutex.Unlock();

  if(!cVNSISession::TransmitMessage(vrp))
  {
    CLockObject lock(m_mutex);
    m_queue.erase(vrp->getSerial());
    return NULL;
  }

  if (!message.event.Wait(g_iConnectTimeout * 1000))
  {
    XBMC->Log(LOG_ERROR, "%s - request timed out after %d seconds", __FUNCTION__, g_iConnectTimeout);
  }

  m_mutex.Lock();

  auto vresp = std::move(message.pkt);

  m_queue.erase(vrp->getSerial());

  m_mutex.Unlock();

  return vresp;
}

bool cVNSIData::GetDriveSpace(long long *total, long long *used)
{
  cRequestPacket vrp;
  vrp.init(VNSI_RECORDINGS_DISKSIZE);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return false;
  }

  uint32_t totalspace    = vresp->extract_U32();
  uint32_t freespace     = vresp->extract_U32();
  /* vresp->extract_U32(); percent not used */

  *total = totalspace;
  *used  = (totalspace - freespace);

  /* Convert from kBytes to Bytes */
  *total *= 1024;
  *used  *= 1024;

  return true;
}

bool cVNSIData::SupportChannelScan()
{
  cRequestPacket vrp;
  vrp.init(VNSI_SCAN_SUPPORTED);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return false;
  }

  uint32_t ret = vresp->extract_U32();
  return ret == VNSI_RET_OK ? true : false;
}

bool cVNSIData::SupportRecordingsUndelete()
{
  if (GetProtocol() > 7)
  {
    cRequestPacket vrp;
    vrp.init(VNSI_RECORDINGS_DELETED_ACCESS_SUPPORTED);

    auto vresp = ReadResult(&vrp);
    if (!vresp)
    {
      XBMC->Log(LOG_INFO, "%s - Can't get response packed", __FUNCTION__);
      return false;
    }

    uint32_t ret = vresp->extract_U32();
    return ret == VNSI_RET_OK ? true : false;
  }

  XBMC->Log(LOG_INFO, "%s - Undelete not supported on backend (min. Ver. 1.3.0; Protocol 7)", __FUNCTION__);
  return false;
}

bool cVNSIData::EnableStatusInterface(bool onOff)
{
  cRequestPacket vrp;
  vrp.init(VNSI_ENABLESTATUSINTERFACE);
  vrp.add_U8(onOff);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return false;
  }

  uint32_t ret = vresp->extract_U32();
  return ret == VNSI_RET_OK ? true : false;
}

int cVNSIData::GetChannelsCount()
{
  cRequestPacket vrp;
  vrp.init(VNSI_CHANNELS_GETCOUNT);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return -1;
  }

  uint32_t count = vresp->extract_U32();
  return count;
}

bool cVNSIData::GetChannelsList(ADDON_HANDLE handle, bool radio)
{
  cRequestPacket vrp;
  vrp.init(VNSI_CHANNELS_GETCHANNELS);
  vrp.add_U32(radio);
  vrp.add_U8(1); // apply filter

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return false;
  }

  while (vresp->getRemainingLength() >= 3 * 4 + 3)
  {
    PVR_CHANNEL tag;
    memset(&tag, 0 , sizeof(tag));

    tag.iChannelNumber    = vresp->extract_U32();
    char *strChannelName  = vresp->extract_String();
    strncpy(tag.strChannelName, strChannelName, sizeof(tag.strChannelName) - 1);
    char *strProviderName = vresp->extract_String();
    tag.iUniqueId         = vresp->extract_U32();
    tag.iEncryptionSystem = vresp->extract_U32();
    char *strCaids        = vresp->extract_String();
    if (m_protocol >= 6)
    {
      std::string path = g_szIconPath;
      std::string ref = vresp->extract_String();
      if (!path.empty())
      {
        if (path[path.length()-1] != '/')
          path += '/';
        path += ref;
        path += ".png";
        strncpy(tag.strIconPath, path.c_str(), sizeof(tag.strIconPath) - 1);
      }
    }
    tag.bIsRadio          = radio;

    PVR->TransferChannelEntry(handle, &tag);
  }

  return true;
}

bool cVNSIData::GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t start, time_t end)
{
  cRequestPacket vrp;
  vrp.init(VNSI_EPG_GETFORCHANNEL);
  vrp.add_U32(channel.iUniqueId);
  vrp.add_U32(start);
  vrp.add_U32(end - start);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return false;
  }

  while (vresp->getRemainingLength() >= 5 * 4 + 3)
  {
    EPG_TAG tag;
    memset(&tag, 0 , sizeof(tag));

    tag.iChannelNumber      = channel.iChannelNumber;
    tag.iUniqueBroadcastId  = vresp->extract_U32();
    tag.startTime           = vresp->extract_U32();
    tag.endTime             = tag.startTime + vresp->extract_U32();
    uint32_t content        = vresp->extract_U32();
    tag.iGenreType          = content & 0xF0;
    tag.iGenreSubType       = content & 0x0F;
    tag.strGenreDescription = "";
    tag.iParentalRating     = vresp->extract_U32();
    tag.strTitle            = vresp->extract_String();
    tag.strPlotOutline      = vresp->extract_String();
    tag.strPlot             = vresp->extract_String();
    tag.strOriginalTitle    = "";
    tag.strCast             = "";
    tag.strDirector         = "";
    tag.strWriter           = "";
    tag.iYear               = 0;
    tag.strIMDBNumber       = "";
    if (tag.strPlotOutline)
      tag.strEpisodeName    = strdup(tag.strPlotOutline);
    tag.iFlags              = EPG_TAG_FLAG_UNDEFINED;

    PVR->TransferEpgEntry(handle, &tag);
    free((void*)tag.strEpisodeName);
  }

  return true;
}


/** OPCODE's 60 - 69: VNSI network functions for timer access */

int cVNSIData::GetTimersCount()
{
  cRequestPacket vrp;
  vrp.init(VNSI_TIMER_GETCOUNT);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return -1;
  }

  uint32_t count = vresp->extract_U32();

  return count;
}

PVR_ERROR cVNSIData::GetTimerInfo(unsigned int timernumber, PVR_TIMER &tag)
{
  cRequestPacket vrp;
  memset(&tag, 0, sizeof(tag));
  vrp.init(VNSI_TIMER_GET);
  vrp.add_U32(timernumber);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return PVR_ERROR_UNKNOWN;
  }

  uint32_t returnCode = vresp->extract_U32();
  if (returnCode != VNSI_RET_OK)
  {
    if (returnCode == VNSI_RET_DATAUNKNOWN)
      return PVR_ERROR_FAILED;
    else if (returnCode == VNSI_RET_ERROR)
      return PVR_ERROR_SERVER_ERROR;
  }

  /* TODO: Implement own timer types to get support for the timer features introduced with PVR API 1.9.7 */
  tag.iTimerType = PVR_TIMER_TYPE_NONE;

  tag.iClientIndex      = vresp->extract_U32();
  int iActive           = vresp->extract_U32();
  int iRecording        = vresp->extract_U32();
  int iPending          = vresp->extract_U32();
  if (iRecording)
    tag.state = PVR_TIMER_STATE_RECORDING;
  else if (iPending || iActive)
    tag.state = PVR_TIMER_STATE_SCHEDULED;
  else
    tag.state = PVR_TIMER_STATE_CANCELLED;
  tag.iPriority         = vresp->extract_U32();
  tag.iLifetime         = vresp->extract_U32();
                          vresp->extract_U32(); // channel number - unused
  tag.iClientChannelUid = vresp->extract_U32();
  tag.startTime         = vresp->extract_U32();
  tag.endTime           = vresp->extract_U32();
  tag.firstDay          = vresp->extract_U32();
  tag.iWeekdays         = vresp->extract_U32();
  char *strTitle = vresp->extract_String();
  strncpy(tag.strTitle, strTitle, sizeof(tag.strTitle) - 1);

  return PVR_ERROR_NO_ERROR;
}

bool cVNSIData::GetTimersList(ADDON_HANDLE handle)
{
  cRequestPacket vrp;
  vrp.init(VNSI_TIMER_GETLIST);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return false;
  }

  uint32_t numTimers = vresp->extract_U32();
  if (numTimers > 0)
  {
    while (vresp->getRemainingLength() >= 12 * 4 + 1)
    {
      PVR_TIMER tag;
      memset(&tag, 0, sizeof(tag));

      /* TODO: Implement own timer types to get support for the timer features introduced with PVR API 1.9.7 */
      tag.iTimerType = PVR_TIMER_TYPE_NONE;

      tag.iClientIndex      = vresp->extract_U32();
      int iActive           = vresp->extract_U32();
      int iRecording        = vresp->extract_U32();
      int iPending          = vresp->extract_U32();
      if (iRecording)
        tag.state = PVR_TIMER_STATE_RECORDING;
      else if (iPending || iActive)
        tag.state = PVR_TIMER_STATE_SCHEDULED;
      else
        tag.state = PVR_TIMER_STATE_CANCELLED;
      tag.iPriority         = vresp->extract_U32();
      tag.iLifetime         = vresp->extract_U32();
                              vresp->extract_U32(); // channel number - unused
      tag.iClientChannelUid = vresp->extract_U32();
      tag.startTime         = vresp->extract_U32();
      tag.endTime           = vresp->extract_U32();
      tag.firstDay          = vresp->extract_U32();
      tag.iWeekdays         = vresp->extract_U32();
      char *strTitle = vresp->extract_String();
      strncpy(tag.strTitle, strTitle, sizeof(tag.strTitle) - 1);
      tag.iMarginStart      = 0;
      tag.iMarginEnd        = 0;

      PVR->TransferTimerEntry(handle, &tag);
    }
  }
  return true;
}

PVR_ERROR cVNSIData::AddTimer(const PVR_TIMER &timerinfo)
{
  cRequestPacket vrp;
  vrp.init(VNSI_TIMER_ADD);

  // add directory in front of the title
  std::string path;
  if(strlen(timerinfo.strDirectory) > 0) {
    path += timerinfo.strDirectory;
    if(path == "/") {
      path.clear();
    }
    else if(path.size() > 1) {
      if(path[0] == '/') {
        path = path.substr(1);
      }
    }

    if(path.size() > 0 && path[path.size()-1] != '/') {
      path += "/";
    }
  }

  // replace directory separators
  for(std::size_t i=0; i<path.size(); i++) {
    if(path[i] == '/' || path[i] == '\\') {
      path[i] = '~';
    }
  }

  if(strlen(timerinfo.strTitle) > 0) {
    path += timerinfo.strTitle;
  }

  if(path.empty()) {
    XBMC->Log(LOG_ERROR, "%s - Empty filename !", __FUNCTION__);
    return PVR_ERROR_UNKNOWN;
  }

  // use timer margin to calculate start/end times
  uint32_t starttime = timerinfo.startTime - timerinfo.iMarginStart*60;
  uint32_t endtime = timerinfo.endTime + timerinfo.iMarginEnd*60;

  vrp.add_U32(timerinfo.state == PVR_TIMER_STATE_SCHEDULED);
  vrp.add_U32(timerinfo.iPriority);
  vrp.add_U32(timerinfo.iLifetime);
  vrp.add_U32(timerinfo.iClientChannelUid);
  vrp.add_U32(starttime);
  vrp.add_U32(endtime);
  vrp.add_U32(timerinfo.iWeekdays != PVR_WEEKDAY_NONE ? timerinfo.firstDay : 0);
  vrp.add_U32(timerinfo.iWeekdays);
  vrp.add_String(path.c_str());
  vrp.add_String("");

  auto vresp = ReadResult(&vrp);
  if (vresp == NULL || vresp->noResponse())
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return PVR_ERROR_UNKNOWN;
  }
  uint32_t returnCode = vresp->extract_U32();
  if (returnCode == VNSI_RET_DATALOCKED)
    return PVR_ERROR_ALREADY_PRESENT;
  else if (returnCode == VNSI_RET_DATAINVALID)
    return PVR_ERROR_INVALID_PARAMETERS;
  else if (returnCode == VNSI_RET_ERROR)
    return PVR_ERROR_SERVER_ERROR;

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cVNSIData::DeleteTimer(const PVR_TIMER &timerinfo, bool force)
{
  cRequestPacket vrp;
  vrp.init(VNSI_TIMER_DELETE);
  vrp.add_U32(timerinfo.iClientIndex);
  vrp.add_U32(force);

  auto vresp = ReadResult(&vrp);
  if (vresp == NULL || vresp->noResponse())
  {
    return PVR_ERROR_UNKNOWN;
  }
  uint32_t returnCode = vresp->extract_U32();

  if (returnCode == VNSI_RET_DATALOCKED)
    return PVR_ERROR_FAILED;
  else if (returnCode == VNSI_RET_RECRUNNING)
    return PVR_ERROR_RECORDING_RUNNING;
  else if (returnCode == VNSI_RET_DATAINVALID)
    return PVR_ERROR_INVALID_PARAMETERS;
  else if (returnCode == VNSI_RET_ERROR)
    return PVR_ERROR_SERVER_ERROR;
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cVNSIData::RenameTimer(const PVR_TIMER &timerinfo, const char *newname)
{
  PVR_TIMER timerinfo1;
  memset(&timerinfo1, 0, sizeof(timerinfo1));
  PVR_ERROR ret = GetTimerInfo(timerinfo.iClientIndex, timerinfo1);
  if (ret != PVR_ERROR_NO_ERROR)
    return ret;

  strncpy(timerinfo1.strTitle, newname, sizeof(timerinfo1.strTitle) - 1);
  return UpdateTimer(timerinfo1);
}

PVR_ERROR cVNSIData::UpdateTimer(const PVR_TIMER &timerinfo)
{
  // use timer margin to calculate start/end times
  uint32_t starttime = timerinfo.startTime - timerinfo.iMarginStart*60;
  uint32_t endtime = timerinfo.endTime + timerinfo.iMarginEnd*60;

  cRequestPacket vrp;
  vrp.init(VNSI_TIMER_UPDATE);
  vrp.add_U32(timerinfo.iClientIndex);
  vrp.add_U32(timerinfo.state == PVR_TIMER_STATE_SCHEDULED);
  vrp.add_U32(timerinfo.iPriority);
  vrp.add_U32(timerinfo.iLifetime);
  vrp.add_U32(timerinfo.iClientChannelUid);
  vrp.add_U32(starttime);
  vrp.add_U32(endtime);
  vrp.add_U32(timerinfo.iWeekdays != PVR_WEEKDAY_NONE ? timerinfo.firstDay : 0);
  vrp.add_U32(timerinfo.iWeekdays);
  vrp.add_String(timerinfo.strTitle);
  vrp.add_String("");

  auto vresp = ReadResult(&vrp);
  if (vresp == NULL || vresp->noResponse())
  {
    return PVR_ERROR_UNKNOWN;
  }
  uint32_t returnCode = vresp->extract_U32();
  if (returnCode == VNSI_RET_DATAUNKNOWN)
    return PVR_ERROR_FAILED;
  else if (returnCode == VNSI_RET_DATAINVALID)
    return PVR_ERROR_INVALID_PARAMETERS;
  else if (returnCode == VNSI_RET_ERROR)
    return PVR_ERROR_SERVER_ERROR;

  return PVR_ERROR_NO_ERROR;
}

int cVNSIData::GetRecordingsCount()
{
  cRequestPacket vrp;
  vrp.init(VNSI_RECORDINGS_GETCOUNT);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return -1;
  }

  uint32_t count = vresp->extract_U32();
  return count;
}

PVR_ERROR cVNSIData::GetRecordingsList(ADDON_HANDLE handle)
{
  cRequestPacket vrp;
  vrp.init(VNSI_RECORDINGS_GETLIST);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return PVR_ERROR_UNKNOWN;
  }

  CStdString strRecordingId;
  while (vresp->getRemainingLength() >= 5 * 4 + 5)
  {
    PVR_RECORDING tag;
    memset(&tag, 0, sizeof(tag));
    tag.recordingTime   = vresp->extract_U32();
    tag.iDuration       = vresp->extract_U32();
    tag.iPriority       = vresp->extract_U32();
    tag.iLifetime       = vresp->extract_U32();
    tag.bIsDeleted      = false;

    char *strChannelName = vresp->extract_String();
    strncpy(tag.strChannelName, strChannelName, sizeof(tag.strChannelName) - 1);

    char *strTitle = vresp->extract_String();
    strncpy(tag.strTitle, strTitle, sizeof(tag.strTitle) - 1);

    char *strPlotOutline = vresp->extract_String();
    strncpy(tag.strPlotOutline, strPlotOutline, sizeof(tag.strPlotOutline) - 1);

    char *strPlot = vresp->extract_String();
    strncpy(tag.strPlot, strPlot, sizeof(tag.strPlot) - 1);

    char *strDirectory = vresp->extract_String();
    strncpy(tag.strDirectory, strDirectory, sizeof(tag.strDirectory) - 1);

    strRecordingId.Format("%i", vresp->extract_U32());
    strncpy(tag.strRecordingId, strRecordingId.c_str(), sizeof(tag.strRecordingId) - 1);

    PVR->TransferRecordingEntry(handle, &tag);
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cVNSIData::RenameRecording(const PVR_RECORDING& recinfo, const char* newname)
{
  cRequestPacket vrp;
  vrp.init(VNSI_RECORDINGS_RENAME);

  // add uid
  XBMC->Log(LOG_DEBUG, "%s - uid: %s", __FUNCTION__, recinfo.strRecordingId);
  vrp.add_U32(atoi(recinfo.strRecordingId));

  // add new title
  vrp.add_String(newname);

  auto vresp = ReadResult(&vrp);
  if (vresp == NULL || vresp->noResponse())
  {
    return PVR_ERROR_SERVER_ERROR;
  }

  uint32_t returnCode = vresp->extract_U32();

  if(returnCode != 0)
   return PVR_ERROR_FAILED;

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cVNSIData::DeleteRecording(const PVR_RECORDING& recinfo)
{
  cRequestPacket vrp;
  vrp.init(recinfo.bIsDeleted ? VNSI_RECORDINGS_DELETED_DELETE : VNSI_RECORDINGS_DELETE);
  vrp.add_U32(atoi(recinfo.strRecordingId));

  auto vresp = ReadResult(&vrp);
  if (vresp == NULL || vresp->noResponse())
  {
    return PVR_ERROR_UNKNOWN;
  }

  uint32_t returnCode = vresp->extract_U32();

  switch(returnCode)
  {
    case VNSI_RET_DATALOCKED:
      return PVR_ERROR_FAILED;

    case VNSI_RET_RECRUNNING:
      return PVR_ERROR_RECORDING_RUNNING;

    case VNSI_RET_DATAINVALID:
      return PVR_ERROR_INVALID_PARAMETERS;

    case VNSI_RET_ERROR:
      return PVR_ERROR_SERVER_ERROR;
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cVNSIData::GetRecordingEdl(const PVR_RECORDING& recinfo, PVR_EDL_ENTRY edl[], int *size)
{
  cRequestPacket vrp;
  vrp.init(VNSI_RECORDINGS_GETEDL);
  vrp.add_U32(atoi(recinfo.strRecordingId));

  auto vresp = ReadResult(&vrp);
  if (vresp == NULL || vresp->noResponse())
  {
    return PVR_ERROR_UNKNOWN;
  }

  *size = 0;
  while (vresp->getRemainingLength() >= 2 * 8 + 4 &&
         *size < PVR_ADDON_EDL_LENGTH)
  {
    edl[*size].start = vresp->extract_S64();
    edl[*size].end = vresp->extract_S64();
    edl[*size].type = (PVR_EDL_TYPE)vresp->extract_S32();
    (*size)++;
  }

  return PVR_ERROR_NO_ERROR;
}

int cVNSIData::GetDeletedRecordingsCount()
{
  cRequestPacket vrp;
  vrp.init(VNSI_RECORDINGS_DELETED_GETCOUNT);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return -1;
  }

  uint32_t count = vresp->extract_U32();
  return count;
}

PVR_ERROR cVNSIData::GetDeletedRecordingsList(ADDON_HANDLE handle)
{
  cRequestPacket vrp;
  vrp.init(VNSI_RECORDINGS_DELETED_GETLIST);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return PVR_ERROR_UNKNOWN;
  }

  CStdString strRecordingId;
  while (vresp->getRemainingLength() >= 5 * 4 + 5)
  {
    PVR_RECORDING tag;
    memset(&tag, 0, sizeof(tag));
    tag.recordingTime   = vresp->extract_U32();
    tag.iDuration       = vresp->extract_U32();
    tag.iPriority       = vresp->extract_U32();
    tag.iLifetime       = vresp->extract_U32();
    tag.bIsDeleted      = true;

    char *strChannelName = vresp->extract_String();
    strncpy(tag.strChannelName, strChannelName, sizeof(tag.strChannelName) - 1);

    char *strTitle = vresp->extract_String();
    strncpy(tag.strTitle, strTitle, sizeof(tag.strTitle) - 1);

    char *strPlotOutline = vresp->extract_String();
    strncpy(tag.strPlotOutline, strPlotOutline, sizeof(tag.strPlotOutline) - 1);

    char *strPlot = vresp->extract_String();
    strncpy(tag.strPlot, strPlot, sizeof(tag.strPlot) - 1);

    char *strDirectory = vresp->extract_String();
    strncpy(tag.strDirectory, strDirectory, sizeof(tag.strDirectory) - 1);

    strRecordingId.Format("%i", vresp->extract_U32());
    strncpy(tag.strRecordingId, strRecordingId.c_str(), sizeof(tag.strRecordingId) - 1);

    PVR->TransferRecordingEntry(handle, &tag);
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cVNSIData::UndeleteRecording(const PVR_RECORDING& recinfo)
{
  cRequestPacket vrp;
  vrp.init(VNSI_RECORDINGS_DELETED_UNDELETE);
  vrp.add_U32(atoi(recinfo.strRecordingId));

  auto vresp = ReadResult(&vrp);
  if (vresp == NULL || vresp->noResponse())
  {
    return PVR_ERROR_UNKNOWN;
  }

  uint32_t returnCode = vresp->extract_U32();
  switch(returnCode)
  {
    case VNSI_RET_DATALOCKED:
      return PVR_ERROR_FAILED;

    case VNSI_RET_RECRUNNING:
      return PVR_ERROR_RECORDING_RUNNING;

    case VNSI_RET_DATAINVALID:
      return PVR_ERROR_INVALID_PARAMETERS;

    case VNSI_RET_ERROR:
      return PVR_ERROR_SERVER_ERROR;
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cVNSIData::DeleteAllRecordingsFromTrash()
{
  cRequestPacket vrp;
  vrp.init(VNSI_RECORDINGS_DELETED_DELETE_ALL);

  auto vresp = ReadResult(&vrp);
  if (vresp == NULL || vresp->noResponse())
  {
    return PVR_ERROR_UNKNOWN;
  }

  uint32_t returnCode = vresp->extract_U32();
  switch(returnCode)
  {
    case VNSI_RET_DATALOCKED:
      return PVR_ERROR_FAILED;

    case VNSI_RET_RECRUNNING:
      return PVR_ERROR_RECORDING_RUNNING;

    case VNSI_RET_DATAINVALID:
      return PVR_ERROR_INVALID_PARAMETERS;

    case VNSI_RET_ERROR:
      return PVR_ERROR_SERVER_ERROR;
  }

  return PVR_ERROR_NO_ERROR;
}

bool cVNSIData::OnResponsePacket(cResponsePacket* pkt)
{
  return false;
}

void *cVNSIData::Process()
{
  std::unique_ptr<cResponsePacket> vresp;

  while (!IsStopped())
  {
    // try to reconnect
    if(ConnectionLost() && !TryReconnect())
    {
      Sleep(1000);
      continue;
    }

    // if there's anything in the buffer, read it
    if ((vresp = cVNSISession::ReadMessage(5)) == NULL)
    {
      Sleep(5);
      continue;
    }

    // CHANNEL_REQUEST_RESPONSE
    if (vresp->getChannelID() == VNSI_CHANNEL_REQUEST_RESPONSE)
    {
      CLockObject lock(m_mutex);
      SMessages::iterator it = m_queue.find(vresp->getRequestID());
      if (it != m_queue.end())
      {
        it->second.pkt = std::move(vresp);
        it->second.event.Broadcast();
      }
    }

    // CHANNEL_STATUS

    else if (vresp->getChannelID() == VNSI_CHANNEL_STATUS)
    {
      if (vresp->getRequestID() == VNSI_STATUS_MESSAGE)
      {
        uint32_t type = vresp->extract_U32();
        char* msgstr  = vresp->extract_String();
        char* strMessageTranslated(NULL);

        if (g_bCharsetConv)
          strMessageTranslated = XBMC->UnknownToUTF8(msgstr);
        else
          strMessageTranslated = msgstr;

        if (type == 2)
          XBMC->QueueNotification(QUEUE_ERROR, strMessageTranslated);
        if (type == 1)
          XBMC->QueueNotification(QUEUE_WARNING, strMessageTranslated);
        else
          XBMC->QueueNotification(QUEUE_INFO, strMessageTranslated);

        if (g_bCharsetConv)
          XBMC->FreeString(strMessageTranslated);
      }
      else if (vresp->getRequestID() == VNSI_STATUS_RECORDING)
      {
                          vresp->extract_U32(); // device currently unused
                          vresp->extract_U32(); // on (not used)
        char* str1      = vresp->extract_String();
        char* str2      = vresp->extract_String();

//        PVR->Recording(str1, str2, on!=0?true:false);
        PVR->TriggerTimerUpdate();
      }
      else if (vresp->getRequestID() == VNSI_STATUS_TIMERCHANGE)
      {
        XBMC->Log(LOG_DEBUG, "Server requested timer update");
        PVR->TriggerTimerUpdate();
      }
      else if (vresp->getRequestID() == VNSI_STATUS_CHANNELCHANGE)
      {
        XBMC->Log(LOG_DEBUG, "Server requested channel update");
        PVR->TriggerChannelUpdate();
      }
      else if (vresp->getRequestID() == VNSI_STATUS_RECORDINGSCHANGE)
      {
        XBMC->Log(LOG_DEBUG, "Server requested recordings update");
        PVR->TriggerRecordingUpdate();
      }
      else if (vresp->getRequestID() == VNSI_STATUS_EPGCHANGE)
      {
        uint32_t channel     = vresp->extract_U32();
        XBMC->Log(LOG_DEBUG, "Server requested Epg update for channel: %d", channel);
        PVR->TriggerEpgUpdate(channel);
      }
    }

    // UNKOWN CHANNELID

    else if (!OnResponsePacket(vresp.get()))
    {
      XBMC->Log(LOG_ERROR, "%s - Rxd a response packet on channel %lu !!", __FUNCTION__, vresp->getChannelID());
    }
  }
  return NULL;
}

int cVNSIData::GetChannelGroupCount(bool automatic)
{
  cRequestPacket vrp;
  vrp.init(VNSI_CHANNELGROUP_GETCOUNT);
  vrp.add_U32(automatic);

  auto vresp = ReadResult(&vrp);
  if (vresp == NULL || vresp->noResponse())
  {
    return 0;
  }

  uint32_t count = vresp->extract_U32();

  return count;
}

bool cVNSIData::GetChannelGroupList(ADDON_HANDLE handle, bool bRadio)
{
  cRequestPacket vrp;
  vrp.init(VNSI_CHANNELGROUP_LIST);
  vrp.add_U8(bRadio);

  auto vresp = ReadResult(&vrp);
  if (vresp == NULL || vresp->noResponse())
  {
    return false;
  }

  while (vresp->getRemainingLength() >= 1 + 1)
  {
    PVR_CHANNEL_GROUP tag;
    memset(&tag, 0, sizeof(tag));

    char *strGroupName = vresp->extract_String();
    strncpy(tag.strGroupName, strGroupName, sizeof(tag.strGroupName) - 1);
    tag.bIsRadio = vresp->extract_U8()!=0?true:false;
    tag.iPosition = 0;

    PVR->TransferChannelGroup(handle, &tag);
  }

  return true;
}

bool cVNSIData::GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
  cRequestPacket vrp;
  vrp.init(VNSI_CHANNELGROUP_MEMBERS);
  vrp.add_String(group.strGroupName);
  vrp.add_U8(group.bIsRadio);
  vrp.add_U8(1); // filter channels

  auto vresp = ReadResult(&vrp);
  if (vresp == NULL || vresp->noResponse())
  {
    return false;
  }

  while (vresp->getRemainingLength() >= 2 * 4)
  {
    PVR_CHANNEL_GROUP_MEMBER tag;
    memset(&tag, 0, sizeof(tag));

    strncpy(tag.strGroupName, group.strGroupName, sizeof(tag.strGroupName) - 1);
    tag.iChannelUniqueId = vresp->extract_U32();
    tag.iChannelNumber = vresp->extract_U32();

    PVR->TransferChannelGroupMember(handle, &tag);
  }

  return true;
}
