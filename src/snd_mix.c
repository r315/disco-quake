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
// snd_mix.c -- portable code to mix sounds for snd_dma.c

#include "quakedef.h"

#ifdef _WIN32
#include "winquake.h"
#else
#define DWORD	unsigned long
#endif

#define	PAINTBUFFER_SIZE	512

static portable_samplepair_t mix_paintbuffer[PAINTBUFFER_SIZE];
static int		mix_scale_table[32][256];
static int		mix_vol;

static inline void SND_Transfer(int endtime)
{
	int 	*src;
	DWORD	*dst;
	int 	count;
	int		val;
	int 	step;
	int 	out_idx;
	int 	out_mask;

	count = (endtime - snd_paintedtime) * snd_shm->channels;
	src = (int *) mix_paintbuffer;
	dst = (DWORD *)snd_shm->buffer;

	out_mask = snd_shm->samples - 1; 
	out_idx = (snd_paintedtime * snd_shm->channels) & out_mask;
	step = 3 - snd_shm->channels;

	if (snd_shm->samplebits == 16)
	{
		short *out = (short *) dst;
		while (count--)
		{
			val = (*src * mix_vol) >> 8;
			src += step;
			if (val > 0x7fff)
				val = 0x7fff;
			else if (val < (short)0x8000)
				val = (short)0x8000;
			out[out_idx] = val;
			out_idx = (out_idx + 1) & out_mask;
		}
	}
	else if (snd_shm->samplebits == 8)
	{
		unsigned char *out = (unsigned char *) dst;
		while (count--)
		{
			val = (*src * mix_vol) >> 8;
			src += step;
			if (val > 0x7fff)
				val = 0x7fff;
			else if (val < (short)0x8000)
				val = (short)0x8000;
			out[out_idx] = (val>>8) + 128;
			out_idx = (out_idx + 1) & out_mask;
		}
	}
}

#if	!id386
static void SND_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int count)
{
	int 	data;
	int		*lscale, *rscale;
	unsigned char *sfx;
	int		i;

	if (ch->leftvol > 255)
		ch->leftvol = 255;
	if (ch->rightvol > 255)
		ch->rightvol = 255;
		
	lscale = mix_scale_table[ch->leftvol >> 3];
	rscale = mix_scale_table[ch->rightvol >> 3];
	sfx = (unsigned char *)sc->data + ch->pos;

	for (i=0 ; i<count ; i++)
	{
		data = sfx[i];
		mix_paintbuffer[i].left += lscale[data];
		mix_paintbuffer[i].right += rscale[data];
	}
	
	ch->pos += count;
}
#endif	// !id386

static void SND_PaintChannelFrom16 (channel_t *ch, sfxcache_t *sc, int count)
{
	int data;
	int left, right;
	int leftvol, rightvol;
	signed short *sfx;
	int	i;

	leftvol = ch->leftvol;
	rightvol = ch->rightvol;
	sfx = (signed short *)sc->data + ch->pos;

	for (i=0 ; i<count ; i++)
	{
		data = sfx[i];
		left = (data * leftvol) >> 8;
		right = (data * rightvol) >> 8;
		mix_paintbuffer[i].left += left;
		mix_paintbuffer[i].right += right;
	}

	ch->pos += count;
}

/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/
void S_MIX_PaintChannels(int endtime)
{
	int 		i;
	int			end;
	channel_t 	*ch;
	sfxcache_t	*sc;
	int			ltime, count;

	while (snd_paintedtime < endtime)
	{
	// if mix_paintbuffer is smaller than DMA buffer
		end = endtime;
		if (endtime - snd_paintedtime > PAINTBUFFER_SIZE)
			end = snd_paintedtime + PAINTBUFFER_SIZE;

	// clear the paint buffer
		Q_memset(mix_paintbuffer, 0, (end - snd_paintedtime) * sizeof(portable_samplepair_t));

	// paint in the channels.
		ch = snd_channels;
		for (i=0; i < snd_total_channels ; i++, ch++)
		{
			if (!ch->sfx)
				continue;

			if (!ch->leftvol && !ch->rightvol)
				continue;

			sc = S_LoadSound (ch->sfx);

			if (!sc)
				continue;

			ltime = snd_paintedtime;

			while (ltime < end)
			{	// paint up to end
				if (ch->end < end)
					count = ch->end - ltime;
				else
					count = end - ltime;

				if (count > 0)
				{	
					if (sc->width == 1)
						SND_PaintChannelFrom8(ch, sc, count);
					else
						SND_PaintChannelFrom16(ch, sc, count);
	
					ltime += count;
				}

			// if at end of loop, restart
				if (ltime >= ch->end)
				{
					if (sc->loopstart >= 0)
					{
						ch->pos = sc->loopstart;
						ch->end = ltime + sc->length - ch->pos;
					}
					else				
					{	// channel just stopped
						ch->sfx = NULL;
						break;
					}
				}
			}
															  
		}

		// transfer out according to DMA format
		mix_vol = snd_volume.value * 256;

		SND_Transfer(end);
		
		snd_paintedtime = end;
	}
}

void S_MIX_InitScaletable (void)
{
	int		i, j;	
	for (i=0 ; i<32 ; i++)
		for (j=0 ; j<256 ; j++)
			mix_scale_table[i][j] = ((signed char)j) * i * 8;
}
