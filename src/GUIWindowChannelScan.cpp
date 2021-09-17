/*
 *  Copyright (C) 2010-2021 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "GUIWindowChannelScan.h"

#include "RequestPacket.h"
#include "ResponsePacket.h"
#include "vnsicommand.h"

#include <algorithm>
#include <cctype>
#include <kodi/General.h>
#include <kodi/Network.h>
#include <limits.h>
#include <sstream>

#define BUTTON_START 5
#define BUTTON_BACK 6
#define BUTTON_CANCEL 7
#define HEADER_LABEL 8

#define SPIN_CONTROL_SOURCE_TYPE 10
#define CONTROL_RADIO_BUTTON_TV 11
#define CONTROL_RADIO_BUTTON_RADIO 12
#define CONTROL_RADIO_BUTTON_FTA 13
#define CONTROL_RADIO_BUTTON_SCRAMBLED 14
#define CONTROL_RADIO_BUTTON_HD 15
#define CONTROL_SPIN_COUNTRIES 16
#define CONTROL_SPIN_SATELLITES 17
#define CONTROL_SPIN_DVBC_INVERSION 18
#define CONTROL_SPIN_DVBC_SYMBOLRATE 29
#define CONTROL_SPIN_DVBC_QAM 20
#define CONTROL_SPIN_DVBT_INVERSION 21
#define CONTROL_SPIN_ATSC_TYPE 22

#define LABEL_TYPE 30
#define LABEL_DEVICE 31
#define PROGRESS_DONE 32
#define LABEL_TRANSPONDER 33
#define LABEL_SIGNAL 34
#define PROGRESS_SIGNAL 35
#define LABEL_STATUS 36

cVNSIChannelScan::cVNSIChannelScan(kodi::addon::CInstancePVRClient& instance)
  : cVNSISession(instance), kodi::gui::CWindow("ChannelScan.xml", "skin.estuary", true, false)
{
}

bool cVNSIChannelScan::Open(const std::string& hostname,
                            int port,
                            const std::string& mac,
                            const char* name)
{
  m_hostname = hostname;
  m_port = port;
  m_wolMac = mac;
  m_running = false;
  m_Canceled = false;
  m_stopped = true;

  if (!cVNSISession::Open(hostname, port, name))
    return false;

  if (!cVNSISession::Login())
    return false;

  m_abort = false;
  m_connectionLost = false;
  m_threadRunning = true;
  m_thread = std::thread([&] { Process(); });

  kodi::gui::CWindow::DoModal();

  delete m_spinSourceType;
  delete m_spinCountries;
  delete m_spinSatellites;
  delete m_spinDVBCInversion;
  delete m_spinDVBCSymbolrates;
  delete m_spinDVBCqam;
  delete m_spinDVBTInversion;
  delete m_spinATSCType;
  delete m_radioButtonTV;
  delete m_radioButtonRadio;
  delete m_radioButtonFTA;
  delete m_radioButtonScrambled;
  delete m_radioButtonHD;
  delete m_progressDone;
  delete m_progressSignal;

  m_threadRunning = false;
  if (m_thread.joinable())
    m_thread.join();
  kodi::gui::CWindow::Close();

  return true;
}

bool cVNSIChannelScan::OnInit()
{
  m_spinSourceType = new kodi::gui::controls::CSpin(this, SPIN_CONTROL_SOURCE_TYPE);
  m_spinSourceType->AddLabel("DVB-T", DVB_TERR);
  m_spinSourceType->AddLabel("DVB-C", DVB_CABLE);
  m_spinSourceType->AddLabel("DVB-S/S2", DVB_SAT);
  m_spinSourceType->AddLabel("Analog TV", PVRINPUT);
  m_spinSourceType->AddLabel("Analog Radio", PVRINPUT_FM);
  m_spinSourceType->AddLabel("ATSC", DVB_ATSC);

  m_spinDVBCInversion = new kodi::gui::controls::CSpin(this, CONTROL_SPIN_DVBC_INVERSION);
  m_spinDVBCInversion->AddLabel("Auto", 0);
  m_spinDVBCInversion->AddLabel("On", 1);
  m_spinDVBCInversion->AddLabel("Off", 2);

  m_spinDVBCSymbolrates = new kodi::gui::controls::CSpin(this, CONTROL_SPIN_DVBC_SYMBOLRATE);
  m_spinDVBCSymbolrates->AddLabel("AUTO", 0);
  m_spinDVBCSymbolrates->AddLabel("6900", 1);
  m_spinDVBCSymbolrates->AddLabel("6875", 2);
  m_spinDVBCSymbolrates->AddLabel("6111", 3);
  m_spinDVBCSymbolrates->AddLabel("6250", 4);
  m_spinDVBCSymbolrates->AddLabel("6790", 5);
  m_spinDVBCSymbolrates->AddLabel("6811", 6);
  m_spinDVBCSymbolrates->AddLabel("5900", 7);
  m_spinDVBCSymbolrates->AddLabel("5000", 8);
  m_spinDVBCSymbolrates->AddLabel("3450", 9);
  m_spinDVBCSymbolrates->AddLabel("4000", 10);
  m_spinDVBCSymbolrates->AddLabel("6950", 11);
  m_spinDVBCSymbolrates->AddLabel("7000", 12);
  m_spinDVBCSymbolrates->AddLabel("6952", 13);
  m_spinDVBCSymbolrates->AddLabel("5156", 14);
  m_spinDVBCSymbolrates->AddLabel("4583", 15);
  m_spinDVBCSymbolrates->AddLabel("ALL (slow)", 16);

  m_spinDVBCqam = new kodi::gui::controls::CSpin(this, CONTROL_SPIN_DVBC_QAM);
  m_spinDVBCqam->AddLabel("AUTO", 0);
  m_spinDVBCqam->AddLabel("64", 1);
  m_spinDVBCqam->AddLabel("128", 2);
  m_spinDVBCqam->AddLabel("256", 3);
  m_spinDVBCqam->AddLabel("ALL (slow)", 4);

  m_spinDVBTInversion = new kodi::gui::controls::CSpin(this, CONTROL_SPIN_DVBT_INVERSION);
  m_spinDVBTInversion->AddLabel("Auto", 0);
  m_spinDVBTInversion->AddLabel("On", 1);
  m_spinDVBTInversion->AddLabel("Off", 2);

  m_spinATSCType = new kodi::gui::controls::CSpin(this, CONTROL_SPIN_ATSC_TYPE);
  m_spinATSCType->AddLabel("VSB (aerial)", 0);
  m_spinATSCType->AddLabel("QAM (cable)", 1);
  m_spinATSCType->AddLabel("VSB + QAM (aerial + cable)", 2);

  m_radioButtonTV = new kodi::gui::controls::CRadioButton(this, CONTROL_RADIO_BUTTON_TV);
  m_radioButtonTV->SetSelected(true);

  m_radioButtonRadio = new kodi::gui::controls::CRadioButton(this, CONTROL_RADIO_BUTTON_RADIO);
  m_radioButtonRadio->SetSelected(true);

  m_radioButtonFTA = new kodi::gui::controls::CRadioButton(this, CONTROL_RADIO_BUTTON_FTA);
  m_radioButtonFTA->SetSelected(true);

  m_radioButtonScrambled =
      new kodi::gui::controls::CRadioButton(this, CONTROL_RADIO_BUTTON_SCRAMBLED);
  m_radioButtonScrambled->SetSelected(true);

  m_radioButtonHD = new kodi::gui::controls::CRadioButton(this, CONTROL_RADIO_BUTTON_HD);
  m_radioButtonHD->SetSelected(true);

  m_progressDone = new kodi::gui::controls::CProgress(this, PROGRESS_DONE);
  m_progressSignal = new kodi::gui::controls::CProgress(this, PROGRESS_SIGNAL);

  if (!ReadCountries())
    return false;

  if (!ReadSatellites())
    return false;

  SetControlsVisible(DVB_TERR);
  return true;
}

bool cVNSIChannelScan::OnFocus(int controlId)
{
  return true;
}

bool cVNSIChannelScan::OnClick(int controlId)
{
  if (controlId == SPIN_CONTROL_SOURCE_TYPE)
  {
    int value = m_spinSourceType->GetIntValue();
    SetControlsVisible((scantype_t)value);
  }
  else if (controlId == BUTTON_BACK)
  {
    kodi::gui::CWindow::Close();
  }
  else if (controlId == BUTTON_START)
  {
    try
    {
      if (!m_running)
      {
        m_running = true;
        m_stopped = false;
        m_Canceled = false;
        SetProperty("Scanning", "running");
        SetControlLabel(BUTTON_START, kodi::GetLocalizedString(222));
        StartScan();
      }
      else if (!m_stopped)
      {
        m_stopped = true;
        m_Canceled = true;
        StopScan();
      }
      else
        ReturnFromProcessView();
    }
    catch (std::exception e)
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
      return false;
    }
  }
  return true;
}

bool cVNSIChannelScan::OnAction(ADDON_ACTION actionId)
{
  if (actionId == ADDON_ACTION_NAV_BACK || actionId == ADDON_ACTION_PREVIOUS_MENU)
    OnClick(BUTTON_BACK);

  return true;
}

void cVNSIChannelScan::StartScan()
{
  m_header = kodi::GetLocalizedString(30025);
  m_Signal = kodi::GetLocalizedString(30029);
  SetProgress(0);
  SetSignal(0, false);

  int source = m_spinSourceType->GetIntValue();
  switch (source)
  {
    case DVB_TERR:
      SetControlLabel(LABEL_TYPE, "DVB-T");
      break;
    case DVB_CABLE:
      SetControlLabel(LABEL_TYPE, "DVB-C");
      break;
    case DVB_SAT:
      SetControlLabel(LABEL_TYPE, "DVB-S/S2");
      break;
    case PVRINPUT:
      SetControlLabel(LABEL_TYPE, kodi::GetLocalizedString(30032));
      break;
    case PVRINPUT_FM:
      SetControlLabel(LABEL_TYPE, kodi::GetLocalizedString(30033));
      break;
    case DVB_ATSC:
      SetControlLabel(LABEL_TYPE, "ATSC");
      break;
  }

  cRequestPacket vrp;
  uint32_t retCode = VNSI_RET_ERROR;
  vrp.init(VNSI_SCAN_START);
  vrp.add_U32(source);
  vrp.add_U8(m_radioButtonTV->IsSelected());
  vrp.add_U8(m_radioButtonRadio->IsSelected());
  vrp.add_U8(m_radioButtonFTA->IsSelected());
  vrp.add_U8(m_radioButtonScrambled->IsSelected());
  vrp.add_U8(m_radioButtonHD->IsSelected());
  vrp.add_U32(m_spinCountries->GetIntValue());
  vrp.add_U32(m_spinDVBCInversion->GetIntValue());
  vrp.add_U32(m_spinDVBCSymbolrates->GetIntValue());
  vrp.add_U32(m_spinDVBCqam->GetIntValue());
  vrp.add_U32(m_spinDVBTInversion->GetIntValue());
  vrp.add_U32(m_spinSatellites->GetIntValue());
  vrp.add_U32(m_spinATSCType->GetIntValue());

  {
    auto vresp = ReadResult(&vrp);
    if (!vresp)
      goto SCANError;

    retCode = vresp->extract_U32();
    if (retCode != VNSI_RET_OK)
      goto SCANError;
  }

  return;

SCANError:
  kodi::Log(ADDON_LOG_ERROR, "%s - Return error after start (%i)", __func__, retCode);
  SetControlLabel(LABEL_STATUS, kodi::GetLocalizedString(24071));
  SetControlLabel(BUTTON_START, kodi::GetLocalizedString(30024));
  SetControlLabel(HEADER_LABEL, kodi::GetLocalizedString(30043));
  m_stopped = true;
}

void cVNSIChannelScan::StopScan()
{
  cRequestPacket vrp;
  vrp.init(VNSI_SCAN_STOP);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
    return;

  uint32_t retCode = vresp->extract_U32();
  if (retCode != VNSI_RET_OK)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - Return error after stop (%i)", __func__, retCode);
    SetControlLabel(LABEL_STATUS, kodi::GetLocalizedString(24071));
    SetControlLabel(BUTTON_START, kodi::GetLocalizedString(30024));
    SetControlLabel(HEADER_LABEL, kodi::GetLocalizedString(30043));
    m_stopped = true;
  }
  return;
}

void cVNSIChannelScan::ReturnFromProcessView()
{
  if (m_running)
  {
    m_running = false;
    ClearProperties();
    SetControlLabel(BUTTON_START, kodi::GetLocalizedString(30010));
    SetControlLabel(HEADER_LABEL, kodi::GetLocalizedString(30009));

    m_progressDone->SetVisible(false);
    m_progressSignal->SetVisible(false);
  }
}

void cVNSIChannelScan::SetProgress(int percent)
{
  std::stringstream header;
  header << percent;

  SetControlLabel(HEADER_LABEL, header.str().c_str());
  m_progressDone->SetPercentage((float)percent);
}

void cVNSIChannelScan::SetSignal(int percent, bool locked)
{
  std::stringstream signal;
  signal << percent;

  SetControlLabel(LABEL_SIGNAL, signal.str().c_str());
  m_progressSignal->SetPercentage((float)percent);

  if (locked)
    SetProperty("Locked", "true");
  else
    SetProperty("Locked", "");
}

bool cVNSIChannelScan::ReadCountries()
{
  m_spinCountries = new kodi::gui::controls::CSpin(this, CONTROL_SPIN_COUNTRIES);

  std::string dvdlang = kodi::GetLanguage(LANG_FMT_ISO_639_1);
  std::transform(dvdlang.begin(), dvdlang.end(), dvdlang.begin(),
                 [](unsigned char c) { return std::toupper(c); });

  cRequestPacket vrp;
  vrp.init(VNSI_SCAN_GETCOUNTRIES);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
    return false;

  int startIndex = -1;
  uint32_t retCode = vresp->extract_U32();
  if (retCode == VNSI_RET_OK)
  {
    while (vresp->getRemainingLength() >= 4 + 2)
    {
      uint32_t index = vresp->extract_U32();
      const char* isoName = vresp->extract_String();
      const char* longName = vresp->extract_String();
      m_spinCountries->AddLabel(longName, index);
      if (dvdlang == isoName)
        startIndex = index;
    }
    if (startIndex >= 0)
      m_spinCountries->SetIntValue(startIndex);
  }
  else
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - Return error after reading countries (%i)", __func__, retCode);
  }
  return retCode == VNSI_RET_OK;
}

bool cVNSIChannelScan::ReadSatellites()
{
  m_spinSatellites = new kodi::gui::controls::CSpin(this, CONTROL_SPIN_SATELLITES);

  cRequestPacket vrp;
  vrp.init(VNSI_SCAN_GETSATELLITES);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
    return false;

  uint32_t retCode = vresp->extract_U32();
  if (retCode == VNSI_RET_OK)
  {
    while (vresp->getRemainingLength() >= 4 + 2)
    {
      uint32_t index = vresp->extract_U32();
      const char* shortName = vresp->extract_String();
      const char* longName = vresp->extract_String();
      m_spinSatellites->AddLabel(longName, index);
    }
    m_spinSatellites->SetIntValue(6); /* default to Astra 19.2         */
  }
  else
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - Return error after reading satellites (%i)", __func__,
              retCode);
  }
  return retCode == VNSI_RET_OK;
}

