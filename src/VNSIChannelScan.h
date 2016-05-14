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

#include "VNSIData.h"
#include "client.h"
#include <string>
#include <map>

#include <kodi/api2/gui/Window.hpp>
#include <kodi/api2/gui/ControlSpin.hpp>
#include <kodi/api2/gui/ControlRadioButton.hpp>
#include <kodi/api2/gui/ControlProgress.hpp>

typedef enum scantype
{
  DVB_TERR    = 0,
  DVB_CABLE   = 1,
  DVB_SAT     = 2,
  PVRINPUT    = 3,
  PVRINPUT_FM = 4,
  DVB_ATSC    = 5,
} scantype_t;


class cVNSIChannelScan : public cVNSIData, public KodiAPI::GUI::CWindow
{
public:

  cVNSIChannelScan();
  virtual ~cVNSIChannelScan();

  bool Open(const std::string& hostname, int port, const char* name = "XBMC channel scanner") override;

  bool OnClick(int controlId) override;
  bool OnFocus(int controlId) override;
  bool OnInit() override;
  bool OnAction(int actionId) override;

protected:

  bool OnResponsePacket(cResponsePacket* resp) override;

private:

  bool ReadCountries();
  bool ReadSatellites();
  void SetControlsVisible(scantype_t type);
  void StartScan();
  void StopScan();
  void ReturnFromProcessView();
  void SetProgress(int procent);
  void SetSignal(int procent, bool locked);

  std::string     m_header;
  std::string     m_Signal;
  bool            m_running;
  bool            m_stopped;
  bool            m_Canceled;

  KodiAPI::GUI::CControlSpin *m_spinSourceType;
  KodiAPI::GUI::CControlSpin *m_spinCountries;
  KodiAPI::GUI::CControlSpin *m_spinSatellites;
  KodiAPI::GUI::CControlSpin *m_spinDVBCInversion;
  KodiAPI::GUI::CControlSpin *m_spinDVBCSymbolrates;
  KodiAPI::GUI::CControlSpin *m_spinDVBCqam;
  KodiAPI::GUI::CControlSpin *m_spinDVBTInversion;
  KodiAPI::GUI::CControlSpin *m_spinATSCType;
  KodiAPI::GUI::CControlRadioButton *m_radioButtonTV;
  KodiAPI::GUI::CControlRadioButton *m_radioButtonRadio;
  KodiAPI::GUI::CControlRadioButton *m_radioButtonFTA;
  KodiAPI::GUI::CControlRadioButton *m_radioButtonScrambled;
  KodiAPI::GUI::CControlRadioButton *m_radioButtonHD;
  KodiAPI::GUI::CControlProgress *m_progressDone;
  KodiAPI::GUI::CControlProgress *m_progressSignal;
};
