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
#include <sstream>

#include "VNSIChannelScan.h"
#include "VNSIData.h"
#include "responsepacket.h"
#include "requestpacket.h"
#include "vnsicommand.h"

//! GUI id's
#define BUTTON_START                    5
#define BUTTON_CANCEL                   6
#define HEADER_LABEL                    8
#define CONTROL_RENDER_ADDON            9

#define SPIN_CONTROL_SOURCE_TYPE        20
#define CONTROL_RADIO_BUTTON_TV         21
#define CONTROL_RADIO_BUTTON_RADIO      22
#define CONTROL_RADIO_BUTTON_FTA        23
#define CONTROL_RADIO_BUTTON_SCRAMBLED  24
#define CONTROL_RADIO_BUTTON_HD         25
#define CONTROL_SPIN_COUNTRIES          26
#define CONTROL_SPIN_SATELLITES         27
#define CONTROL_SPIN_DVBC_INVERSION     28
#define CONTROL_SPIN_DVBC_SYMBOLRATE    29
#define CONTROL_SPIN_DVBC_QAM           30
#define CONTROL_SPIN_DVBT_INVERSION     31
#define CONTROL_SPIN_ATSC_TYPE          32

#define LABEL_TYPE                      40
#define LABEL_DEVICE                    41
#define PROGRESS_DONE                   42
#define LABEL_TRANSPONDER               43
#define LABEL_SIGNAL                    44
#define PROGRESS_SIGNAL                 45
#define LABEL_STATUS                    46

//! Action id's
#define ACTION_NAV_BACK                 92

using namespace ADDON;

cVNSIChannelScan::cVNSIChannelScan()
 : m_running(false),
   m_stopped(false),
   m_Canceled(false),
   m_window(NULL),
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
   m_progressSignal(NULL),
   m_renderControl(NULL)
{
}

cVNSIChannelScan::~cVNSIChannelScan()
{
}

bool cVNSIChannelScan::Open(const std::string& hostname, int port, const char* name)
{
  if(!cVNSIData::Open(hostname, port, name))
    return false;

  if(!cVNSIData::Login())
    return false;

  m_running         = false;
  m_Canceled        = false;
  m_stopped         = true;
  m_progressDone    = NULL;
  m_progressSignal  = NULL;

  /* Load the Window as Dialog */
  m_window = GUI->Window_create("ChannelScan.xml", "Confluence", false, true);
  m_window->m_cbhdl   = this;
  m_window->CBOnInit  = OnInitCB;
  m_window->CBOnFocus = OnFocusCB;
  m_window->CBOnClick = OnClickCB;
  m_window->CBOnAction= OnActionCB;
  m_window->DoModal();

  GUI->Control_releaseRendering(m_renderControl);
  GUI->Window_destroy(m_window);

  return true;
}

/*!
 * Static GUI callback functions
 */
bool cVNSIChannelScan::OnInitCB(GUIHANDLE cbhdl)
{
  cVNSIChannelScan* scanner = static_cast<cVNSIChannelScan*>(cbhdl);
  return scanner->OnInit();
}

bool cVNSIChannelScan::OnClickCB(GUIHANDLE cbhdl, int controlId)
{
  cVNSIChannelScan* scanner = static_cast<cVNSIChannelScan*>(cbhdl);
  return scanner->OnClick(controlId);
}

bool cVNSIChannelScan::OnFocusCB(GUIHANDLE cbhdl, int controlId)
{
  cVNSIChannelScan* scanner = static_cast<cVNSIChannelScan*>(cbhdl);
  return scanner->OnFocus(controlId);
}

bool cVNSIChannelScan::OnActionCB(GUIHANDLE cbhdl, int actionId)
{
  cVNSIChannelScan* scanner = static_cast<cVNSIChannelScan*>(cbhdl);
  return scanner->OnAction(actionId);
}

bool cVNSIChannelScan::CreateCB(GUIHANDLE cbhdl, int x, int y, int w, int h, void *device)
{
  return true;
}

void cVNSIChannelScan::RenderCB(GUIHANDLE cbhdl)
{
}

void cVNSIChannelScan::StopCB(GUIHANDLE cbhdl)
{
}

bool cVNSIChannelScan::DirtyCB(GUIHANDLE cbhdl)
{
  cVNSIChannelScan* scanner = static_cast<cVNSIChannelScan*>(cbhdl);
  return scanner->Dirty();
}

/*!
 * Variable callback functions called from static
 */

