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

#include "GUIWindowAdmin.h"

#include "RequestPacket.h"
#include "ResponsePacket.h"
#include "vnsicommand.h"

#include <kodi/General.h>
#include <kodi/Network.h>
#include <kodi/gui/gl/GL.h>
#include <kodi/gui/gl/Shader.h>
#include <queue>
#include <stdio.h>

#if !defined(GL_UNPACK_ROW_LENGTH)
#if defined(GL_UNPACK_ROW_LENGTH_EXT)
#define GL_UNPACK_ROW_LENGTH GL_UNPACK_ROW_LENGTH_EXT
#define GL_UNPACK_SKIP_ROWS GL_UNPACK_SKIP_ROWS_EXT
#define GL_UNPACK_SKIP_PIXELS GL_UNPACK_SKIP_PIXELS_EXT
#else
#undef HAS_GLES
#endif
#endif

#define CONTROL_RENDER_ADDON 9
#define CONTROL_MENU 10
#define CONTROL_OSD_BUTTON 13
#define CONTROL_SPIN_TIMESHIFT_MODE 21
#define CONTROL_SPIN_TIMESHIFT_BUFFER_RAM 22
#define CONTROL_SPIN_TIMESHIFT_BUFFER_FILE 23
#define CONTROL_LABEL_FILTERS 31
#define CONTROL_RADIO_ISRADIO 32
#define CONTROL_PROVIDERS_BUTTON 33
#define CONTROL_CHANNELS_BUTTON 34
#define CONTROL_FILTERSAVE_BUTTON 35
#define CONTROL_ITEM_LIST 36

class cOSDTexture
{
public:
  cOSDTexture(int bpp, int x0, int y0, int x1, int y1);
  virtual ~cOSDTexture();
  void SetPalette(int numColors, uint32_t* colors);
  void SetBlock(int x0, int y0, int x1, int y1, int stride, void* data, int len);
  void Clear();
  void GetSize(int& width, int& height);
  void GetOrigin(int& x0, int& y0)
  {
    x0 = m_x0;
    y0 = m_y0;
  };
  bool IsDirty(int& x0, int& y0, int& x1, int& y1);
  void* GetBuffer() { return (void*)m_buffer; };

protected:
  int m_x0, m_x1, m_y0, m_y1;
  int m_dirtyX0, m_dirtyX1, m_dirtyY0, m_dirtyY1;
  int m_bpp;
  int m_numColors;
  uint32_t m_palette[256];
  uint8_t* m_buffer;
  bool m_dirty;
};

cOSDTexture::cOSDTexture(int bpp, int x0, int y0, int x1, int y1)
{
  m_bpp = bpp;
  m_x0 = x0;
  m_x1 = x1;
  m_y0 = y0;
  m_y1 = y1;
  m_buffer = new uint8_t[(x1 - x0 + 1) * (y1 - y0 + 1) * sizeof(uint32_t)];
  memset(m_buffer, 0, (x1 - x0 + 1) * (y1 - y0 + 1) * sizeof(uint32_t));
  m_dirtyX0 = m_dirtyY0 = 0;
  m_dirtyX1 = x1 - x0;
  m_dirtyY1 = y1 - y0;
  m_dirty = false;
}

cOSDTexture::~cOSDTexture()
{
  delete[] m_buffer;
}

void cOSDTexture::Clear()
{
  memset(m_buffer, 0, (m_x1 - m_x0 + 1) * (m_y1 - m_y0 + 1) * sizeof(uint32_t));
  m_dirtyX0 = m_dirtyY0 = 0;
  m_dirtyX1 = m_x1 - m_x0;
  m_dirtyY1 = m_y1 - m_y0;
  m_dirty = false;
}

void cOSDTexture::SetBlock(int x0, int y0, int x1, int y1, int stride, void* data, int len)
{
  int line = y0;
  int col;
  int color;
  int width = m_x1 - m_x0 + 1;
  uint8_t* dataPtr = (uint8_t*)data;
  int pos = 0;
  uint32_t* buffer = (uint32_t*)m_buffer;
  while (line <= y1)
  {
    int lastPos = pos;
    col = x0;
    int offset = line * width;
    while (col <= x1)
    {
      if (pos >= len)
      {
        kodi::Log(ADDON_LOG_ERROR, "cOSDTexture::SetBlock: reached unexpected end of buffer");
        return;
      }
      color = dataPtr[pos];
      if (m_bpp == 8)
      {
        buffer[offset + col] = m_palette[color];
      }
      else if (m_bpp == 4)
      {
        buffer[offset + col] = m_palette[color & 0x0F];
      }
      else if (m_bpp == 2)
      {
        buffer[offset + col] = m_palette[color & 0x03];
      }
      else if (m_bpp == 1)
      {
        buffer[offset + col] = m_palette[color & 0x01];
      }
      pos++;
      col++;
    }
    line++;
    pos = lastPos + stride;
  }
  if (x0 < m_dirtyX0)
    m_dirtyX0 = x0;
  if (x1 > m_dirtyX1)
    m_dirtyX1 = x1;
  if (y0 < m_dirtyY0)
    m_dirtyY0 = y0;
  if (y1 > m_dirtyY1)
    m_dirtyY1 = y1;
  m_dirty = true;
}

