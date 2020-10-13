/*
 *      Copyright (C) 2010-2020 Alwin Esch (Team Kodi)
 *      Copyright (C) 2010-2020 Team Kodi
 *      http://www.kodi.tv
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
 *  along with Kodi; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
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