bool cVNSIChannelScan::OnInit()
{
  uint32_t flags = ReadSupportedFlags();

  int firstSupported = 0;
  m_spinSourceType = GUI->Control_getSpin(m_window, SPIN_CONTROL_SOURCE_TYPE);
  m_spinSourceType->Clear();
  if (flags & VNSI_SCAN_SUPPORT_DVB_T)
    m_spinSourceType->AddLabel(XBMC->GetLocalizedString(30053), SCAN_TERRESTRIAL);
  if (flags & VNSI_SCAN_SUPPORT_DVB_C)
    m_spinSourceType->AddLabel(XBMC->GetLocalizedString(30054), SCAN_CABLE);
  if (flags & VNSI_SCAN_SUPPORT_DVB_S)
    m_spinSourceType->AddLabel(XBMC->GetLocalizedString(30055), SCAN_SATELLITE);
  if (flags & VNSI_SCAN_SUPPORT_ANALOG_TV)
    m_spinSourceType->AddLabel(XBMC->GetLocalizedString(30032), SCAN_PVRINPUT);
  if (flags & VNSI_SCAN_SUPPORT_ANALOG_RADIO)
    m_spinSourceType->AddLabel(XBMC->GetLocalizedString(30033), SCAN_PVRINPUT_FM);
  if (flags & VNSI_SCAN_SUPPORT_ATSC)
    m_spinSourceType->AddLabel(XBMC->GetLocalizedString(30058), SCAN_TERRCABLE_ATSC);

  m_spinDVBCInversion = GUI->Control_getSpin(m_window, CONTROL_SPIN_DVBC_INVERSION);
  m_spinDVBCInversion->Clear();
  m_spinDVBCInversion->AddLabel(XBMC->GetLocalizedString(30038), 0);
  m_spinDVBCInversion->AddLabel(XBMC->GetLocalizedString(30050), 1);
  m_spinDVBCInversion->AddLabel(XBMC->GetLocalizedString(30051), 2);

  m_spinDVBCSymbolrates = GUI->Control_getSpin(m_window, CONTROL_SPIN_DVBC_SYMBOLRATE);
  m_spinDVBCSymbolrates->Clear();
  m_spinDVBCSymbolrates->AddLabel(XBMC->GetLocalizedString(30038), 0);
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
  m_spinDVBCSymbolrates->AddLabel("5483", 15);
  m_spinDVBCSymbolrates->AddLabel(XBMC->GetLocalizedString(30052), 16);

  m_spinDVBCqam = GUI->Control_getSpin(m_window, CONTROL_SPIN_DVBC_QAM);
  m_spinDVBCqam->Clear();
  m_spinDVBCqam->AddLabel(XBMC->GetLocalizedString(30038), 0);
  m_spinDVBCqam->AddLabel("QAM-64", 1);
  m_spinDVBCqam->AddLabel("QAM-128", 2);
  m_spinDVBCqam->AddLabel("QAM-256", 3);
  m_spinDVBCqam->AddLabel(XBMC->GetLocalizedString(30052), 4);

  m_spinDVBTInversion = GUI->Control_getSpin(m_window, CONTROL_SPIN_DVBT_INVERSION);
  m_spinDVBTInversion->Clear();
  m_spinDVBTInversion->AddLabel(XBMC->GetLocalizedString(30038), 0);
  m_spinDVBTInversion->AddLabel(XBMC->GetLocalizedString(30050), 1);
  m_spinDVBTInversion->AddLabel(XBMC->GetLocalizedString(30051), 2);

  m_spinATSCType = GUI->Control_getSpin(m_window, CONTROL_SPIN_ATSC_TYPE);
  m_spinATSCType->Clear();
  m_spinATSCType->AddLabel(XBMC->GetLocalizedString(30059), 0);
  m_spinATSCType->AddLabel(XBMC->GetLocalizedString(30060), 1);
  m_spinATSCType->AddLabel(XBMC->GetLocalizedString(30061), 2);

  m_radioButtonTV = GUI->Control_getRadioButton(m_window, CONTROL_RADIO_BUTTON_TV);
  m_radioButtonTV->SetSelected(true);

  m_radioButtonRadio = GUI->Control_getRadioButton(m_window, CONTROL_RADIO_BUTTON_RADIO);
  m_radioButtonRadio->SetSelected(true);

  m_radioButtonFTA = GUI->Control_getRadioButton(m_window, CONTROL_RADIO_BUTTON_FTA);
  m_radioButtonFTA->SetSelected(true);

  m_radioButtonScrambled = GUI->Control_getRadioButton(m_window, CONTROL_RADIO_BUTTON_SCRAMBLED);
  m_radioButtonScrambled->SetSelected(true);

  m_radioButtonHD = GUI->Control_getRadioButton(m_window, CONTROL_RADIO_BUTTON_HD);
  m_radioButtonHD->SetSelected(true);

  m_renderControl = GUI->Control_getRendering(m_window, CONTROL_RENDER_ADDON);
  m_renderControl->m_cbhdl   = this;
  m_renderControl->CBCreate = CreateCB;
  m_renderControl->CBRender = RenderCB;
  m_renderControl->CBStop = StopCB;
  m_renderControl->CBDirty = DirtyCB;
  m_renderControl->Init();

  if (!ReadCountries())
    return false;

  if (!ReadSatellites())
    return false;

  m_window->ClearList();

  SetControlsVisible((scantype_t)m_spinSourceType->GetValue());
  return true;
}