void cOSDTexture::SetPalette(int numColors, uint32_t* colors)
{
  m_numColors = numColors;
  for (int i = 0; i < m_numColors; i++)
  {
    // convert from ARGB to RGBA
    m_palette[i] = ((colors[i] & 0xFF000000)) | ((colors[i] & 0x00FF0000) >> 16) |
                   ((colors[i] & 0x0000FF00)) | ((colors[i] & 0x000000FF) << 16);
  }
}

void cOSDTexture::GetSize(int& width, int& height)
{
  width = m_x1 - m_x0 + 1;
  height = m_y1 - m_y0 + 1;
}

bool cOSDTexture::IsDirty(int& x0, int& y0, int& x1, int& y1)
{
  bool ret = m_dirty;
  x0 = m_dirtyX0;
  x1 = m_dirtyX1;
  y0 = m_dirtyY0;
  y1 = m_dirtyY1;
  m_dirty = false;
  return ret;
}
//-----------------------------------------------------------------------------
#define MAX_TEXTURES 16

class cOSDRender
{
public:
  cOSDRender();
  virtual ~cOSDRender();
  void SetOSDSize(int width, int height)
  {
    m_osdWidth = width;
    m_osdHeight = height;
  };
  void SetControlSize(int width, int height)
  {
    m_controlWidth = width;
    m_controlHeight = height;
  };
  void AddTexture(int wndId, int bpp, int x0, int y0, int x1, int y1, int reset);
  void SetPalette(int wndId, int numColors, uint32_t* colors);
  void SetBlock(int wndId, int x0, int y0, int x1, int y1, int stride, void* data, int len);
  void Clear(int wndId);
  virtual void DisposeTexture(int wndId);
  virtual void FreeResources();
  virtual void Render(){};
  virtual void SetDevice(void* device){};
  virtual bool Init() { return true; };

protected:
  cOSDTexture* m_osdTextures[MAX_TEXTURES];
  std::queue<cOSDTexture*> m_disposedTextures;
  int m_osdWidth, m_osdHeight;
  int m_controlWidth, m_controlHeight;
};

cOSDRender::cOSDRender()
{
  for (int i = 0; i < MAX_TEXTURES; i++)
    m_osdTextures[i] = 0;
}

cOSDRender::~cOSDRender()
{
  for (int i = 0; i < MAX_TEXTURES; i++)
  {
    DisposeTexture(i);
  }
  FreeResources();
}

void cOSDRender::DisposeTexture(int wndId)
{
  if (m_osdTextures[wndId])
  {
    m_disposedTextures.push(m_osdTextures[wndId]);
    m_osdTextures[wndId] = 0;
  }
}

void cOSDRender::FreeResources()
{
  while (!m_disposedTextures.empty())
  {
    delete m_disposedTextures.front();
    m_disposedTextures.pop();
  }
}

void cOSDRender::AddTexture(int wndId, int bpp, int x0, int y0, int x1, int y1, int reset)
{
  if (reset)
    DisposeTexture(wndId);
  if (!m_osdTextures[wndId])
    m_osdTextures[wndId] = new cOSDTexture(bpp, x0, y0, x1, y1);
}

void cOSDRender::Clear(int wndId)
{
  if (m_osdTextures[wndId])
    m_osdTextures[wndId]->Clear();
}

void cOSDRender::SetPalette(int wndId, int numColors, uint32_t* colors)
{
  if (m_osdTextures[wndId])
    m_osdTextures[wndId]->SetPalette(numColors, colors);
}

void cOSDRender::SetBlock(
    int wndId, int x0, int y0, int x1, int y1, int stride, void* data, int len)
{
  if (m_osdTextures[wndId])
    m_osdTextures[wndId]->SetBlock(x0, y0, x1, y1, stride, data, len);
}

//-----------------------------------------------------------------------------

#if defined(HAS_GL) || defined(HAS_GLES)

class ATTRIBUTE_HIDDEN cOSDRenderGL : public cOSDRender, public kodi::gui::gl::CShaderProgram
{
public:
  cOSDRenderGL();
  virtual ~cOSDRenderGL();
  void DisposeTexture(int wndId) override;
  void FreeResources() override;
  void Render() override;
  bool Init() override;

  void OnCompiledAndLinked() override;

protected:
  GLuint m_hwTextures[MAX_TEXTURES];
  std::queue<GLuint> m_disposedHwTextures;

  const GLubyte m_idx[4] = {0, 1, 3, 2};

#if defined(HAS_GL)
  GLuint m_vertexVBO = 0;
  GLuint m_indexVBO = 0;
#endif

  GLint m_aPosition = -1;
  GLint m_aCoord = -1;
};

cOSDRenderGL::cOSDRenderGL()
{
  for (int i = 0; i < MAX_TEXTURES; i++)
    m_hwTextures[i] = 0;
}

