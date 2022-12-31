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
// screen.h

#define SCR_MAX_VIEWSIZE    120
#define SCR_MIN_VIEWSIZE    100
#define SCR_MAX_FOV         170
#define SCR_MIN_FOV         10

void SCR_Init (void);
void SCR_UpdateScreen (void);
void SCR_SizeUp (void);
void SCR_SizeDown (void);
void SCR_BringDownConsole (void);
void SCR_CenterPrint (char *str);
void SCR_BeginLoadingPlaque (void);
void SCR_EndLoadingPlaque (void);
int  SCR_ModalMessage (char *text);
void SCR_SetFullUpdate (void);      // Fill screen with background texture and copy full view to video buffer
void SCR_SetTopCopy (void);         // Copy screen part above status bar to video buffer
void SCR_SetClearNotify (void);
int  SCR_GetConsoleSize (void);
void SCR_SetEnable (qboolean en);
qboolean SCR_GetEnable (void);
void SCR_ChangeViewSize (int dir);
float SCR_GetSize (void);
void SCR_Changed (void);

extern cvar_t       scr_viewsize;
extern cvar_t	    scr_fov;

// only the refresh window will be updated unless these variables are flagged
extern vrect_t	    scr_vrect;