bool cVNSIChannelScan::OnClick(int controlId)
{
  if (controlId == SPIN_CONTROL_SOURCE_TYPE)
  {
    int value = m_spinSourceType->GetValue();
    SetControlsVisible((scantype_t)value);
  }
  else if (controlId == BUTTON_CANCEL)
  {
    m_window->Close();
    GUI->Control_releaseSpin(m_spinSourceType);
    GUI->Control_releaseSpin(m_spinCountries);
    GUI->Control_releaseSpin(m_spinSatellites);
    GUI->Control_releaseSpin(m_spinDVBCInversion);
    GUI->Control_releaseSpin(m_spinDVBCSymbolrates);
    GUI->Control_releaseSpin(m_spinDVBCqam);
    GUI->Control_releaseSpin(m_spinDVBTInversion);
    GUI->Control_releaseSpin(m_spinATSCType);
    GUI->Control_releaseRadioButton(m_radioButtonTV);
    GUI->Control_releaseRadioButton(m_radioButtonRadio);
    GUI->Control_releaseRadioButton(m_radioButtonFTA);
    GUI->Control_releaseRadioButton(m_radioButtonScrambled);
    GUI->Control_releaseRadioButton(m_radioButtonHD);
    if (m_progressDone)
    {
      GUI->Control_releaseProgress(m_progressDone);
      m_progressDone = NULL;
    }
    if (m_progressSignal)
    {
      GUI->Control_releaseProgress(m_progressSignal);
      m_progressSignal = NULL;
    }
  }
  else if (controlId == BUTTON_START)
  {
    if (!m_running)
    {
      m_running = true;
      m_stopped = false;
      m_Canceled = false;
      m_window->SetProperty("Scanning", "running");
      m_window->SetControlLabel(BUTTON_START, XBMC->GetLocalizedString(222));
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
  return true;
}

bool cVNSIChannelScan::OnFocus(int controlId)
{
  return true;
}

bool cVNSIChannelScan::OnAction(int actionId)
{
  if (actionId == ADDON_ACTION_CLOSE_DIALOG ||
      actionId == ADDON_ACTION_PREVIOUS_MENU ||
      actionId == ACTION_NAV_BACK)
    return OnClick(BUTTON_CANCEL);
  else
    return false;
}

bool cVNSIChannelScan::Dirty()
{
  if (!m_ResponsePackets.empty())
  {
    cResponsePacket* resp = m_ResponsePackets.front();
    m_ResponsePackets.pop();

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
      m_window->SetControlLabel(LABEL_DEVICE, str);
      delete[] str;
    }
    else if (requestID == VNSI_SCANNER_TRANSPONDER)
    {
      char* str = resp->extract_String();
      m_window->SetControlLabel(LABEL_TRANSPONDER, str);
      delete[] str;
    }
    else if (requestID == VNSI_SCANNER_NEWCHANNEL)
    {
      uint32_t isRadio      = resp->extract_U32();
      uint32_t isEncrypted  = resp->extract_U32();
      uint32_t isHD         = resp->extract_U32();
      char* str             = resp->extract_String();

      CAddonListItem* item = GUI->ListItem_create(str, NULL, NULL, NULL, NULL);
      if (isEncrypted)
        item->SetProperty("IsEncrypted", "yes");
      if (isRadio)
        item->SetProperty("IsRadio", "yes");
      if (isHD)
        item->SetProperty("IsHD", "yes");

      m_window->AddItem(item, 0);
      GUI->ListItem_destroy(item);

      delete[] str;
    }
    else if (requestID == VNSI_SCANNER_FINISHED)
    {
      if (!m_Canceled)
      {
        m_window->SetControlLabel(HEADER_LABEL, XBMC->GetLocalizedString(30036));
        m_window->SetControlLabel(BUTTON_START, XBMC->GetLocalizedString(30024));
        m_window->SetControlLabel(LABEL_STATUS, XBMC->GetLocalizedString(30041));
      }
      else
      {
        m_window->SetControlLabel(HEADER_LABEL, XBMC->GetLocalizedString(30042));
      }
      m_window->SetProperty("ScanActive", "");
    }
    else if (requestID == VNSI_SCANNER_STATUS)
    {
      uint32_t status = resp->extract_U32();
      if (status == 0)
      {
        m_window->SetControlLabel(LABEL_STATUS, XBMC->GetLocalizedString(30040));
        m_window->SetControlLabel(BUTTON_START, XBMC->GetLocalizedString(30024));
        m_window->SetControlLabel(HEADER_LABEL, XBMC->GetLocalizedString(30043));
        m_stopped = true;
      }
      else if (status == 1)
      {
        m_window->SetControlLabel(LABEL_STATUS, XBMC->GetLocalizedString(30039));
      }
      else if (status == 2)
      {
        if (m_Canceled)
          m_window->SetControlLabel(LABEL_STATUS, XBMC->GetLocalizedString(16200));
        else
          m_window->SetControlLabel(LABEL_STATUS, XBMC->GetLocalizedString(30040));

        m_window->SetControlLabel(BUTTON_START, XBMC->GetLocalizedString(30024));
        m_stopped = true;
      }
      else if (status == 3)
      {
        m_window->SetControlLabel(LABEL_STATUS, XBMC->GetLocalizedString(30037));
        m_window->SetControlLabel(BUTTON_START, XBMC->GetLocalizedString(30024));
        m_stopped = true;
      }
    }
    return true;
  }
  return false;
}

/*!
 * Internal GUI class handling functions
 */

void cVNSIChannelScan::StartScan()
{
  m_header = XBMC->GetLocalizedString(30025);
  m_Signal = XBMC->GetLocalizedString(30029);
  SetProgress(0);
  SetSignal(0, false);

  int source = m_spinSourceType->GetValue();
  switch (source)
  {
    case SCAN_TERRESTRIAL:
      m_window->SetControlLabel(LABEL_TYPE, XBMC->GetLocalizedString(30053));
      break;
    case SCAN_CABLE:
      m_window->SetControlLabel(LABEL_TYPE, XBMC->GetLocalizedString(30054));
      break;
    case SCAN_SATELLITE:
      m_window->SetControlLabel(LABEL_TYPE, XBMC->GetLocalizedString(30055));
      break;
    case SCAN_PVRINPUT:
      m_window->SetControlLabel(LABEL_TYPE, XBMC->GetLocalizedString(30032));
      break;
    case SCAN_PVRINPUT_FM:
      m_window->SetControlLabel(LABEL_TYPE, XBMC->GetLocalizedString(30033));
      break;
    case SCAN_TERRCABLE_ATSC:
      m_window->SetControlLabel(LABEL_TYPE, XBMC->GetLocalizedString(30058));
      break;
  }
  m_window->ClearList();
  m_window->SetProperty("ScanActive", "true");

  cRequestPacket vrp;
  cResponsePacket* vresp = NULL;
  uint32_t retCode = VNSI_RET_ERROR;
  if (!vrp.init(VNSI_SCAN_START))                         goto SCANError;
  if (!vrp.add_U32(source))                               goto SCANError;
  if (!vrp.add_U8(m_radioButtonTV->IsSelected()))         goto SCANError;
  if (!vrp.add_U8(m_radioButtonRadio->IsSelected()))      goto SCANError;
  if (!vrp.add_U8(m_radioButtonFTA->IsSelected()))        goto SCANError;
  if (!vrp.add_U8(m_radioButtonScrambled->IsSelected()))  goto SCANError;
  if (!vrp.add_U8(m_radioButtonHD->IsSelected()))         goto SCANError;
  if (!vrp.add_U32(m_spinCountries->GetValue()))          goto SCANError;
  if (!vrp.add_U32(m_spinDVBCInversion->GetValue()))      goto SCANError;
  if (!vrp.add_U32(m_spinDVBCSymbolrates->GetValue()))    goto SCANError;
  if (!vrp.add_U32(m_spinDVBCqam->GetValue()))            goto SCANError;
  if (!vrp.add_U32(m_spinDVBTInversion->GetValue()))      goto SCANError;
  if (!vrp.add_U32(m_spinSatellites->GetValue()))         goto SCANError;
  if (!vrp.add_U32(m_spinATSCType->GetValue()))           goto SCANError;

  vresp = ReadResult(&vrp);
  if (!vresp)
    goto SCANError;

  retCode = vresp->extract_U32();
  if (retCode != VNSI_RET_OK)
    goto SCANError;

  return;

SCANError:
  XBMC->Log(LOG_ERROR, "%s - Return error after start (%i)", __FUNCTION__, retCode);
  m_window->SetControlLabel(LABEL_STATUS, XBMC->GetLocalizedString(24071));
  m_window->SetControlLabel(BUTTON_START, XBMC->GetLocalizedString(30024));
  m_window->SetControlLabel(HEADER_LABEL, XBMC->GetLocalizedString(30043));
  m_stopped = true;
}

void cVNSIChannelScan::StopScan()
{
  cRequestPacket vrp;
  if (!vrp.init(VNSI_SCAN_STOP))
    return;

  cResponsePacket* vresp = ReadResult(&vrp);
  if (!vresp)
    return;

  uint32_t retCode = vresp->extract_U32();
  if (retCode != VNSI_RET_OK)
  {
    XBMC->Log(LOG_ERROR, "%s - Return error after stop (%i)", __FUNCTION__, retCode);
    m_window->SetControlLabel(LABEL_STATUS, XBMC->GetLocalizedString(24071));
    m_window->SetControlLabel(BUTTON_START, XBMC->GetLocalizedString(30024));
    m_window->SetControlLabel(HEADER_LABEL, XBMC->GetLocalizedString(30043));
    m_stopped = true;
  }
  return;
}

void cVNSIChannelScan::ReturnFromProcessView()
{
  if (m_running)
  {
    m_running = false;
    m_window->ClearProperties();
    m_window->SetControlLabel(BUTTON_START, XBMC->GetLocalizedString(30010));
    m_window->SetControlLabel(HEADER_LABEL, XBMC->GetLocalizedString(30009));

    if (m_progressDone)
    {
      GUI->Control_releaseProgress(m_progressDone);
      m_progressDone = NULL;
    }
    if (m_progressSignal)
    {
      GUI->Control_releaseProgress(m_progressSignal);
      m_progressSignal = NULL;
    }
  }
}

void cVNSIChannelScan::SetProgress(int percent)
{
  if (!m_progressDone)
    m_progressDone = GUI->Control_getProgress(m_window, PROGRESS_DONE);

  std::stringstream header;
  header << XBMC->GetLocalizedString(30057);
  header << percent;
  header << " %";

  m_window->SetControlLabel(HEADER_LABEL, header.str().c_str());
  m_progressDone->SetPercentage((float)percent);
}

void cVNSIChannelScan::SetSignal(int percent, bool locked)
{
  if (!m_progressSignal)
    m_progressSignal = GUI->Control_getProgress(m_window, PROGRESS_SIGNAL);

  std::stringstream signal;
  signal << XBMC->GetLocalizedString(30056);
  signal << percent;
  signal << " %";

  m_window->SetControlLabel(LABEL_SIGNAL, signal.str().c_str());
  m_progressSignal->SetPercentage((float)percent);

  if (locked)
    m_window->SetProperty("Locked", "true");
  else
    m_window->SetProperty("Locked", "");
}

bool cVNSIChannelScan::ReadCountries()
{
  m_spinCountries = GUI->Control_getSpin(m_window, CONTROL_SPIN_COUNTRIES);
  m_spinCountries->Clear();

  std::locale loc;
  std::string country = XBMC->GetDVDMenuLanguage();
  for (std::string::size_type i = 0; i < country.length(); ++i)
    country[i] = std::toupper(country[i],loc);

  /*! @todo hardcoded change from country to language code
   * In addon lib must be the function added to get true country code instead
   * of language.
   */
  if (country == "EN")
    country = "GB";

  cRequestPacket vrp;
  if (!vrp.init(VNSI_SCAN_GETCOUNTRIES))
    return false;

  cResponsePacket* vresp = ReadResult(&vrp);
  if (!vresp)
    return false;

  int startIndex = -1;
  uint32_t retCode = vresp->extract_U32();
  if (retCode == VNSI_RET_OK)
  {
    while (!vresp->end())
    {
      uint32_t    index     = vresp->extract_U32();
      const char *isoName   = vresp->extract_String();
      const char *longName  = vresp->extract_String();
      m_spinCountries->AddLabel(longName, index);
      if (country == isoName)
        startIndex = index;

      delete[] longName;
      delete[] isoName;
    }
    if (startIndex >= 0)
      m_spinCountries->SetValue(startIndex);
  }
  else
  {
    XBMC->Log(LOG_ERROR, "%s - Return error after reading countries (%i)", __FUNCTION__, retCode);
  }

  delete vresp;
  return retCode == VNSI_RET_OK;
}

bool cVNSIChannelScan::ReadSatellites()
{
  m_spinSatellites = GUI->Control_getSpin(m_window, CONTROL_SPIN_SATELLITES);
  m_spinSatellites->Clear();

  cRequestPacket vrp;
  if (!vrp.init(VNSI_SCAN_GETSATELLITES))
    return false;

  cResponsePacket* vresp = ReadResult(&vrp);
  if (!vresp)
    return false;

  uint32_t retCode = vresp->extract_U32();
  if (retCode == VNSI_RET_OK)
  {
    while (!vresp->end())
    {
      uint32_t    index     = vresp->extract_U32();
      const char *shortName = vresp->extract_String();
      const char *longName  = vresp->extract_String();
      m_spinSatellites->AddLabel(longName, index);
      delete[] longName;
      delete[] shortName;
    }
    m_spinSatellites->SetValue(6);      /* default to Astra 19.2         */
  }
  else
  {
    XBMC->Log(LOG_ERROR, "%s - Return error after reading satellites (%i)", __FUNCTION__, retCode);
  }
  delete vresp;
  return retCode == VNSI_RET_OK;
}

uint32_t cVNSIChannelScan::ReadSupportedFlags()
{
  cRequestPacket vrp;
  if (!vrp.init(VNSI_SCAN_SUPPORTED_TYPES))
  {
    XBMC->Log(LOG_ERROR, "%s - Can't init cRequestPacket", __FUNCTION__);
    return false;
  }

  cResponsePacket* vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return false;
  }

  uint32_t ret = vresp->extract_U32();
  delete vresp;
  return ret;
}