cOSDRenderGL::~cOSDRenderGL()
{
  for (int i = 0; i < MAX_TEXTURES; i++)
  {
    DisposeTexture(i);
  }
  FreeResources();

#if defined(HAS_GL)
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDeleteBuffers(1, &m_vertexVBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glDeleteBuffers(1, &m_indexVBO);
#endif
}

bool cOSDRenderGL::Init()
{
  std::string fraqShader = kodi::GetAddonPath("resources/shaders/" GL_TYPE_STRING "/frag.glsl");
  std::string vertShader = kodi::GetAddonPath("resources/shaders/" GL_TYPE_STRING "/vert.glsl");
  if (!LoadShaderFiles(vertShader, fraqShader) || !CompileAndLink())
    return false;

#if defined(HAS_GL)
  glGenBuffers(1, &m_vertexVBO);
  glGenBuffers(1, &m_indexVBO);
#endif

  return true;
}

void cOSDRenderGL::DisposeTexture(int wndId)
{
  if (m_hwTextures[wndId])
  {
    m_disposedHwTextures.push(m_hwTextures[wndId]);
    m_hwTextures[wndId] = 0;
  }
  cOSDRender::DisposeTexture(wndId);
}

void cOSDRenderGL::FreeResources()
{
  while (!m_disposedHwTextures.empty())
  {
    if (glIsTexture(m_disposedHwTextures.front()))
    {
      glFinish();
      glDeleteTextures(1, &m_disposedHwTextures.front());
      m_disposedHwTextures.pop();
    }
  }
  cOSDRender::FreeResources();
}

void cOSDRenderGL::Render()
{
  glDisable(GL_BLEND);

  for (int i = 0; i < MAX_TEXTURES; i++)
  {
    int width, height, offsetX, offsetY;
    int x0, x1, y0, y1;
    bool dirty;

    if (m_osdTextures[i] == 0)
      continue;

    m_osdTextures[i]->GetSize(width, height);
    m_osdTextures[i]->GetOrigin(offsetX, offsetY);
    dirty = m_osdTextures[i]->IsDirty(x0, y0, x1, y1);

    // create gl texture
    if (dirty && !glIsTexture(m_hwTextures[i]))
    {
      glGenTextures(1, &m_hwTextures[i]);
      glBindTexture(GL_TEXTURE_2D, m_hwTextures[i]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, width);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                   m_osdTextures[i]->GetBuffer());
      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
    // update texture
    else if (dirty)
    {
      glBindTexture(GL_TEXTURE_2D, m_hwTextures[i]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, width);
      glPixelStorei(GL_UNPACK_SKIP_PIXELS, x0);
      glPixelStorei(GL_UNPACK_SKIP_ROWS, y0);
      glTexSubImage2D(GL_TEXTURE_2D, 0, x0, y0, x1 - x0 + 1, y1 - y0 + 1, GL_RGBA, GL_UNSIGNED_BYTE,
                      m_osdTextures[i]->GetBuffer());
      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }

    // render texture

    // calculate ndc for OSD texture
    float destX0 = (float)offsetX * 2 / m_osdWidth - 1;
    float destX1 = (float)(offsetX + width) * 2 / m_osdWidth - 1;
    float destY0 = (float)offsetY * 2 / m_osdHeight - 1;
    float destY1 = (float)(offsetY + height) * 2 / m_osdHeight - 1;
    float aspectControl = (float)m_controlWidth / m_controlHeight;
    float aspectOSD = (float)m_osdWidth / m_osdHeight;
    if (aspectOSD > aspectControl)
    {
      destY0 *= aspectControl / aspectOSD;
      destY1 *= aspectControl / aspectOSD;
    }
    else if (aspectOSD < aspectControl)
    {
      destX0 *= aspectOSD / aspectControl;
      destX1 *= aspectOSD / aspectControl;
    }

    // y inveted
    destY0 *= -1;
    destY1 *= -1;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hwTextures[i]);

    EnableShader();

#if defined(HAS_GL)
    struct PackedVertex
    {
      float x, y, z;
      float u1, v1;
    } vertex[4];

    vertex[0].x = destX0;
    vertex[0].y = destY0;
    vertex[0].z = 0.0f;
    vertex[0].u1 = 0.0f;
    vertex[0].v1 = 0.0f;

    vertex[1].x = destX1;
    vertex[1].y = destY0;
    vertex[1].z = 0.0f;
    vertex[1].u1 = 1.0f;
    vertex[1].v1 = 0.0f;

    vertex[2].x = destX1;
    vertex[2].y = destY1;
    vertex[2].z = 0.0f;
    vertex[2].u1 = 1.0f;
    vertex[2].v1 = 1.0f;

    vertex[3].x = destX0;
    vertex[3].y = destY1;
    vertex[3].z = 0.0f;
    vertex[3].u1 = 0.0f;
    vertex[3].v1 = 1.0f;

    glBindBuffer(GL_ARRAY_BUFFER, m_vertexVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(PackedVertex) * 4, &vertex[0], GL_STATIC_DRAW);

    glVertexAttribPointer(m_aPosition, 3, GL_FLOAT, GL_FALSE, sizeof(PackedVertex),
                          BUFFER_OFFSET(offsetof(PackedVertex, x)));
    glEnableVertexAttribArray(m_aPosition);

    glVertexAttribPointer(m_aCoord, 2, GL_FLOAT, GL_FALSE, sizeof(PackedVertex),
                          BUFFER_OFFSET(offsetof(PackedVertex, u1)));
    glEnableVertexAttribArray(m_aCoord);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexVBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLubyte) * 4, m_idx, GL_STATIC_DRAW);

    glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_BYTE, 0);

