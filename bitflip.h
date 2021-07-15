/***************************************************************************
 *   Copyright (C) 2015 by Juan Carlos Fabero Jiménez                      *
 *   jcfabero@ucm.es                                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

#ifndef BITFLIP_H
#define BITFLIP_H

#define DUMMY     0xFFFFFFFF
#define BUS_SYNC  0x000000BB
#define BUS_WIDTH_DETECT 0x11220044
#define SYNC_WORD 0xAA995566
#define NOOP      0x20000000
#define W_CMD     0x30008001
#define NULLCMD   0x00000000
#define WCFG      0x00000001
#define DGHIGH    0x00000003
#define START     0x00000005
#define RCAP      0x00000006
#define RCRC      0x00000007
#define GRESTORE  0x0000000A
#define GCAPTURE  0x0000000C
#define DESYNCH   0x0000000D
#define WR_IDCODE 0x30018001
#define WR_FAR    0x30002001
#define WR_FDRI   0x30004000

#define LASTFRM   0x03BE0000

#define MAGIC1    0x00090FF0
#define MAGIC2	  0x0FF00FF0
#define MAGIC3    0x0FF00000
#define MAGIC4    0x01




#define ERROR_FAIL -1
#define ERROR_OK   0


typedef struct {
  unsigned long int frame;
  unsigned int offset;
} flipflop_t ;

#endif /* BITFLIP_H*/