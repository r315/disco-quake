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
// snd_mem.c: sound caching

#include "quakedef.h"

/*
================
ResampleSfx
================
*/
void ResampleSfx (sfx_t *sfx, int inrate, int inwidth, byte *data)
{
	int		outcount;
	int		srcsample;
	float	stepscale;
	int		i;
	int		sample, samplefrac, fracstep;
	sfxcache_t	*sc;
	
	sc = Cache_Check (&sfx->cache);
	if (!sc)
		return;

	stepscale = (float)inrate / snd_shm->speed;	// this is usually 0.5, 1, or 2

	outcount = sc->length / stepscale;
	sc->length = outcount;
	if (sc->loopstart != -1)
		sc->loopstart = sc->loopstart / stepscale;

	sc->speed = snd_shm->speed;
	if (snd_loadas8bit.value)
		sc->width = 1;
	else
		sc->width = inwidth;
	sc->stereo = 0;

// resample / decimate to the current source rate

	if (stepscale == 1 && inwidth == 1 && sc->width == 1)
	{
// fast special case
		for (i=0 ; i<outcount ; i++)
			((signed char *)sc->data)[i]
			= (int)( (unsigned char)(data[i]) - 128);
	}
	else
	{
// general case
		samplefrac = 0;
		fracstep = stepscale*256;
		for (i=0 ; i<outcount ; i++)
		{
			srcsample = samplefrac >> 8;
			samplefrac += fracstep;
			if (inwidth == 2)
				sample = LittleShort ( ((short *)data)[srcsample] );
			else
				sample = (int)( (unsigned char)(data[srcsample]) - 128) << 8;
			if (sc->width == 2)
				((short *)sc->data)[i] = sample;
			else
				((signed char *)sc->data)[i] = sample >> 8;
		}
	}
}

//=============================================================================

/*
==============
S_LoadSound
==============
*/
sfxcache_t *S_LoadSound (sfx_t *s)
{
    char	namebuffer[256];
	byte	*data;
	wavinfo_t	info;
	int		len;
	float	stepscale;
	sfxcache_t	*sc;
	byte	stackbuf[1*1024];		// avoid dirtying the cache heap

// see if still in memory
	sc = Cache_Check (&s->cache);
	if (sc)
		return sc;

//Con_Printf ("S_LoadSound: %x\n", (int)stackbuf);
// load it in
    Q_strcpy(namebuffer, "sound/");
    Q_strcat(namebuffer, s->name);

//	Con_Printf ("loading %s\n",namebuffer);

	data = COM_LoadStackFile(namebuffer, stackbuf, sizeof(stackbuf));

	if (!data)
	{
		Con_Printf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	info = SND_GetWavinfo (s->name, data, com_filesize);
	if (info.channels != 1)
	{
		Con_Printf ("%s is a stereo sample\n",s->name);
		return NULL;
	}

	stepscale = (float)info.rate / snd_shm->speed;
	len = info.samples / stepscale;

	len = len * info.width * info.channels;

	sc = Cache_Alloc ( &s->cache, len + sizeof(sfxcache_t), s->name);
	if (!sc)
		return NULL;
	
	sc->length = info.samples;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;

	ResampleSfx (s, sc->speed, sc->width, data + info.dataofs);

	return sc;
}



/*
===============================================================================

WAV loading

===============================================================================
*/
static byte *GetLittleShort(byte *ptr, short *val)
{
	*val = *(short*)ptr;
	return ptr + 2;
}

static byte *GetLittleLong(byte *ptr, int *val)
{
	*val = *(int*)ptr;
	return ptr + 4;
}

static byte *FindChunk(char *name, byte *start, byte *end)
{
	byte *ptr;
	int chunk_size;

	while (1)
	{
		ptr = start;

		if (ptr >= end){
			// didn't find the chunk
			return NULL;
		}
		
		ptr += 4;
		ptr = GetLittleLong(ptr, &chunk_size);

		if (chunk_size < 0) {
			return NULL;
		}
//		if (iff_chunk_len > 1024*1024)
//			Sys_Error ("FindNextChunk: %i length is past the 1 meg sanity limit", iff_chunk_len);
		ptr -= 8;

		if (!Q_strncmp(ptr, name, 4))
			return ptr;

		start = ptr + 8 + ( (chunk_size + 1) & ~1 );
	}
}

void DumpChunks(byte *start, byte *end)
{
	char	str[5];
	byte	*ptr;
	int		chunk_len;
	
	str[4] = 0;
	ptr = start;
	do
	{
		memcpy (str, ptr, 4);
		ptr += 4;
		ptr = GetLittleLong(ptr, &chunk_len);
		Con_Printf ("0x%x : %s (%d)\n", (int)(ptr - 4), str, chunk_len);
		ptr += (chunk_len + 1) & ~1;
	} while (ptr < end);
}

/*
============
SND_GetWavinfo
============
*/
wavinfo_t SND_GetWavinfo (char *name, byte *wavdata, int wavlength)
{
	wavinfo_t	info;
	short		format;
	int         samples;
	byte        *data_p, *iff_data, *iff_end;

	memset (&info, 0, sizeof(info));

	if (!wavdata)
		return info;
		
	iff_data = wavdata;
	iff_end = wavdata + wavlength;

    // find "RIFF" chunk
	data_p = FindChunk("RIFF", iff_data, iff_end);
	if (!(data_p && !Q_strncmp(data_p + 8, "WAVE", 4)))
	{
		Con_Printf("Missing RIFF/WAVE chunks\n");
		return info;
	}

// get "fmt " chunk
	iff_data = data_p + 12;
// DumpChunks (iff_data, iff_end);

	data_p = FindChunk("fmt ", iff_data, iff_end);
	if (!data_p)
	{
		Con_Printf("Missing fmt chunk\n");
		return info;
	}
	data_p += 8;
	data_p = GetLittleShort(data_p, &format);

	if (format != 1)
	{
		Con_Printf("Microsoft PCM format only\n");
		return info;
	}

	data_p = GetLittleShort(data_p, (short*)&info.channels);
	data_p = GetLittleLong(data_p, &info.rate);
	data_p += 4 + 2;
	data_p = GetLittleShort(data_p, (short*)&info.width);
	info.width /= 8;

// get cue chunk
	data_p = FindChunk("cue ", iff_data, iff_end);
	if (data_p)
	{
		data_p += 32;
		data_p = GetLittleLong(data_p, &info.loopstart);
//		Con_Printf("loopstart=%d\n", sfx->loopstart);

	// if the next chunk is a LIST chunk, look for a cue length marker
		data_p = FindChunk ("LIST", data_p, iff_end);
		if (data_p)
		{
			if (!Q_strncmp(data_p + 28, "mark", 4))
			{	// this is not a proper parse, but it works with cooledit...
				data_p += 24;
				data_p = GetLittleLong (data_p, &samples);	// samples in loop
				info.samples = info.loopstart + samples;
//				Con_Printf("looped length: %i\n", i);
			}
		}
	}
	else
		info.loopstart = -1;

// find data chunk
	data_p = FindChunk("data", iff_data, iff_end);
	if (!data_p)
	{
		Con_Printf("Missing data chunk\n");
		return info;
	}

	data_p += 4;
	data_p = GetLittleLong (data_p, &samples);
	samples /= info.width;

	if (info.samples)
	{
		if (samples < info.samples)
			Sys_Error ("Sound %s has a bad loop length", name);
	}
	else
		info.samples = samples;

	info.dataofs = data_p - wavdata;
	
	return info;
}

