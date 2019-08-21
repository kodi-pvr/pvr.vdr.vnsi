cmake_minimum_required(VERSION 3.5)
project(pvr.vdr.vnsi)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

find_package(Kodi REQUIRED)
find_package(p8-platform REQUIRED)

add_definitions(-DUSE_DEMUX)

if(NOT WIN32)
  find_package(EMBEDDED)
  if((APP_RENDER_SYSTEM STREQUAL "gl" OR NOT APP_RENDER_SYSTEM) AND NOT EMBEDDED_FOUND)
    find_package(OpenGL)
  endif()

  if(OPENGL_FOUND)
    set(APP_RENDER_SYSTEM gl)
    set(INCLUDES ${OPENGL_INCLUDE_DIR})
    set(DEPLIBS ${OPENGL_LIBRARIES})
    set(VDR_SOURCES src/shaders/GUIShader.cpp
                    src/shaders/Matrix.cpp
                    src/shaders/Shader.cpp)
    add_definitions(-DHAS_GL)
    message(STATUS "VDR OSD support enabled via OpenGL")
  else()
    find_package(OpenGLES2)
    if(OPENGLES2_FOUND)
      set(APP_RENDER_SYSTEM gles)
      set(VDR_SOURCES src/shaders/GUIShader.cpp
                      src/shaders/Matrix.cpp
                      src/shaders/Shader.cpp)
      add_definitions(-DHAS_GLES2)
      set(INCLUDES ${OPENGLES2_INCLUDE_DIRS})
      set(DEPLIBS ${OPENGLES2_LIBRARIES})
      message(STATUS "VDR OSD support enabled via OpenGLES")
    else()
      message(STATUS " VDR OSD support disabled because OpenGL(ES) wasn't found")
    endif()
  endif()
else()
  message(STATUS " VDR OSD support disabled because it is not migrated to DX11 yet")
endif()

list(APPEND VDR_SOURCES src/client.cpp
                        src/requestpacket.cpp
                        src/responsepacket.cpp
                        src/tools.cpp
                        src/VNSIAdmin.cpp
                        src/VNSIChannels.cpp
                        src/VNSIChannelScan.cpp
                        src/VNSIData.cpp
                        src/VNSIDemux.cpp
                        src/VNSIRecording.cpp
                        src/VNSISession.cpp)

list(APPEND VDR_HEADERS src/client.h
                        src/requestpacket.h
                        src/responsepacket.h
                        src/tools.h
                        src/VNSIAdmin.h
                        src/VNSIChannelScan.h
                        src/VNSIChannels.h
                        src/vnsicommand.h
                        src/VNSIData.h
                        src/VNSIDemux.h
                        src/VNSIRecording.h
                        src/VNSISession.h)

list(APPEND DEPLIBS ${p8-platform_LIBRARIES})
if(WIN32)
  set(APP_RENDER_SYSTEM dx11)
  list(APPEND DEPLIBS ws2_32)
  add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
endif()

message(STATUS "Configured render system: ${APP_RENDER_SYSTEM}")

include_directories(${INCLUDES}
                    ${p8-platform_INCLUDE_DIRS}
                    ${KODI_INCLUDE_DIR})

build_addon(pvr.vdr.vnsi VDR DEPLIBS)

include(CPack)
