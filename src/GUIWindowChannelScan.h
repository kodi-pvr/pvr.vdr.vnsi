/*
 *  Copyright (C) 2010-2021 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "ClientInstance.h"

#include <atomic>
#include <kodi/gui/ListItem.h>
#include <kodi/gui/Window.h>
#include <kodi/gui/controls/Progress.h>
#include <kodi/gui/controls/RadioButton.h>
#include <kodi/gui/controls/Spin.h>
#include <map>
#include <string>
#include <thread>

typedef enum scantype
{
  DVB_TERR = 0,
  DVB_CABLE = 1,
  DVB_SAT = 2,
  PVRINPUT = 3,
  PVRINPUT_FM = 4,
  DVB_ATSC = 5,
} scantype_t;

class CPVRAddon;

class ATTR_DLL_LOCAL cVNSIChannelScan : public cVNSISession, public kodi::gui::CWindow
{
public:
  cVNSIChannelScan(kodi::addon::CInstancePVRClient& instance);
  ~cVNSIChannelScan() = default;

  bool Open(const std::string& hostname,
            int port,
            const std::string& mac,
            const char* name = "XBMC channel scanner");

  bool OnInit() override;
  bool OnFocus(int controlId) override;
  bool OnClick(int controlId) override;
  bool OnAction(ADDON_ACTION actionId) override;

protected:
  void Process();

private:
  bool ReadCountries();
  bool ReadSatellites();
  void SetControlsVisible(scantype_t type);
  void StartScan();
  void StopScan();
  void ReturnFromProcessView();
  void SetProgress(int procent);
  void SetSignal(int procent, bool locked);
  bool OnResponsePacket(cResponsePacket* resp);

  std::string m_header;
  std::string m_Signal;
  bool m_running = false;
  bool m_stopped = false;
  bool m_Canceled = false;
  bool m_progressActive = false;
  std::string m_wolMac;

  kodi::gui::controls::CSpin* m_spinSourceType = nullptr;
  kodi::gui::controls::CSpin* m_spinCountries = nullptr;
  kodi::gui::controls::CSpin* m_spinSatellites = nullptr;
  kodi::gui::controls::CSpin* m_spinDVBCInversion = nullptr;
  kodi::gui::controls::CSpin* m_spinDVBCSymbolrates = nullptr;
  kodi::gui::controls::CSpin* m_spinDVBCqam = nullptr;
  kodi::gui::controls::CSpin* m_spinDVBTInversion = nullptr;
  kodi::gui::controls::CSpin* m_spinATSCType = nullptr;
  kodi::gui::controls::CRadioButton* m_radioButtonTV = nullptr;
  kodi::gui::controls::CRadioButton* m_radioButtonRadio = nullptr;
  kodi::gui::controls::CRadioButton* m_radioButtonFTA = nullptr;
  kodi::gui::controls::CRadioButton* m_radioButtonScrambled = nullptr;
  kodi::gui::controls::CRadioButton* m_radioButtonHD = nullptr;
  kodi::gui::controls::CProgress* m_progressDone = nullptr;
  kodi::gui::controls::CProgress* m_progressSignal = nullptr;

  std::atomic<bool> m_threadRunning = {false};
  std::thread m_thread;  
};
