#pragma once

/*
 *      Copyright (C) 2005-2012 Team XBMC
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "VNSIData.h"
#include "VNSIChannels.h"
#include "client.h"

#include <kodi/api2/gui/Window.hpp>
#include <kodi/api2/gui/ControlSpin.hpp>
#include <kodi/api2/gui/ControlRadioButton.hpp>
#include <kodi/api2/gui/ControlRendering.hpp>
#include <kodi/api2/gui/ListItem.hpp>

class cOSDRender;

class cVNSIAdmin : public cVNSIData, public KodiAPI::GUI::CWindow
{
public:

  cVNSIAdmin();
  ~cVNSIAdmin();

  bool Open(const std::string& hostname, int port, const char* name = "XBMC osd client") override;

  bool OnClick(int controlId);
  bool OnFocus(int controlId);
  bool OnInit();
  bool OnAction(int actionId);

  bool Create(int x, int y, int w, int h, void* device);
  void Render();
  void Stop();
  bool Dirty();

  static bool OnClickCB(GUIHANDLE cbhdl, int controlId);
  static bool OnFocusCB(GUIHANDLE cbhdl, int controlId);
  static bool OnInitCB(GUIHANDLE cbhdl);
  static bool OnActionCB(GUIHANDLE cbhdl, int actionId);

  static bool CreateCB(GUIHANDLE cbhdl, int x, int y, int w, int h, void *device);
  static void RenderCB(GUIHANDLE cbhdl);
  static void StopCB(GUIHANDLE cbhdl);
  static bool DirtyCB(GUIHANDLE cbhdl);

protected:

  bool OnResponsePacket(cResponsePacket* resp) override;
  void OnDisconnect() override {};
  void OnReconnect() override {};
  bool ConnectOSD();
  bool IsVdrAction(int action);
  bool ReadChannelList(bool radio);
  bool ReadChannelWhitelist(bool radio);
  bool ReadChannelBlacklist(bool radio);
  bool SaveChannelWhitelist(bool radio);
  bool SaveChannelBlacklist(bool radio);
  void ClearListItems();
  void LoadListItemsProviders();
  void LoadListItemsChannels();

private:

  KodiAPI::GUI::CControlRendering *m_renderControl;
  KodiAPI::GUI::CControlSpin *m_spinTimeshiftMode;
  KodiAPI::GUI::CControlSpin *m_spinTimeshiftBufferRam;
  KodiAPI::GUI::CControlSpin *m_spinTimeshiftBufferFile;
  KodiAPI::GUI::CControlRadioButton *m_ratioIsRadio;
  std::vector<KodiAPI::GUI::CListItem*> m_listItems;
  std::map<KodiAPI::GUIHANDLE, int> m_listItemsMap;
  std::map<KodiAPI::GUIHANDLE, int> m_listItemsChannelsMap;
  CVNSIChannels m_channels;
  bool m_bIsOsdControl;
  bool m_bIsOsdDirty;
  int m_width, m_height;
  int m_osdWidth, m_osdHeight;
  cOSDRender *m_osdRender;
  P8PLATFORM::CMutex m_osdMutex;
};
