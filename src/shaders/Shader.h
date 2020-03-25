/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#if defined(HAS_GL)
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif//__APPLE__
#else
#if defined(__APPLE__)                                                                                                                                                                                           
#include <OpenGLES/ES2/gl.h>                                                                                                                                                                                     
#include <OpenGLES/ES2/glext.h>                                                                                                                                                                                  
#else                                                                                                                                                                                                            
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif//__APPLE__
#endif

#include "kodi/AddonBase.h"
#include <vector>
#include <string>

//////////////////////////////////////////////////////////////////////
// CShader - base class
//////////////////////////////////////////////////////////////////////
class ATTRIBUTE_HIDDEN CShader
{
public:
  CShader() = default;
  virtual ~CShader() = default;
  virtual bool Compile() = 0;
  virtual void Free() = 0;
  virtual GLuint Handle() = 0;

  bool LoadSource(std::string &file);
  bool OK() { return m_compiled; }

protected:
  std::string m_source;
  std::string m_lastLog;
  std::vector<std::string> m_attr;
  bool m_compiled = false;
};


//////////////////////////////////////////////////////////////////////
// CVertexShader
//////////////////////////////////////////////////////////////////////
class ATTRIBUTE_HIDDEN CVertexShader : public CShader
{
public:
  CVertexShader() = default;
  ~CVertexShader() override { Free(); }
  void Free() override;
  bool Compile() override;
  GLuint Handle() override { return m_vertexShader; }

protected:
  GLuint m_vertexShader = 0;
};

//////////////////////////////////////////////////////////////////////
// CPixelShader
//////////////////////////////////////////////////////////////////////
class ATTRIBUTE_HIDDEN CPixelShader : public CShader
{
public:
  CPixelShader() = default;
  ~CPixelShader() { Free(); }
  void Free() override;
  bool Compile() override;
  GLuint Handle() override { return m_pixelShader; }

protected:
  GLuint m_pixelShader = 0;
};

//////////////////////////////////////////////////////////////////////
// CShaderProgram
//////////////////////////////////////////////////////////////////////
class ATTRIBUTE_HIDDEN CShaderProgram
{
public:
  CShaderProgram(std::string &vert, std::string &frag);

  virtual ~CShaderProgram()
  {
    Free();
    delete m_pFP;
    delete m_pVP;
  }

  bool Enable();
  void Disable();
  bool OK() { return m_ok; }
  void Free();

  CVertexShader* VertexShader() { return m_pVP; }
  CPixelShader* PixelShader() { return m_pFP; }
  bool CompileAndLink();

  virtual void OnCompiledAndLinked() {};
  virtual bool OnEnabled() { return false; };
  virtual void OnDisabled() {};

  GLuint ProgramHandle() { return m_shaderProgram; }

protected:
  CVertexShader* m_pVP = nullptr;
  CPixelShader* m_pFP = nullptr;
  GLuint m_shaderProgram = 0;
  bool m_ok = false;
  bool m_validated = false;
};

