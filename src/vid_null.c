/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// vid_null.c -- null video driver to aid porting efforts

#include "quakedef.h"
#include "d_local.h"

#define	CANVAS_WIDTH	320
#define	CANVAS_HEIGHT	200

viddef_t vid;				// global video state
byte vid_buffer[CANVAS_WIDTH*CANVAS_HEIGHT];
short zbuffer[CANVAS_WIDTH*CANVAS_HEIGHT];
byte surfcache[256*1024];
#ifdef _WIN32
#include "winquake.h"
modestate_t modestate;
#endif
cvar_t _windowed_mouse;

unsigned short d_8to16table[256];
unsigned d_8to24table[256];

void VID_SetPalette (unsigned char *palette){}

void VID_ShiftPalette (unsigned char *palette) {}

void VID_Init (unsigned char *palette)
{
	vid.maxwarpwidth = vid.width = vid.conwidth = CANVAS_WIDTH;
	vid.maxwarpheight = vid.height = vid.conheight = CANVAS_HEIGHT;
	vid.aspect = 1.0;
	vid.numpages = 1;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
	vid.buffer = vid.conbuffer = vid_buffer;
	vid.rowbytes = vid.conrowbytes = CANVAS_WIDTH;
	
	d_pzbuffer = zbuffer;
	D_InitCaches (surfcache, sizeof(surfcache));
}

void VID_Shutdown (void){}
void VID_Update (vrect_t *rects){}
#if defined(_WIN32)
void VID_HandlePause (qboolean pause){}
#endif
/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}


/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
}


