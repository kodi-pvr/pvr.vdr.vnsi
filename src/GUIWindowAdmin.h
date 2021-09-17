/*
 *  Copyright (C) 2010-2021 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "Channels.h"
#include "ClientInstance.h"

#include <kodi/gui/ListItem.h>
#include <kodi/gui/Window.h>
#include <kodi/gui/controls/RadioButton.h>
#include <kodi/gui/controls/Rendering.h>
#include <kodi/gui/controls/Spin.h>

class cOSDRender;

class ATTRIBUTE_HIDDEN cVNSIAdmin : public cVNSISession,
                                    public kodi::gui::CWindow
{
public:
  cVNSIAdmin(kodi::addon::CInstancePVRClient& instance);
  ~cVNSIAdmin() = default;

  bool Open(const std::string& hostname,
            int port,
            const std::string& mac,
            const char* name = "XBMC osd client");

  bool OnInit() override;
  bool OnFocus(int controlId) override;
  bool OnClick(int controlId) override;
  bool OnAction(ADDON_ACTION actionId) override;

  bool Create(int x, int y, int w, int h, void* device);
  void Render();
  void Stop();
  bool Dirty();

  static bool CreateCB(kodi::gui::ClientHandle cbhdl, int x, int y, int w, int h, void* device);
  static void RenderCB(kodi::gui::ClientHandle cbhdl);
  static void StopCB(kodi::gui::ClientHandle cbhdl);
  static bool DirtyCB(kodi::gui::ClientHandle cbhdl);

private:
  bool OnResponsePacket(cResponsePacket* resp);
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

  kodi::gui::controls::CRendering m_renderControl;
  kodi::gui::controls::CSpin m_spinTimeshiftMode;
  kodi::gui::controls::CSpin m_spinTimeshiftBufferRam;
  kodi::gui::controls::CSpin m_spinTimeshiftBufferFile;
  kodi::gui::controls::CRadioButton m_ratioIsRadio;
  std::vector<std::shared_ptr<kodi::gui::CListItem>> m_listItems;

  CVNSIChannels m_channels;
  bool m_bIsOsdControl;
  bool m_bIsOsdDirty = false;
  int m_width, m_height;
  int m_osdWidth, m_osdHeight;
  cOSDRender* m_osdRender = nullptr;
  std::string m_wolMac;
};
