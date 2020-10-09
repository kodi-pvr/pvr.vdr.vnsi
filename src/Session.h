/*
 *  Copyright (C) 2010-2020 Team Kodi
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "Socket.h"

#include <atomic>
#include <kodi/addon-instance/PVR.h>
#include <memory>
#include <mutex>
#include <p8-platform/sockets/tcp.h>
#include <stdint.h>
#include <string>

class cResponsePacket;
class cRequestPacket;

namespace P8PLATFORM
{
class CTcpConnection;
}

class ATTRIBUTE_HIDDEN cVNSISession
{
public:
  cVNSISession(kodi::addon::CInstancePVRClient& instance);
  virtual ~cVNSISession();

  virtual bool Open(const std::string& hostname, int port, const char* name = nullptr);
  virtual void Close();

  int GetProtocol() const { return m_protocol; }
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
  virtual bool Login();

  std::unique_ptr<cResponsePacket> ReadMessage(int iInitialTimeout, int iDatapacketTimeout);
  bool TransmitMessage(cRequestPacket* vrp);
  std::unique_ptr<cResponsePacket> ReadResult(cRequestPacket* vrp);
  bool ReadSuccess(cRequestPacket* m);
  void SleepMs(int ms);

  eCONNECTIONSTATE TryReconnect();
  bool IsOpen();
  virtual void OnDisconnect();
  virtual void OnReconnect();
  virtual void SignalConnectionLost();

  std::string m_hostname;
  int m_port;
  std::string m_name;
  std::mutex m_mutex;
  int m_protocol = 0;
  std::string m_server;
  std::string m_version;
  bool m_connectionLost = false;
  std::atomic_bool m_abort = {false};

private:
  bool ReadData(uint8_t* buffer, int totalBytes, int timeout);

  vdrvnsi::Socket* m_socket = nullptr;

  kodi::addon::CInstancePVRClient& m_instance;
};
