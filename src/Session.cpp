/*
 *  Copyright (C) 2010-2021 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Session.h"

#include "ClientInstance.h"
#include "RequestPacket.h"
#include "ResponsePacket.h"
#include "Settings.h"
#include "Tools.h"
#include "vnsicommand.h"

#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <kodi/addon-instance/inputstream/DemuxPacket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>

/* Needed on Mac OS/X */

#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

cVNSISession::cVNSISession(kodi::addon::CInstancePVRClient& instance) : m_instance(instance)
{
}

cVNSISession::~cVNSISession()
{
  Close();
}

void cVNSISession::Close()
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  if (m_socket)
  {
    m_socket->Shutdown();
    m_socket->Close();
  }

  delete m_socket;
  m_socket = nullptr;
}

bool cVNSISession::Open(const std::string& hostname, int port, const char* name)
{
  using namespace std::chrono;

  Close();

  auto iNow = system_clock::now();
  auto iTarget = iNow + milliseconds(CVNSISettings::Get().GetConnectTimeout() * 1000);

  if (!m_socket)
    m_socket = new vdrvnsi::TCPSocket(hostname, port);
  while (!m_socket->IsOpen() && iNow < iTarget && !m_abort)
  {
    uint64_t ms = duration_cast<milliseconds>(iTarget - iNow).count();
    if (!m_socket->Open(duration_cast<milliseconds>(iTarget - iNow).count()))
      std::this_thread::sleep_for(milliseconds(100));
    iNow = system_clock::now();
  }

  if (!m_socket->IsOpen() && !m_abort)
  {
    kodi::Log(ADDON_LOG_DEBUG, "%s - failed to connect to the backend", __func__);
    return false;
  }

  // store connection data
  m_hostname = hostname;
  m_port = port;

  if (name != nullptr)
    m_name = name;

  return true;
}

bool cVNSISession::Login()
{
  try
  {
    cRequestPacket vrp;
    vrp.init(VNSI_LOGIN);
    vrp.add_U32(VNSI_PROTOCOLVERSION);
    vrp.add_U8(false); // netlog
    if (!m_name.empty())
    {
      vrp.add_String(m_name.c_str());
    }
    else
    {
      vrp.add_String("XBMC Media Center");
    }

    // read welcome
    std::unique_ptr<cResponsePacket> vresp(ReadResult(&vrp));
    if (!vresp)
      throw "failed to read greeting from server";

    uint32_t protocol = vresp->extract_U32();
    uint32_t vdrTime = vresp->extract_U32();
    int32_t vdrTimeOffset = vresp->extract_S32();
    const char* serverName = vresp->extract_String();
    const char* serverVersion = vresp->extract_String();

    m_server = serverName;
    m_version = serverVersion;
    m_protocol = static_cast<int>(protocol);

    if (m_protocol < VNSI_MIN_PROTOCOLVERSION)
      throw "Protocol versions do not match";

    if (m_name.empty())
    {
      kodi::Log(ADDON_LOG_INFO,
                "Logged in at '%lu+%i' to '%s' Version: '%s' with protocol version '%d'", vdrTime,
                vdrTimeOffset, serverName, serverVersion, protocol);
    }
  }
  catch (const std::out_of_range& e)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, e.what());
    Close();
    return false;
  }
  catch (const char* str)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - %s", __func__, str);
    Close();
    return false;
  }

  return true;
}

