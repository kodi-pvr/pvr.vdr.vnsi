/*
 *  Copyright (C) 2010-2020 Team Kodi
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "VNSIData.h"
#include "VNSIChannels.h"
#include "client.h"

class cOSDRender;

class cVNSIAdmin : public cVNSIData
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

  CAddonGUIWindow *m_window;

  CAddonGUIRenderingControl *m_renderControl;
  CAddonGUISpinControl *m_spinTimeshiftMode;
  CAddonGUISpinControl *m_spinTimeshiftBufferRam;
  CAddonGUISpinControl *m_spinTimeshiftBufferFile;
  CAddonGUIRadioButton *m_ratioIsRadio;
  std::vector<CAddonListItem*> m_listItems;
  std::map<GUIHANDLE, int> m_listItemsMap;
  std::map<GUIHANDLE, int> m_listItemsChannelsMap;
  CVNSIChannels m_channels;
  bool m_bIsOsdControl;
  bool m_bIsOsdDirty;
  int m_width, m_height;
  int m_osdWidth, m_osdHeight;
  cOSDRender *m_osdRender;
  P8PLATFORM::CMutex m_osdMutex;
};