#elif defined(HAS_GLES)

    GLfloat ver[4][4];
    GLfloat tex[4][2];

    glVertexAttribPointer(m_aPosition, 4, GL_FLOAT, GL_FALSE, 0, ver);
    glEnableVertexAttribArray(m_aPosition);

    glVertexAttribPointer(m_aCoord, 2, GL_FLOAT, GL_FALSE, 0, tex);
    glEnableVertexAttribArray(m_aCoord);

    // Set vertex coordinates
    for (int i = 0; i < 4; i++)
    {
      ver[i][2] = 0.0f; // set z to 0
      ver[i][3] = 1.0f;
    }
    ver[0][0] = ver[3][0] = destX0;
    ver[0][1] = ver[1][1] = destY0;
    ver[1][0] = ver[2][0] = destX1;
    ver[2][1] = ver[3][1] = destY1;

    // Set texture coordinates
    tex[0][0] = tex[3][0] = 0;
    tex[0][1] = tex[1][1] = 0;
    tex[1][0] = tex[2][0] = 1;
    tex[2][1] = tex[3][1] = 1;

    glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_BYTE, m_idx);
#endif

    DisableShader();

    glDisableVertexAttribArray(m_aPosition);
    glDisableVertexAttribArray(m_aCoord);

    glBindTexture(GL_TEXTURE_2D, 0);
  }
}

void cOSDRenderGL::OnCompiledAndLinked()
{
  m_aPosition = glGetAttribLocation(ProgramHandle(), "a_pos");
  m_aCoord = glGetAttribLocation(ProgramHandle(), "a_coord");
}

#endif // #if defined(HAS_GL) || defined(HAS_GLES)

//-----------------------------------------------------------------------------

cVNSIAdmin::cVNSIAdmin(kodi::addon::CInstancePVRClient& instance)
  : cVNSISession(instance), kodi::gui::CWindow("Admin.xml", "skin.estuary", true, false)
{
}

bool cVNSIAdmin::Open(const std::string& hostname,
                      int port,
                      const std::string& mac,
                      const char* name)
{
  m_hostname = hostname;
  m_port = port;
  m_wolMac = mac;

  if (!cVNSISession::Open(m_hostname, m_port, name))
    return false;

  if (!cVNSISession::Login())
    return false;

  m_bIsOsdControl = false;
#if defined(HAS_GL) || defined(HAS_GLES2)
  m_osdRender = new cOSDRenderGL();
#else
  m_osdRender = new cOSDRender();
#endif

  if (!m_osdRender->Init())
  {
    delete m_osdRender;
    m_osdRender = nullptr;
    return false;
  }

  m_abort = false;
  m_connectionLost = false;
  CreateThread();

  if (!ConnectOSD())
    return false;

  kodi::gui::CWindow::DoModal();

  ClearListItems();
  ClearProperties();

  delete m_renderControl;
  delete m_spinTimeshiftMode;
  delete m_spinTimeshiftBufferRam;
  delete m_spinTimeshiftBufferFile;
  delete m_ratioIsRadio;

  StopThread();
  kodi::gui::CWindow::Close();

  if (m_osdRender)
  {
    delete m_osdRender;
    m_osdRender = nullptr;
  }

  return true;
}

bool cVNSIAdmin::IsVdrAction(int action)
{
  if (action == ADDON_ACTION_MOVE_LEFT || action == ADDON_ACTION_MOVE_RIGHT ||
      action == ADDON_ACTION_MOVE_UP || action == ADDON_ACTION_MOVE_DOWN ||
      action == ADDON_ACTION_SELECT_ITEM || action == ADDON_ACTION_PREVIOUS_MENU ||
      action == ADDON_ACTION_REMOTE_0 || action == ADDON_ACTION_REMOTE_1 ||
      action == ADDON_ACTION_REMOTE_2 || action == ADDON_ACTION_REMOTE_3 ||
      action == ADDON_ACTION_REMOTE_4 || action == ADDON_ACTION_REMOTE_5 ||
      action == ADDON_ACTION_REMOTE_6 || action == ADDON_ACTION_REMOTE_7 ||
      action == ADDON_ACTION_REMOTE_8 || action == ADDON_ACTION_REMOTE_9 ||
      action == ADDON_ACTION_NAV_BACK || action == ADDON_ACTION_TELETEXT_RED ||
      action == ADDON_ACTION_TELETEXT_GREEN || action == ADDON_ACTION_TELETEXT_YELLOW ||
      action == ADDON_ACTION_TELETEXT_BLUE)
    return true;
  else
    return false;
}

