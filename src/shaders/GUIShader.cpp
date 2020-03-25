/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "GUIShader.h"

CGUIShader::CGUIShader(std::string vert, std::string frag ) : CShaderProgram(vert, frag)
{
}

void CGUIShader::OnCompiledAndLinked()
{
  // Variables passed directly to the Fragment shader
  m_hTex = glGetUniformLocation(ProgramHandle(), "m_samp");

  // Variables passed directly to the Vertex shader
  m_hProj = glGetUniformLocation(ProgramHandle(), "m_proj");
  m_hModel = glGetUniformLocation(ProgramHandle(), "m_model");
  m_hPos = glGetAttribLocation(ProgramHandle(), "m_attrpos");
  m_hCord = glGetAttribLocation(ProgramHandle(), "m_attrcord");

  // It's okay to do this only one time. Textures units never change.
  glUseProgram(ProgramHandle());
  glUniform1i(m_hTex, 0);
  glUseProgram(0);
}

bool CGUIShader::OnEnabled()
{
  // This is called after glUseProgram()
  glUniformMatrix4fv(m_hProj, 1, GL_FALSE, GetMatrix(MM_PROJECTION));
  glUniformMatrix4fv(m_hModel, 1, GL_FALSE, GetMatrix(MM_MODELVIEW));

  return true;
}

void CGUIShader::Free()
{
  CShaderProgram::Free();
}
