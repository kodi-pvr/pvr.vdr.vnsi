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

#include <limits.h>
#include "VNSIChannelScan.h"
#include "responsepacket.h"
#include "requestpacket.h"
#include "vnsicommand.h"

#include <sstream>

#include <kodi/api2/AddonLib.hpp>
#include <kodi/api2/addon/General.hpp>
#include <kodi/api2/gui/ListItem.hpp>

#define BUTTON_START                    5
#define BUTTON_BACK                     6
#define BUTTON_CANCEL                   7
#define HEADER_LABEL                    8

#define SPIN_CONTROL_SOURCE_TYPE        10
#define CONTROL_RADIO_BUTTON_TV         11
#define CONTROL_RADIO_BUTTON_RADIO      12
#define CONTROL_RADIO_BUTTON_FTA        13
#define CONTROL_RADIO_BUTTON_SCRAMBLED  14
#define CONTROL_RADIO_BUTTON_HD         15
#define CONTROL_SPIN_COUNTRIES          16
#define CONTROL_SPIN_SATELLITES         17
#define CONTROL_SPIN_DVBC_INVERSION     18
#define CONTROL_SPIN_DVBC_SYMBOLRATE    29
#define CONTROL_SPIN_DVBC_QAM           20
#define CONTROL_SPIN_DVBT_INVERSION     21
#define CONTROL_SPIN_ATSC_TYPE          22

#define LABEL_TYPE                      30
#define LABEL_DEVICE                    31
#define PROGRESS_DONE                   32
#define LABEL_TRANSPONDER               33
#define LABEL_SIGNAL                    34
#define PROGRESS_SIGNAL                 35
#define LABEL_STATUS                    36

cVNSIChannelScan::cVNSIChannelScan()
 : CWindow("ChannelScan.xml", "skin.confluence", false, true),
   m_running(false),
   m_stopped(false),
   m_Canceled(false),
   m_spinSourceType(NULL),
   m_spinCountries(NULL),
   m_spinSatellites(NULL),
   m_spinDVBCInversion(NULL),
   m_spinDVBCSymbolrates(NULL),
   m_spinDVBCqam(NULL),
   m_spinDVBTInversion(NULL),
   m_spinATSCType(NULL),
   m_radioButtonTV(NULL),
   m_radioButtonRadio(NULL),
   m_radioButtonFTA(NULL),
   m_radioButtonScrambled(NULL),
   m_radioButtonHD(NULL),
   m_progressDone(NULL),
   m_progressSignal(NULL)
{
}

cVNSIChannelScan::~cVNSIChannelScan()
{
}

bool cVNSIChannelScan::Open(const std::string& hostname, int port, const char* name)
{
  m_running         = false;
  m_Canceled        = false;
  m_stopped         = true;
  m_progressDone    = NULL;
  m_progressSignal  = NULL;

  if(!cVNSIData::Open(hostname, port, "XBMC channel scanner"))
    return false;

  /* Load the Window as Dialog */
  KodiAPI::GUI::CWindow::DoModal();

  cVNSIData::Close();

  return true;
}

void cVNSIChannelScan::StartScan()
{
  m_header = KodiAPI::AddOn::General::GetLocalizedString(30025);
  m_Signal = KodiAPI::AddOn::General::GetLocalizedString(30029);
  SetProgress(0);
  SetSignal(0, false);

  int source = m_spinSourceType->GetIntValue();
  switch (source)
  {
    case DVB_TERR:
      KodiAPI::GUI::CWindow::SetControlLabel(LABEL_TYPE, "DVB-T");
      break;
    case DVB_CABLE:
      KodiAPI::GUI::CWindow::SetControlLabel(LABEL_TYPE, "DVB-C");
      break;
    case DVB_SAT:
      KodiAPI::GUI::CWindow::SetControlLabel(LABEL_TYPE, "DVB-S/S2");
      break;
    case PVRINPUT:
      KodiAPI::GUI::CWindow::SetControlLabel(LABEL_TYPE, KodiAPI::AddOn::General::GetLocalizedString(30032).c_str());
      break;
    case PVRINPUT_FM:
      KodiAPI::GUI::CWindow::SetControlLabel(LABEL_TYPE, KodiAPI::AddOn::General::GetLocalizedString(30033).c_str());
      break;
    case DVB_ATSC:
      KodiAPI::GUI::CWindow::SetControlLabel(LABEL_TYPE, "ATSC");
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
  KodiAPI::Log(ADDON_LOG_ERROR, "%s - Return error after start (%i)", __FUNCTION__, retCode);
  KodiAPI::GUI::CWindow::SetControlLabel(LABEL_STATUS, KodiAPI::AddOn::General::GetLocalizedString(24071).c_str());
  KodiAPI::GUI::CWindow::SetControlLabel(BUTTON_START, KodiAPI::AddOn::General::GetLocalizedString(30024).c_str());
  KodiAPI::GUI::CWindow::SetControlLabel(HEADER_LABEL, KodiAPI::AddOn::General::GetLocalizedString(30043).c_str());
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
    KodiAPI::Log(ADDON_LOG_ERROR, "%s - Return error after stop (%i)", __FUNCTION__, retCode);
    KodiAPI::GUI::CWindow::SetControlLabel(LABEL_STATUS, KodiAPI::AddOn::General::GetLocalizedString(24071).c_str());
    KodiAPI::GUI::CWindow::SetControlLabel(BUTTON_START, KodiAPI::AddOn::General::GetLocalizedString(30024).c_str());
    KodiAPI::GUI::CWindow::SetControlLabel(HEADER_LABEL, KodiAPI::AddOn::General::GetLocalizedString(30043).c_str());
    m_stopped = true;
  }
  return;
}