bool cVNSIAdmin::OnInit()
{
  cRequestPacket vrp;
  vrp.init(VNSI_OSD_HITKEY);
  vrp.add_U32(0);
  cVNSISession::TransmitMessage(&vrp);

  // setup parameters
  m_spinTimeshiftMode = new kodi::gui::controls::CSpin(this, CONTROL_SPIN_TIMESHIFT_MODE);
  m_spinTimeshiftMode->SetType(kodi::gui::controls::ADDON_SPIN_CONTROL_TYPE_TEXT);
  m_spinTimeshiftMode->SetIntRange(0, 2);
  m_spinTimeshiftMode->AddLabel("OFF", 0);
  m_spinTimeshiftMode->AddLabel("RAM", 1);
  m_spinTimeshiftMode->AddLabel("FILE", 2);

  {
    cRequestPacket vrp;
    vrp.init(VNSI_GETSETUP);
    vrp.add_String(CONFNAME_TIMESHIFT);
    auto resp = ReadResult(&vrp);
    if (!resp)
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - failed to get timeshift mode", __func__);
      return false;
    }
    int mode = resp->extract_U32();
    m_spinTimeshiftMode->SetIntValue(mode);
  }

  m_spinTimeshiftBufferRam =
      new kodi::gui::controls::CSpin(this, CONTROL_SPIN_TIMESHIFT_BUFFER_RAM);
  m_spinTimeshiftBufferRam->SetType(kodi::gui::controls::ADDON_SPIN_CONTROL_TYPE_INT);
  m_spinTimeshiftBufferRam->SetIntRange(1, 80);

  {
    cRequestPacket vrp;
    vrp.init(VNSI_GETSETUP);
    vrp.add_String(CONFNAME_TIMESHIFTBUFFERSIZE);
    auto resp = ReadResult(&vrp);
    if (!resp)
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - failed to get timeshift buffer size", __func__);
      return false;
    }
    int mode = resp->extract_U32();
    m_spinTimeshiftBufferRam->SetIntValue(mode);
  }

  m_spinTimeshiftBufferFile =
      new kodi::gui::controls::CSpin(this, CONTROL_SPIN_TIMESHIFT_BUFFER_FILE);
  m_spinTimeshiftBufferFile->SetType(kodi::gui::controls::ADDON_SPIN_CONTROL_TYPE_INT);
  m_spinTimeshiftBufferFile->SetIntRange(1, 20);

  {
    cRequestPacket vrp;
    vrp.init(VNSI_GETSETUP);
    vrp.add_String(CONFNAME_TIMESHIFTBUFFERFILESIZE);
    auto resp = ReadResult(&vrp);
    if (!resp)
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - failed to get timeshift buffer (file) size", __func__);
      return false;
    }
    int mode = resp->extract_U32();
    m_spinTimeshiftBufferFile->SetIntValue(mode);
  }

  m_ratioIsRadio = new kodi::gui::controls::CRadioButton(this, CONTROL_RADIO_ISRADIO);
  m_renderControl = new kodi::gui::controls::CRendering(this, CONTROL_RENDER_ADDON);
  m_renderControl->SetIndependentCallbacks(this, CreateCB, RenderCB, StopCB, DirtyCB);

  return true;
}

bool cVNSIAdmin::OnFocus(int controlId)
{
  if (controlId == CONTROL_OSD_BUTTON)
  {
    SetControlLabel(CONTROL_OSD_BUTTON, kodi::GetLocalizedString(30102));
    MarkDirtyRegion();
    m_bIsOsdControl = true;
    return true;
  }
  else if (m_bIsOsdControl)
  {
    SetControlLabel(CONTROL_OSD_BUTTON, kodi::GetLocalizedString(30103));
    MarkDirtyRegion();
    m_bIsOsdControl = false;
    return true;
  }
  return false;
}