void cVNSIChannelScan::SetControlsVisible(scantype_t type)
{
  m_spinCountries->SetVisible(type == DVB_TERR || type == DVB_CABLE || type == PVRINPUT);
  m_spinSatellites->SetVisible(type == DVB_SAT || type == DVB_ATSC);
  m_spinDVBCInversion->SetVisible(type == DVB_CABLE);
  m_spinDVBCSymbolrates->SetVisible(type == DVB_CABLE);
  m_spinDVBCqam->SetVisible(type == DVB_CABLE);
  m_spinDVBTInversion->SetVisible(type == DVB_TERR);
  m_spinATSCType->SetVisible(type == DVB_ATSC);
  m_radioButtonTV->SetVisible(type == DVB_TERR || type == DVB_CABLE || type == DVB_SAT ||
                              type == DVB_ATSC);
  m_radioButtonRadio->SetVisible(type == DVB_TERR || type == DVB_CABLE || type == DVB_SAT ||
                                 type == DVB_ATSC);
  m_radioButtonFTA->SetVisible(type == DVB_TERR || type == DVB_CABLE || type == DVB_SAT ||
                               type == DVB_ATSC);
  m_radioButtonScrambled->SetVisible(type == DVB_TERR || type == DVB_CABLE || type == DVB_SAT ||
                                     type == DVB_ATSC);
  m_radioButtonHD->SetVisible(type == DVB_TERR || type == DVB_CABLE || type == DVB_SAT ||
                              type == DVB_ATSC);
}

