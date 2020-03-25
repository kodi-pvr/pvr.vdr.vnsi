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
#include <string.h>
#include <vector>

enum EMATRIXMODE
{
  MM_PROJECTION = 0,
  MM_MODELVIEW,
  MM_TEXTURE,
  MM_MATRIXSIZE  // Must be last! used for size of matrices
};

class ATTRIBUTE_HIDDEN CMatrix
{
public:
  CMatrix();
  virtual ~CMatrix();
  
  GLfloat* GetMatrix(EMATRIXMODE mode);

  void MatrixMode(EMATRIXMODE mode);
  void PushMatrix();
  void PopMatrix();
  void LoadIdentity();
  void Ortho(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f);
  void Ortho2D(GLfloat l, GLfloat r, GLfloat b, GLfloat t);
  void Frustum(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f);
  void Translatef(GLfloat x, GLfloat y, GLfloat z);
  void Scalef(GLfloat x, GLfloat y, GLfloat z);
  void Rotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
  void MultMatrixf(const GLfloat *matrix);
  void LookAt(GLfloat eyex, GLfloat eyey, GLfloat eyez, GLfloat centerx, GLfloat centery, GLfloat centerz, GLfloat upx, GLfloat upy, GLfloat upz);
  bool Project(GLfloat objx, GLfloat objy, GLfloat objz, const GLfloat modelMatrix[16], const GLfloat projMatrix[16], const GLint viewport[4], GLfloat* winx, GLfloat* winy, GLfloat* winz);

protected:

  struct MatrixWrapper 
  {
    MatrixWrapper(){};
    MatrixWrapper( const float values[16]) { memcpy(m_values,values,sizeof(m_values)); }
    MatrixWrapper( const MatrixWrapper &rhs ) { memcpy(m_values, rhs.m_values, sizeof(m_values)); }
    MatrixWrapper &operator=( const MatrixWrapper &rhs ) { memcpy(m_values, rhs.m_values, sizeof(m_values)); return *this;}
    operator float*() { return m_values; }
    operator const float*() const { return m_values; }

    float m_values[16];
  };

  std::vector<struct MatrixWrapper> m_matrices[(int)MM_MATRIXSIZE];
  GLfloat *m_pMatrix;
  EMATRIXMODE m_matrixMode;
};