void cVNSIChannelScan::ReturnFromProcessView()
{
  if (m_running)
  {
    m_running = false;
    KodiAPI::GUI::CWindow::ClearProperties();
    KodiAPI::GUI::CWindow::SetControlLabel(BUTTON_START, KodiAPI::AddOn::General::GetLocalizedString(30010).c_str());
    KodiAPI::GUI::CWindow::SetControlLabel(HEADER_LABEL, KodiAPI::AddOn::General::GetLocalizedString(30009).c_str());

    if (m_progressDone)
    {
      delete m_progressDone;
      m_progressDone = NULL;
    }
    if (m_progressSignal)
    {
      delete m_progressSignal;
      m_progressSignal = NULL;
    }
  }
}

void cVNSIChannelScan::SetProgress(int percent)
{
  if (!m_progressDone)
    m_progressDone = new KodiAPI::GUI::CControlProgress(this, PROGRESS_DONE);

  std::stringstream header;
  header << percent;

  KodiAPI::GUI::CWindow::SetControlLabel(HEADER_LABEL, header.str().c_str());
  m_progressDone->SetPercentage((float)percent);
}

void cVNSIChannelScan::SetSignal(int percent, bool locked)
{
  if (!m_progressSignal)
    m_progressSignal = new KodiAPI::GUI::CControlProgress(this, PROGRESS_SIGNAL);

  std::stringstream signal;
  signal << percent;

  KodiAPI::GUI::CWindow::SetControlLabel(LABEL_SIGNAL, signal.str().c_str());
  m_progressSignal->SetPercentage((float)percent);

  if (locked)
    KodiAPI::GUI::CWindow::SetProperty("Locked", "true");
  else
    KodiAPI::GUI::CWindow::SetProperty("Locked", "");
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
    KodiAPI::GUI::CWindow::Close();
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
    if (m_progressDone)
    {
      delete m_progressDone;
      m_progressDone = NULL;
    }
    if (m_progressSignal)
    {
      delete m_progressSignal;
      m_progressSignal = NULL;
    }
  }
  else if (controlId == BUTTON_START)
  {
    try {
      if (!m_running)
      {
        m_running = true;
        m_stopped = false;
        m_Canceled = false;
        KodiAPI::GUI::CWindow::SetProperty("Scanning", "running");
        KodiAPI::GUI::CWindow::SetControlLabel(BUTTON_START, KodiAPI::AddOn::General::GetLocalizedString(222).c_str());
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
    } catch (std::exception e) {
      KodiAPI::Log(ADDON_LOG_ERROR, "%s - %s", __FUNCTION__, e.what());
      return false;
    }
  }
  return true;
}

bool cVNSIChannelScan::OnFocus(int controlId)
{
  return true;
}