bool cVNSIChannelScan::OnResponsePacket(cResponsePacket* resp)
{
  uint32_t requestID = resp->getRequestID();

  if (requestID == VNSI_SCANNER_PERCENTAGE)
  {
    uint32_t percent = resp->extract_U32();
    if (percent <= 100)
      SetProgress(percent);
  }
  else if (requestID == VNSI_SCANNER_SIGNAL)
  {
    uint32_t strength = resp->extract_U32();
    uint32_t locked = resp->extract_U32();
    SetSignal(strength, (locked > 0));
  }
  else if (requestID == VNSI_SCANNER_DEVICE)
  {
    char* str = resp->extract_String();
    SetControlLabel(LABEL_DEVICE, str);
  }
  else if (requestID == VNSI_SCANNER_TRANSPONDER)
  {
    char* str = resp->extract_String();
    SetControlLabel(LABEL_TRANSPONDER, str);
  }
  else if (requestID == VNSI_SCANNER_NEWCHANNEL)
  {
    uint32_t isRadio = resp->extract_U32();
    uint32_t isEncrypted = resp->extract_U32();
    uint32_t isHD = resp->extract_U32();
    char* str = resp->extract_String();

    std::shared_ptr<kodi::gui::CListItem> item = std::make_shared<kodi::gui::CListItem>(str);

    if (isEncrypted)
      item->SetProperty("IsEncrypted", "yes");
    if (isRadio)
      item->SetProperty("IsRadio", "yes");
    if (isHD)
      item->SetProperty("IsHD", "yes");

    AddListItem(item, 0);
  }
  else if (requestID == VNSI_SCANNER_FINISHED)
  {
    if (!m_Canceled)
    {
      SetControlLabel(HEADER_LABEL, kodi::GetLocalizedString(30036));
      SetControlLabel(BUTTON_START, kodi::GetLocalizedString(30024));
      SetControlLabel(LABEL_STATUS, kodi::GetLocalizedString(30041));
    }
    else
    {
      SetControlLabel(HEADER_LABEL, kodi::GetLocalizedString(30042));
    }
  }
  else if (requestID == VNSI_SCANNER_STATUS)
  {
    uint32_t status = resp->extract_U32();
    if (status == 0)
    {
      if (m_Canceled)
        SetControlLabel(LABEL_STATUS, kodi::GetLocalizedString(16200));
      else
        SetControlLabel(LABEL_STATUS, kodi::GetLocalizedString(30040));

      SetControlLabel(BUTTON_START, kodi::GetLocalizedString(30024));
      m_stopped = true;
    }
    else if (status == 1)
    {
      SetControlLabel(LABEL_STATUS, kodi::GetLocalizedString(30039));
    }
    else if (status == 2)
    {
      SetControlLabel(LABEL_STATUS, kodi::GetLocalizedString(30037));
      SetControlLabel(BUTTON_START, kodi::GetLocalizedString(30024));
      SetControlLabel(HEADER_LABEL, kodi::GetLocalizedString(30043));
      m_stopped = true;
    }
    else if (status == 3)
    {
      SetControlLabel(LABEL_STATUS, kodi::GetLocalizedString(30038));
    }
  }
  else
  {
    return false;
  }

  return true;
}

void cVNSIChannelScan::Process()
{
  std::unique_ptr<cResponsePacket> vresp;

  while (m_threadRunning)
  {
    // try to reconnect
    if (m_connectionLost)
    {
      // First wake up the VDR server in case a MAC-Address is specified
      if (!m_wolMac.empty())
      {
        if (!kodi::network::WakeOnLan(m_wolMac))
        {
          kodi::Log(ADDON_LOG_ERROR, "Error waking up VNSI Server at MAC-Address %s",
                    m_wolMac.c_str());
        }
      }

      cVNSISession::eCONNECTIONSTATE state = TryReconnect();
      if (state != cVNSISession::CONN_ESABLISHED)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        continue;
      }
    }

    // if there's anything in the buffer, read it
    if ((vresp = cVNSISession::ReadMessage(5, 10000)) == nullptr)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      continue;
    }

    if (!OnResponsePacket(vresp.get()))
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - Rxd a response packet on channel %lu !!", __func__,
                vresp->getChannelID());
    }
  }
}