std::unique_ptr<cResponsePacket> cVNSISession::ReadMessage(int iInitialTimeout /*= 10000*/,
                                                           int iDatapacketTimeout /*= 10000*/)
{
  uint32_t channelID = 0;
  uint32_t userDataLength = 0;
  uint8_t* userData = nullptr;

  cResponsePacket* vresp = nullptr;

  if (!ReadData(reinterpret_cast<uint8_t*>(&channelID), sizeof(uint32_t), iInitialTimeout))
    return nullptr;

  // Data was read

  channelID = ntohl(channelID);
  if (channelID == VNSI_CHANNEL_STREAM)
  {
    vresp = new cResponsePacket(m_instance);

    if (!ReadData(vresp->getHeader(), vresp->getStreamHeaderLength(), iDatapacketTimeout))
    {
      delete vresp;
      kodi::Log(ADDON_LOG_ERROR, "%s - lost sync on channel stream packet", __func__);
      SignalConnectionLost();
      return nullptr;
    }
    vresp->extractStreamHeader();
    userDataLength = vresp->getUserDataLength();

    if (vresp->getOpCodeID() == VNSI_STREAM_MUXPKT)
    {
      DEMUX_PACKET* p = m_instance.AllocateDemuxPacket(userDataLength);
      userData = (uint8_t*)p;
      if (userDataLength > 0)
      {
        if (!userData)
          return nullptr;
        if (!ReadData(p->pData, userDataLength, iDatapacketTimeout))
        {
          m_instance.FreeDemuxPacket(p);
          delete vresp;
          kodi::Log(ADDON_LOG_ERROR, "%s - lost sync on channel stream mux packet", __func__);
          SignalConnectionLost();
          return nullptr;
        }
      }
    }
    else if (userDataLength > 0)
    {
      userData = static_cast<uint8_t*>(malloc(userDataLength));
      if (!userData)
        return nullptr;
      if (!ReadData(userData, userDataLength, iDatapacketTimeout))
      {
        free(userData);
        delete vresp;
        kodi::Log(ADDON_LOG_ERROR, "%s - lost sync on channel stream (other) packet", __func__);
        SignalConnectionLost();
        return nullptr;
      }
    }
    vresp->setStream(userData, userDataLength);
  }
  else if (channelID == VNSI_CHANNEL_OSD)
  {
    vresp = new cResponsePacket(m_instance);

    if (!ReadData(vresp->getHeader(), vresp->getOSDHeaderLength(), iDatapacketTimeout))
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - lost sync on osd packet", __func__);
      SignalConnectionLost();
      return nullptr;
    }
    vresp->extractOSDHeader();
    userDataLength = vresp->getUserDataLength();

    userData = nullptr;
    if (userDataLength > 0)
    {
      userData = static_cast<uint8_t*>(malloc(userDataLength));
      if (!userData)
        return nullptr;
      if (!ReadData(userData, userDataLength, iDatapacketTimeout))
      {
        free(userData);
        delete vresp;
        kodi::Log(ADDON_LOG_ERROR, "%s - lost sync on additional osd packet", __func__);
        SignalConnectionLost();
        return nullptr;
      }
    }
    vresp->setOSD(userData, userDataLength);
  }
  else
  {
    vresp = new cResponsePacket(m_instance);

    if (!ReadData(vresp->getHeader(), vresp->getHeaderLength(), iDatapacketTimeout))
    {
      delete vresp;
      kodi::Log(ADDON_LOG_ERROR, "%s - lost sync on response packet", __func__);
      SignalConnectionLost();
      return nullptr;
    }
    vresp->extractHeader();
    userDataLength = vresp->getUserDataLength();

    userData = nullptr;
    if (userDataLength > 0)
    {
      userData = static_cast<uint8_t*>(malloc(userDataLength));
      if (!userData)
        return nullptr;
      if (!ReadData(userData, userDataLength, iDatapacketTimeout))
      {
        free(userData);
        delete vresp;
        kodi::Log(ADDON_LOG_ERROR, "%s - lost sync on additional response packet", __func__);
        SignalConnectionLost();
        return nullptr;
      }
    }

    if (channelID == VNSI_CHANNEL_STATUS)
      vresp->setStatus(userData, userDataLength);
    else
      vresp->setResponse(userData, userDataLength);
  }

  return std::unique_ptr<cResponsePacket>(vresp);
}

bool cVNSISession::TransmitMessage(cRequestPacket* vrp)
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (!IsOpen())
    return false;

  int64_t iWriteResult = m_socket->Write(vrp->getPtr(), vrp->getLen());
  if (iWriteResult != static_cast<int64_t>(vrp->getLen()))
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - Failed to write packet, bytes written: %d of total: %d",
              __func__, iWriteResult, vrp->getLen());
    return false;
  }
  return true;
}

std::unique_ptr<cResponsePacket> cVNSISession::ReadResult(cRequestPacket* vrp)
{
  if (!TransmitMessage(vrp))
  {
    SignalConnectionLost();
    return nullptr;
  }

  std::unique_ptr<cResponsePacket> pkt;

  while ((pkt = ReadMessage(10000, 10000)))
  {
    // Discard everything other as response packets until it is received
    if (pkt->getChannelID() == VNSI_CHANNEL_REQUEST_RESPONSE &&
        pkt->getRequestID() == vrp->getSerial())
    {
      return pkt;
    }
  }

  SignalConnectionLost();
  return nullptr;
}

bool cVNSISession::ReadSuccess(cRequestPacket* vrp)
{
  std::unique_ptr<cResponsePacket> pkt;
  if ((pkt = ReadResult(vrp)) == nullptr)
  {
    return false;
  }
  uint32_t retCode = pkt->extract_U32();

  if (retCode != VNSI_RET_OK)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - failed with error code '%i'", __func__, retCode);
    return false;
  }
  return true;
}

void cVNSISession::OnReconnect()
{
}

void cVNSISession::OnDisconnect()
{
}

cVNSISession::eCONNECTIONSTATE cVNSISession::TryReconnect()
{
  if (!Open(m_hostname, m_port))
    return CONN_HOST_NOT_REACHABLE;

  if (!Login())
    return CONN_LOGIN_FAILED;

  kodi::Log(ADDON_LOG_DEBUG, "%s - reconnected", __func__);
  m_connectionLost = false;

  OnReconnect();

  return CONN_ESABLISHED;
}

bool cVNSISession::IsOpen()
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  return m_socket && m_socket->IsOpen();
}

void cVNSISession::SignalConnectionLost()
{
  if (m_connectionLost)
    return;

  kodi::Log(ADDON_LOG_ERROR, "%s - connection lost !!!", __func__);

  m_connectionLost = true;
  Close();

  OnDisconnect();
}

bool cVNSISession::ReadData(uint8_t* buffer, int totalBytes, int timeout)
{
  int bytesRead = m_socket->Read(buffer, totalBytes, timeout);
  if (bytesRead == totalBytes)
  {
    return true;
  }
  else if (bytesRead > 0)
  {
    // we did read something. try to finish the read
    bytesRead += m_socket->Read(buffer + bytesRead, totalBytes - bytesRead, timeout);
    if (bytesRead == totalBytes)
      return true;
  }
  else if (m_socket->LastError() == vdrvnsi::SocketError::TimeOut)
  {
    return false;
  }

  SignalConnectionLost();
  return false;
}

void cVNSISession::SleepMs(int ms)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
