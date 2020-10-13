cmake_minimum_required(VERSION 3.5)
project(pvr.vdr.vnsi)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

find_package(Kodi REQUIRED)

set(CMAKE_CXX_STANDARD 17)

if(NOT WIN32 AND (APP_RENDER_SYSTEM STREQUAL "gl" OR NOT APP_RENDER_SYSTEM))
  find_package(OpenGl REQUIRED)
  set(DEPLIBS ${OPENGL_LIBRARIES})
  set(INCLUDES ${OPENGL_INCLUDE_DIR})
  add_definitions(${OPENGL_DEFINITIONS})
else()
  find_package(OpenGLES REQUIRED)
  set(DEPLIBS ${OPENGLES_LIBRARIES})
  set(INCLUDES ${OPENGLES_INCLUDE_DIR})
  add_definitions(${OPENGLES_DEFINITIONS})
endif()

include_directories(${INCLUDES}
                    ${PROJECT_SOURCE_DIR}/lib
                    ${KODI_INCLUDE_DIR}/..) # Hack way with "/..", need bigger Kodi cmake rework to match right include ways

list(APPEND VDR_SOURCES src/Addon.cpp
                        src/Channels.cpp
                        src/ClientInstance.cpp
                        src/GUIWindowAdmin.cpp
                        src/GUIWindowChannelScan.cpp
                        src/InputstreamDemux.cpp
                        src/InputstreamRecording.cpp
                        src/RequestPacket.cpp
                        src/ResponsePacket.cpp
                        src/Session.cpp
                        src/Settings.cpp
                        src/TCPSocket.cpp
                        src/Tools.cpp)

list(APPEND VDR_HEADERS src/Addon.h
                        src/Channels.h
                        src/ClientInstance.h
                        src/GUIWindowAdmin.h
                        src/GUIWindowChannelScan.h
                        src/InputstreamDemux.h
                        src/InputstreamRecording.h
                        src/RequestPacket.h
                        src/ResponsePacket.h
                        src/Session.h
                        src/Settings.h
                        src/TCPSocket.h
                        src/Tools.h
                        src/vnsicommand.h)

build_addon(pvr.vdr.vnsi VDR DEPLIBS)

include(CPack)