bool cVNSIAdmin::OnClick(int controlId)
{
  if (controlId == CONTROL_SPIN_TIMESHIFT_MODE)
  {
    int value = m_spinTimeshiftMode->GetIntValue();
    cRequestPacket vrp;
    vrp.init(VNSI_STORESETUP);
    vrp.add_String(CONFNAME_TIMESHIFT);
    vrp.add_U32(value);
    if (!ReadSuccess(&vrp))
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - failed to set timeshift mode", __func__);
    }
    return true;
  }
  else if (controlId == CONTROL_SPIN_TIMESHIFT_BUFFER_RAM)
  {
    int value = m_spinTimeshiftBufferRam->GetIntValue();
    cRequestPacket vrp;
    vrp.init(VNSI_STORESETUP);
    vrp.add_String(CONFNAME_TIMESHIFTBUFFERSIZE);
    vrp.add_U32(value);
    if (!ReadSuccess(&vrp))
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - failed to set timeshift buffer", __func__);
    }
    return true;
  }
  else if (controlId == CONTROL_SPIN_TIMESHIFT_BUFFER_FILE)
  {
    int value = m_spinTimeshiftBufferFile->GetIntValue();
    cRequestPacket vrp;
    vrp.init(VNSI_STORESETUP);
    vrp.add_String(CONFNAME_TIMESHIFTBUFFERFILESIZE);
    vrp.add_U32(value);
    if (!ReadSuccess(&vrp))
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - failed to set timeshift buffer file", __func__);
    }
    return true;
  }
  else if (controlId == CONTROL_PROVIDERS_BUTTON)
  {
    if (!m_channels.m_loaded || m_ratioIsRadio->IsSelected() != m_channels.m_radio)
    {
      ReadChannelList(m_ratioIsRadio->IsSelected());
      ReadChannelWhitelist(m_ratioIsRadio->IsSelected());
      ReadChannelBlacklist(m_ratioIsRadio->IsSelected());
      m_channels.CreateProviders();
      m_channels.LoadProviderWhitelist();
      m_channels.LoadChannelBlacklist();
      m_channels.m_loaded = true;
      m_channels.m_radio = m_ratioIsRadio->IsSelected();
      SetProperty("IsDirty", "0");
    }
    LoadListItemsProviders();
    m_channels.m_mode = CVNSIChannels::PROVIDER;
  }
  else if (controlId == CONTROL_CHANNELS_BUTTON)
  {
    if (!m_channels.m_loaded || m_ratioIsRadio->IsSelected() != m_channels.m_radio)
    {
      ReadChannelList(m_ratioIsRadio->IsSelected());
      ReadChannelWhitelist(m_ratioIsRadio->IsSelected());
      ReadChannelBlacklist(m_ratioIsRadio->IsSelected());
      m_channels.CreateProviders();
      m_channels.LoadProviderWhitelist();
      m_channels.LoadChannelBlacklist();
      m_channels.m_loaded = true;
      m_channels.m_radio = m_ratioIsRadio->IsSelected();
      SetProperty("IsDirty", "0");
    }
    LoadListItemsChannels();
    m_channels.m_mode = CVNSIChannels::CHANNEL;
  }
  else if (controlId == CONTROL_FILTERSAVE_BUTTON)
  {
    if (m_channels.m_loaded)
    {
      SaveChannelWhitelist(m_ratioIsRadio->IsSelected());
      SaveChannelBlacklist(m_ratioIsRadio->IsSelected());
      SetProperty("IsDirty", "0");
    }
  }
  else if (controlId == CONTROL_ITEM_LIST)
  {
    if (m_channels.m_mode == CVNSIChannels::PROVIDER)
    {
      int pos = GetCurrentListPosition();
      std::shared_ptr<kodi::gui::CListItem> item = GetListItem(pos);
      int idx = std::stoi(item->GetProperty("identifier"));
      if (m_channels.m_providers[idx].m_whitelist)
      {
        item->SetProperty("IsWhitelist", "false");
        m_channels.m_providers[idx].m_whitelist = false;
      }
      else
      {
        item->SetProperty("IsWhitelist", "true");
        m_channels.m_providers[idx].m_whitelist = true;
      }
      SetProperty("IsDirty", "1");
    }
    else if (m_channels.m_mode == CVNSIChannels::CHANNEL)
    {
      int pos = GetCurrentListPosition();
      std::shared_ptr<kodi::gui::CListItem> item = GetListItem(pos);
      int channelidx = std::stoi(item->GetProperty("identifier"));
      if (m_channels.m_channels[channelidx].m_blacklist)
      {
        item->SetProperty("IsBlacklist", "false");
        m_channels.m_channels[channelidx].m_blacklist = false;
      }
      else
      {
        item->SetProperty("IsBlacklist", "true");
        m_channels.m_channels[channelidx].m_blacklist = true;
      }
      SetProperty("IsDirty", "1");
    }
  }
  return false;
}

bool cVNSIAdmin::OnAction(ADDON_ACTION actionId)
{
  if (GetFocusId() != CONTROL_OSD_BUTTON && m_bIsOsdControl)
  {
    m_bIsOsdControl = false;
    SetControlLabel(CONTROL_OSD_BUTTON, kodi::GetLocalizedString(30103));
    MarkDirtyRegion();
  }
  else if (GetFocusId() == CONTROL_OSD_BUTTON)
  {
    if (actionId == ADDON_ACTION_SHOW_INFO)
    {
      SetFocusId(CONTROL_MENU);
      return true;
    }
    else if (IsVdrAction(actionId))
    {
      // send all actions to vdr
      cRequestPacket vrp;
      vrp.init(VNSI_OSD_HITKEY);
      vrp.add_U32(actionId);
      cVNSISession::TransmitMessage(&vrp);
      return true;
    }
  }

  if (actionId == ADDON_ACTION_PREVIOUS_MENU || actionId == ADDON_ACTION_NAV_BACK)
  {
    kodi::gui::CWindow::Close();
    return true;
  }

  return false;
}

bool cVNSIAdmin::Create(int x, int y, int w, int h, void* device)
{
  if (m_osdRender)
  {
    m_osdRender->SetControlSize(w, h);
    m_osdRender->SetDevice(device);
  }
  return true;
}

void cVNSIAdmin::Render()
{
  const P8PLATFORM::CLockObject lock(m_osdMutex);
  if (m_osdRender)
  {
    m_osdRender->Render();
    m_osdRender->FreeResources();
  }
  m_bIsOsdDirty = false;
}

void cVNSIAdmin::Stop()
{
  const P8PLATFORM::CLockObject lock(m_osdMutex);
  if (m_osdRender)
  {
    delete m_osdRender;
    m_osdRender = nullptr;
  }
}

bool cVNSIAdmin::Dirty()
{
  return m_bIsOsdDirty;
}

bool cVNSIAdmin::CreateCB(kodi::gui::ClientHandle cbhdl, int x, int y, int w, int h, void* device)
{
  cVNSIAdmin* osd = static_cast<cVNSIAdmin*>(cbhdl);
  return osd->Create(x, y, w, h, device);
}

