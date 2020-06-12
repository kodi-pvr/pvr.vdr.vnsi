/*
 *  Copyright (C) 2010-2020 Team Kodi
 *  Copyright (C) 2010 Alwin Esch (Team Kodi)
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <inttypes.h>

#ifndef TARGET_WINDOWS
// need to check for ntohll definition
// as it was added in iOS SDKs since 8.0
#if !defined(ntohll)
uint64_t ntohll(uint64_t a);
#endif
#if !defined(htonll)
uint64_t htonll(uint64_t a);
#endif
#endif