bool cVNSIChannelScan::OnInit()
{
  m_spinSourceType = new KodiAPI::GUI::CControlSpin(this, SPIN_CONTROL_SOURCE_TYPE);
  m_spinSourceType->Reset();
  m_spinSourceType->AddLabel("DVB-T", DVB_TERR);
  m_spinSourceType->AddLabel("DVB-C", DVB_CABLE);
  m_spinSourceType->AddLabel("DVB-S/S2", DVB_SAT);
  m_spinSourceType->AddLabel("Analog TV", PVRINPUT);
  m_spinSourceType->AddLabel("Analog Radio", PVRINPUT_FM);
  m_spinSourceType->AddLabel("ATSC", DVB_ATSC);

  m_spinDVBCInversion = new KodiAPI::GUI::CControlSpin(this, CONTROL_SPIN_DVBC_INVERSION);
  m_spinDVBCInversion->Reset();
  m_spinDVBCInversion->AddLabel("Auto", 0);
  m_spinDVBCInversion->AddLabel("On", 1);
  m_spinDVBCInversion->AddLabel("Off", 2);

  m_spinDVBCSymbolrates = new KodiAPI::GUI::CControlSpin(this, CONTROL_SPIN_DVBC_SYMBOLRATE);
  m_spinDVBCSymbolrates->Reset();
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

  m_spinDVBCqam = new KodiAPI::GUI::CControlSpin(this, CONTROL_SPIN_DVBC_QAM);
  m_spinDVBCqam->Reset();
  m_spinDVBCqam->AddLabel("AUTO", 0);
  m_spinDVBCqam->AddLabel("64", 1);
  m_spinDVBCqam->AddLabel("128", 2);
  m_spinDVBCqam->AddLabel("256", 3);
  m_spinDVBCqam->AddLabel("ALL (slow)", 4);

  m_spinDVBTInversion = new KodiAPI::GUI::CControlSpin(this, CONTROL_SPIN_DVBT_INVERSION);
  m_spinDVBTInversion->Reset();
  m_spinDVBTInversion->AddLabel("Auto", 0);
  m_spinDVBTInversion->AddLabel("On", 1);
  m_spinDVBTInversion->AddLabel("Off", 2);

  m_spinATSCType = new KodiAPI::GUI::CControlSpin(this, CONTROL_SPIN_ATSC_TYPE);
  m_spinATSCType->Reset();
  m_spinATSCType->AddLabel("VSB (aerial)", 0);
  m_spinATSCType->AddLabel("QAM (cable)", 1);
  m_spinATSCType->AddLabel("VSB + QAM (aerial + cable)", 2);

  m_radioButtonTV = new KodiAPI::GUI::CControlRadioButton(this, CONTROL_RADIO_BUTTON_TV);
  m_radioButtonTV->SetSelected(true);

  m_radioButtonRadio = new KodiAPI::GUI::CControlRadioButton(this, CONTROL_RADIO_BUTTON_RADIO);
  m_radioButtonRadio->SetSelected(true);

  m_radioButtonFTA = new KodiAPI::GUI::CControlRadioButton(this, CONTROL_RADIO_BUTTON_FTA);
  m_radioButtonFTA->SetSelected(true);

  m_radioButtonScrambled = new KodiAPI::GUI::CControlRadioButton(this, CONTROL_RADIO_BUTTON_SCRAMBLED);
  m_radioButtonScrambled->SetSelected(true);

  m_radioButtonHD = new KodiAPI::GUI::CControlRadioButton(this, CONTROL_RADIO_BUTTON_HD);
  m_radioButtonHD->SetSelected(true);

  if (!ReadCountries())
    return false;

  if (!ReadSatellites())
    return false;

  SetControlsVisible(DVB_TERR);
  return true;
}

bool cVNSIChannelScan::OnAction(int actionId)
{
  if (actionId == ADDON_ACTION_PREVIOUS_MENU)
    OnClick(BUTTON_BACK);

  return true;
}

bool cVNSIChannelScan::ReadCountries()
{
  m_spinCountries = new KodiAPI::GUI::CControlSpin(this, CONTROL_SPIN_COUNTRIES);
  m_spinCountries->Reset();

  std::string dvdlang = KodiAPI::AddOn::General::GetDVDMenuLanguage();
  //dvdlang = dvdlang.ToUpper();

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
      uint32_t    index     = vresp->extract_U32();
      const char *isoName   = vresp->extract_String();
      const char *longName  = vresp->extract_String();
      m_spinCountries->AddLabel(longName, index);
      if (dvdlang == isoName)
        startIndex = index;
    }
    if (startIndex >= 0)
      m_spinCountries->SetIntValue(startIndex);
  }
  else
  {
    KodiAPI::Log(ADDON_LOG_ERROR, "%s - Return error after reading countries (%i)", __FUNCTION__, retCode);
  }
  return retCode == VNSI_RET_OK;
}

