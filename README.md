[![Build Status](https://travis-ci.org/kodi-pvr/pvr.vdr.vnsi.svg?branch=Leia)](https://travis-ci.org/kodi-pvr/pvr.vdr.vnsi/branches)
[![Build Status](https://dev.azure.com/teamkodi/kodi-pvr/_apis/build/status/kodi-pvr.pvr.vdr.vnsi?branchName=Leia)](https://dev.azure.com/teamkodi/kodi-pvr/_build/latest?definitionId=69&branchName=Leia)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/5120/badge.svg)](https://scan.coverity.com/projects/5120)

# VDR VNSI PVR
VDR VNSI PVR client addon for [Kodi] (http://kodi.tv)

## Build instructions

### Linux

1. `git clone --branch Leia https://github.com/xbmc/xbmc.git`
2. `git clone https://github.com/kodi-pvr/pvr.vdr.vnsi.git`
3. `cd pvr.vdr.vnsi && mkdir build && cd build`
4. `cmake -DADDONS_TO_BUILD=pvr.vdr.vnsi -DADDON_SRC_PREFIX=../.. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=../../xbmc/addons -DPACKAGE_ZIP=1 ../../xbmc/cmake/addons`
5. `make`

##### Useful links

* [Kodi's PVR user support] (http://forum.kodi.tv/forumdisplay.php?fid=169)
* [Kodi's PVR development support] (http://forum.kodi.tv/forumdisplay.php?fid=136)