void cVNSIAdmin::RenderCB(kodi::gui::ClientHandle cbhdl)
{
  cVNSIAdmin* osd = static_cast<cVNSIAdmin*>(cbhdl);
  osd->Render();
}

void cVNSIAdmin::StopCB(kodi::gui::ClientHandle cbhdl)
{
  cVNSIAdmin* osd = static_cast<cVNSIAdmin*>(cbhdl);
  osd->Stop();
}

bool cVNSIAdmin::DirtyCB(kodi::gui::ClientHandle cbhdl)
{
  cVNSIAdmin* osd = static_cast<cVNSIAdmin*>(cbhdl);
  return osd->Dirty();
}

bool cVNSIAdmin::OnResponsePacket(cResponsePacket* resp)
{
  if (resp->getChannelID() == VNSI_CHANNEL_OSD)
  {
    uint32_t wnd, color, x0, y0, x1, y1, len;
    uint8_t* data;
    resp->getOSDData(wnd, color, x0, y0, x1, y1);
    if (wnd >= MAX_TEXTURES)
    {
      kodi::Log(ADDON_LOG_ERROR, "cVNSIAdmin::OnResponsePacket - invalid wndId: %s", wnd);
      return true;
    }
    if (resp->getOpCodeID() == VNSI_OSD_OPEN)
    {
      data = resp->getUserData();
      len = resp->getUserDataLength();
      const P8PLATFORM::CLockObject lock(m_osdMutex);
      if (m_osdRender)
        m_osdRender->AddTexture(wnd, color, x0, y0, x1, y1, data[0]);
    }
    else if (resp->getOpCodeID() == VNSI_OSD_SETPALETTE)
    {
      data = resp->getUserData();
      len = resp->getUserDataLength();
      const P8PLATFORM::CLockObject lock(m_osdMutex);
      if (m_osdRender)
        m_osdRender->SetPalette(wnd, x0, (uint32_t*)data);
    }
    else if (resp->getOpCodeID() == VNSI_OSD_SETBLOCK)
    {
      data = resp->getUserData();
      len = resp->getUserDataLength();
      const P8PLATFORM::CLockObject lock(m_osdMutex);
      if (m_osdRender)
      {
        m_osdRender->SetBlock(wnd, x0, y0, x1, y1, color, data, len);
        m_bIsOsdDirty = true;
      }
    }
    else if (resp->getOpCodeID() == VNSI_OSD_CLEAR)
    {
      const P8PLATFORM::CLockObject lock(m_osdMutex);
      if (m_osdRender)
        m_osdRender->Clear(wnd);
      m_bIsOsdDirty = true;
    }
    else if (resp->getOpCodeID() == VNSI_OSD_CLOSE)
    {
      {
        const P8PLATFORM::CLockObject lock(m_osdMutex);
        if (m_osdRender)
          m_osdRender->DisposeTexture(wnd);
        m_bIsOsdDirty = true;
      }
    }
    else if (resp->getOpCodeID() == VNSI_OSD_MOVEWINDOW)
    {
    }
    else
      return false;
  }
  else
    return false;

  return true;
}

bool cVNSIAdmin::ConnectOSD()
{
  cRequestPacket vrp;
  vrp.init(VNSI_OSD_CONNECT);

  auto vresp = ReadResult(&vrp);
  if (vresp == nullptr || vresp->noResponse())
  {
    return false;
  }
  uint32_t osdWidth = vresp->extract_U32();
  uint32_t osdHeight = vresp->extract_U32();
  if (m_osdRender)
    m_osdRender->SetOSDSize(osdWidth, osdHeight);

  return true;
}

bool cVNSIAdmin::ReadChannelList(bool radio)
{
  cRequestPacket vrp;
  vrp.init(VNSI_CHANNELS_GETCHANNELS);
  vrp.add_U32(radio);
  vrp.add_U8(0); // apply no filter

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
    return false;
  }

  m_channels.m_channels.clear();
  m_channels.m_channelsMap.clear();
  while (vresp->getRemainingLength() >= 3 * 4 + 3)
  {
    CChannel channel;
    channel.m_blacklist = false;

    channel.m_number = vresp->extract_U32();
    char* strChannelName = vresp->extract_String();
    channel.m_name = strChannelName;
    char* strProviderName = vresp->extract_String();
    channel.m_provider = strProviderName;
    channel.m_id = vresp->extract_U32();
    vresp->extract_U32(); // first caid
    char* strCaids = vresp->extract_String();
    channel.SetCaids(strCaids);
    if (m_protocol >= 6)
    {
      std::string ref = vresp->extract_String();
    }
    channel.m_radio = radio;

    m_channels.m_channels.push_back(channel);
    m_channels.m_channelsMap[channel.m_id] = m_channels.m_channels.size() - 1;
  }

  return true;
}

bool cVNSIAdmin::ReadChannelWhitelist(bool radio)
{
  cRequestPacket vrp;
  vrp.init(VNSI_CHANNELS_GETWHITELIST);
  vrp.add_U8(radio);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
    return false;
  }

  m_channels.m_providerWhitelist.clear();
  CProvider provider;
  while (vresp->getRemainingLength() >= 1 + 4)
  {
    char* strProviderName = vresp->extract_String();
    provider.m_name = strProviderName;
    provider.m_caid = vresp->extract_U32();
    m_channels.m_providerWhitelist.push_back(provider);
  }

  return true;
}

