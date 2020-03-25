/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "Shader.h"
#include "Matrix.h"

class ATTRIBUTE_HIDDEN CGUIShader : public CShaderProgram, public CMatrix
{
public:
  CGUIShader(std::string vert, std::string frag);

  void OnCompiledAndLinked() override;
  bool OnEnabled() override;
  void Free();

  GLint GetPosLoc() { return m_hPos; }
  GLint GetCordLoc() { return m_hCord; }

protected:
  GLint m_hTex = -1;
  GLint m_hProj = -1;
  GLint m_hModel = -1;
  GLint m_hPos = -1;
  GLint m_hCord = -1;

  GLfloat *m_proj = nullptr;
  GLfloat *m_model = nullptr;
};