void cVNSIChannelScan::SetControlsVisible(scantype_t type)
{
  m_spinCountries->SetVisible(type == SCAN_TERRESTRIAL || type == SCAN_CABLE || type == SCAN_PVRINPUT);
  m_spinSatellites->SetVisible(type == SCAN_SATELLITE || type == SCAN_TERRCABLE_ATSC);
  m_spinDVBCInversion->SetVisible(type == SCAN_CABLE);
  m_spinDVBCSymbolrates->SetVisible(type == SCAN_CABLE);
  m_spinDVBCqam->SetVisible(type == SCAN_CABLE);
  m_spinDVBTInversion->SetVisible(type == SCAN_TERRESTRIAL);
  m_spinATSCType->SetVisible(type == SCAN_TERRCABLE_ATSC);
  m_radioButtonTV->SetVisible(type == SCAN_TERRESTRIAL || type == SCAN_CABLE || type == SCAN_SATELLITE || type == SCAN_TERRCABLE_ATSC);
  m_radioButtonRadio->SetVisible(type == SCAN_TERRESTRIAL || type == SCAN_CABLE || type == SCAN_SATELLITE || type == SCAN_TERRCABLE_ATSC);
  m_radioButtonFTA->SetVisible(type == SCAN_TERRESTRIAL || type == SCAN_CABLE || type == SCAN_SATELLITE || type == SCAN_TERRCABLE_ATSC);
  m_radioButtonScrambled->SetVisible(type == SCAN_TERRESTRIAL || type == SCAN_CABLE || type == SCAN_SATELLITE || type == SCAN_TERRCABLE_ATSC);
  m_radioButtonHD->SetVisible(type == SCAN_TERRESTRIAL || type == SCAN_CABLE || type == SCAN_SATELLITE || type == SCAN_TERRCABLE_ATSC);
}

bool cVNSIChannelScan::OnResponsePacket(cResponsePacket* resp)
{
  m_ResponsePackets.push(resp);
  return true;
}
