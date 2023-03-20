#version 100

attribute vec4 a_pos;
attribute vec2 a_coord;

varying vec2 v_coord;

uniform mat4 m_proj;
uniform mat4 m_model;

void main()
{
  mat4 mvp = m_proj * m_model;
  gl_Position = a_pos;
  v_coord = a_coord;
}
