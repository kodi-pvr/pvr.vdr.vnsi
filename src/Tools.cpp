/*
 *  Copyright (C) 2010-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Tools.h"

#ifndef TARGET_WINDOWS
#define TYP_INIT 0
#define TYP_SMLE 1
#define TYP_BIGE 2

// need to check for ntohll definition
// as it was added in iOS SDKs since 8.0
#if !defined(ntohll)
uint64_t ntohll(uint64_t a)
{
  return htonll(a);
}
#endif

#if !defined(htonll)
uint64_t htonll(uint64_t a)
{
  static int typ = TYP_INIT;
  unsigned char c;
  union
  {
    uint64_t ull;
    unsigned char c[8];
  } x;
  if (typ == TYP_INIT)
  {
    x.ull = 0x01;
    typ = (x.c[7] == 0x01ULL) ? TYP_BIGE : TYP_SMLE;
  }
  if (typ == TYP_BIGE)
    return a;
  x.ull = a;
  c = x.c[0];
  x.c[0] = x.c[7];
  x.c[7] = c;
  c = x.c[1];
  x.c[1] = x.c[6];
  x.c[6] = c;
  c = x.c[2];
  x.c[2] = x.c[5];
  x.c[5] = c;
  c = x.c[3];
  x.c[3] = x.c[4];
  x.c[4] = c;
  return x.ull;
}
#endif
#endif