bool cVNSIChannelScan::ReadSatellites()
{
  m_spinSatellites = new KodiAPI::GUI::CControlSpin(this, CONTROL_SPIN_SATELLITES);
  m_spinSatellites->Reset();

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
      uint32_t    index     = vresp->extract_U32();
      const char *shortName = vresp->extract_String();
      const char *longName  = vresp->extract_String();
      m_spinSatellites->AddLabel(longName, index);
    }
    m_spinSatellites->SetIntValue(6);      /* default to Astra 19.2         */
  }
  else
  {
    KodiAPI::Log(ADDON_LOG_ERROR, "%s - Return error after reading satellites (%i)", __FUNCTION__, retCode);
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
  m_radioButtonTV->SetVisible(type == DVB_TERR || type == DVB_CABLE || type == DVB_SAT || type == DVB_ATSC);
  m_radioButtonRadio->SetVisible(type == DVB_TERR || type == DVB_CABLE || type == DVB_SAT || type == DVB_ATSC);
  m_radioButtonFTA->SetVisible(type == DVB_TERR || type == DVB_CABLE || type == DVB_SAT || type == DVB_ATSC);
  m_radioButtonScrambled->SetVisible(type == DVB_TERR || type == DVB_CABLE || type == DVB_SAT || type == DVB_ATSC);
  m_radioButtonHD->SetVisible(type == DVB_TERR || type == DVB_CABLE || type == DVB_SAT || type == DVB_ATSC);
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
    uint32_t locked   = resp->extract_U32();
    SetSignal(strength, (locked>0));
  }
  else if (requestID == VNSI_SCANNER_DEVICE)
  {
    char* str = resp->extract_String();
    KodiAPI::GUI::CWindow::SetControlLabel(LABEL_DEVICE, str);
  }
  else if (requestID == VNSI_SCANNER_TRANSPONDER)
  {
    char* str = resp->extract_String();
    KodiAPI::GUI::CWindow::SetControlLabel(LABEL_TRANSPONDER, str);
  }
  else if (requestID == VNSI_SCANNER_NEWCHANNEL)
  {
    uint32_t isRadio      = resp->extract_U32();
    uint32_t isEncrypted  = resp->extract_U32();
    uint32_t isHD         = resp->extract_U32();
    char* str             = resp->extract_String();

    KodiAPI::GUI::CListItem* item = new KodiAPI::GUI::CListItem(str, NULL, NULL, NULL, NULL);
    if (isEncrypted)
      item->SetProperty("IsEncrypted", "yes");
    if (isRadio)
      item->SetProperty("IsRadio", "yes");
    if (isHD)
      item->SetProperty("IsHD", "yes");

    KodiAPI::GUI::CWindow::AddItem(item, 0);
    delete item;
  }
  else if (requestID == VNSI_SCANNER_FINISHED)
  {
    if (!m_Canceled)
    {
      KodiAPI::GUI::CWindow::SetControlLabel(HEADER_LABEL, KodiAPI::AddOn::General::GetLocalizedString(30036).c_str());
      KodiAPI::GUI::CWindow::SetControlLabel(BUTTON_START, KodiAPI::AddOn::General::GetLocalizedString(30024).c_str());
      KodiAPI::GUI::CWindow::SetControlLabel(LABEL_STATUS, KodiAPI::AddOn::General::GetLocalizedString(30041).c_str());
    }
    else
    {
      KodiAPI::GUI::CWindow::SetControlLabel(HEADER_LABEL, KodiAPI::AddOn::General::GetLocalizedString(30042).c_str());
    }
  }
  else if (requestID == VNSI_SCANNER_STATUS)
  {
    uint32_t status = resp->extract_U32();
    if (status == 0)
    {
      if (m_Canceled)
        KodiAPI::GUI::CWindow::SetControlLabel(LABEL_STATUS, KodiAPI::AddOn::General::GetLocalizedString(16200).c_str());
      else
        KodiAPI::GUI::CWindow::SetControlLabel(LABEL_STATUS, KodiAPI::AddOn::General::GetLocalizedString(30040).c_str());

      KodiAPI::GUI::CWindow::SetControlLabel(BUTTON_START, KodiAPI::AddOn::General::GetLocalizedString(30024).c_str());
      m_stopped = true;
    }
    else if (status == 1)
    {
      KodiAPI::GUI::CWindow::SetControlLabel(LABEL_STATUS, KodiAPI::AddOn::General::GetLocalizedString(30039).c_str());
    }
    else if (status == 2)
    {
      KodiAPI::GUI::CWindow::SetControlLabel(LABEL_STATUS, KodiAPI::AddOn::General::GetLocalizedString(30037).c_str());
      KodiAPI::GUI::CWindow::SetControlLabel(BUTTON_START, KodiAPI::AddOn::General::GetLocalizedString(30024).c_str());
      KodiAPI::GUI::CWindow::SetControlLabel(HEADER_LABEL, KodiAPI::AddOn::General::GetLocalizedString(30043).c_str());
      m_stopped = true;
    }
    else if (status == 3)
    {
      KodiAPI::GUI::CWindow::SetControlLabel(LABEL_STATUS, KodiAPI::AddOn::General::GetLocalizedString(30038).c_str());
    }
  }
  else {
    return false;
  }

  return true;
}