bool cVNSIAdmin::SaveChannelWhitelist(bool radio)
{
  m_channels.ExtractProviderWhitelist();

  cRequestPacket vrp;
  vrp.init(VNSI_CHANNELS_SETWHITELIST);
  vrp.add_U8(radio);

  for (const auto& provider : m_channels.m_providerWhitelist)
  {
    vrp.add_String(provider.m_name.c_str());
    vrp.add_S32(provider.m_caid);
  }

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
    return false;
  }

  return true;
}

bool cVNSIAdmin::ReadChannelBlacklist(bool radio)
{
  cRequestPacket vrp;
  vrp.init(VNSI_CHANNELS_GETBLACKLIST);
  vrp.add_U8(radio);

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
    return false;
  }

  m_channels.m_channelBlacklist.clear();
  while (vresp->getRemainingLength() >= 4)
  {
    int id = vresp->extract_U32();
    m_channels.m_channelBlacklist.push_back(id);
  }

  return true;
}

bool cVNSIAdmin::SaveChannelBlacklist(bool radio)
{
  m_channels.ExtractChannelBlacklist();

  cRequestPacket vrp;
  vrp.init(VNSI_CHANNELS_SETBLACKLIST);
  vrp.add_U8(radio);

  for (auto b : m_channels.m_channelBlacklist)
  {
    vrp.add_S32(b);
  }

  auto vresp = ReadResult(&vrp);
  if (!vresp)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s - Can't get response packed", __func__);
    return false;
  }

  return true;
}

void cVNSIAdmin::ClearListItems()
{
  ClearList();
  m_listItems.clear();
}

void cVNSIAdmin::LoadListItemsProviders()
{
  ClearListItems();

  int count = 0;
  for (unsigned int i = 0; i < m_channels.m_providers.size(); i++)
  {
    std::string tmp;
    if (!m_channels.m_providers[i].m_name.empty())
      tmp = m_channels.m_providers[i].m_name;
    else
      tmp = kodi::GetLocalizedString(30114);
    if (m_channels.m_providers[i].m_caid == 0)
    {
      tmp += " - FTA";
    }
    else
    {
      tmp += " - CAID: ";
      char buf[16];
      sprintf(buf, "%04x", m_channels.m_providers[i].m_caid);
      tmp += buf;
    }

    std::shared_ptr<kodi::gui::CListItem> item = std::make_shared<kodi::gui::CListItem>(tmp);
    item->SetProperty("identifier", std::to_string(i));
    AddListItem(item, count);
    m_listItems.push_back(item);

    if (m_channels.m_providers[i].m_whitelist)
      item->SetProperty("IsWhitelist", "true");
    else
      item->SetProperty("IsWhitelist", "false");

    count++;
  }
}

void cVNSIAdmin::LoadListItemsChannels()
{
  ClearListItems();

  int count = 0;
  std::string tmp;
  for (unsigned int i = 0; i < m_channels.m_channels.size(); i++)
  {
    if (!m_channels.IsWhitelist(m_channels.m_channels[i]))
      continue;

    tmp = m_channels.m_channels[i].m_name;
    tmp += " (";
    if (!m_channels.m_channels[i].m_provider.empty())
      tmp += m_channels.m_channels[i].m_provider;
    else
      tmp += kodi::GetLocalizedString(30114);
    tmp += ")";
    std::shared_ptr<kodi::gui::CListItem> item = std::make_shared<kodi::gui::CListItem>(tmp);
    item->SetProperty("identifier", std::to_string(i));
    AddListItem(item, count);
    m_listItems.push_back(item);

    if (m_channels.m_channels[i].m_blacklist)
      item->SetProperty("IsBlacklist", "true");
    else
      item->SetProperty("IsBlacklist", "false");

    count++;
  }
}

void* cVNSIAdmin::Process(void)
{
  std::unique_ptr<cResponsePacket> vresp;

  while (!IsStopped())
  {
    // try to reconnect
    if (m_connectionLost)
    {
      // First wake up the VDR server in case a MAC-Address is specified
      if (!m_wolMac.empty())
      {
        if (!kodi::network::WakeOnLan(m_wolMac))
        {
          kodi::Log(ADDON_LOG_ERROR, "Error waking up VNSI Server at MAC-Address %s",
                    m_wolMac.c_str());
        }
      }

      cVNSISession::eCONNECTIONSTATE state = TryReconnect();
      if (state != cVNSISession::CONN_ESABLISHED)
      {
        Sleep(1000);
        continue;
      }
    }

    // if there's anything in the buffer, read it
    if ((vresp = cVNSISession::ReadMessage(5, 10000)) == nullptr)
    {
      Sleep(5);
      continue;
    }

    if (!OnResponsePacket(vresp.get()))
    {
      kodi::Log(ADDON_LOG_ERROR, "%s - Rxd a response packet on channel %lu !!", __func__,
                vresp->getChannelID());
    }
  }
  return nullptr;
}
