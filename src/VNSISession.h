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

#include <stdint.h>
#include <string>
#include <atomic>
#include "p8-platform/threads/threads.h"

#include <memory>

class cResponsePacket;
class cRequestPacket;

namespace P8PLATFORM
{
  class CTcpConnection;
}

class cVNSISession
{
public:
  cVNSISession();
  virtual ~cVNSISession();

  virtual bool      Open(const std::string& hostname, int port, const char *name = NULL);
  virtual bool      Login();
  virtual void      Close();

  std::unique_ptr<cResponsePacket> ReadMessage(int iInitialTimeout = 10000, int iDatapacketTimeout = 10000);
  bool              TransmitMessage(cRequestPacket* vrp);

  std::unique_ptr<cResponsePacket> ReadResult(cRequestPacket* vrp);
  bool              ReadSuccess(cRequestPacket* m);

  int                GetProtocol() const { return m_protocol; }
  const std::string& GetServerName() const { return m_server; }
  const std::string& GetVersion() const { return m_version; }

  enum eCONNECTIONSTATE
  {
    CONN_ESABLISHED = 0,
    CONN_HOST_NOT_REACHABLE,
    CONN_LOGIN_FAILED,
    CONN_UNKNOWN
  };

protected:

  void SleepMs(int ms);

  eCONNECTIONSTATE TryReconnect();
  bool IsOpen();

  virtual void OnDisconnect();
  virtual void OnReconnect();

  virtual void SignalConnectionLost();

  std::string      m_hostname;
  int              m_port;
  std::string      m_name;
  P8PLATFORM::CMutex m_mutex;
  int              m_protocol;
  std::string      m_server;
  std::string      m_version;
  bool m_connectionLost;
  std::atomic_bool m_abort;

private:

  bool readData(uint8_t* buffer, int totalBytes, int timeout);

  P8PLATFORM::CTcpConnection *m_socket;
  P8PLATFORM::CMutex          m_readMutex;
};
