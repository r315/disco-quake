
#include <stdio.h>
#include "SDL_audio.h"
#include "SDL_byteorder.h"
#include "quakedef.h"

static dma_t the_shm;
static int snd_inited;

static int desired_speed = 11025;
static int desired_bits = 16;
static int desired_channels = 2;
static int desired_samples = 512;

int gSndBufSize = (64 * 1024);
void *pDSBuf = NULL;


void SNDDMA_Submit(void)
{
}

static void paint_audio(void *unused, Uint8 *stream, int len)
{
	if (snd_shm ) {
		snd_shm->buffer = stream;
		snd_shm->samplepos += len / (snd_shm->samplebits / 8) / snd_shm->channels;
	}
}

qboolean SNDDMA_Init(void)
{
	SDL_AudioSpec desired, obtained;

	snd_inited = 0;

	/* Set up the desired format */
	desired.freq = desired_speed;
	switch (desired_bits) {
		case 8:
			desired.format = AUDIO_U8;
			break;
		case 16:
			if ( SDL_BYTEORDER == SDL_BIG_ENDIAN )
				desired.format = AUDIO_S16MSB;
			else
				desired.format = AUDIO_S16LSB;
			break;
		default:
        		Con_Printf("Unknown number of audio bits: %d\n",
								desired_bits);
			return 0;
	}

	desired.channels = desired_channels;
	desired.samples = desired_samples;
	desired.callback = paint_audio;

	/* Open the audio device */
	if ( SDL_OpenAudio(&desired, &obtained) < 0 ) {
        	Con_Printf("Couldn't open SDL audio: %s\n", SDL_GetError());
		return 0;
	}

	/* Make sure we can support the audio format */
	switch (obtained.format) {
		case AUDIO_U8:
			/* Supported */
			break;
		case AUDIO_S16LSB:
		case AUDIO_S16MSB:
			if ( ((obtained.format == AUDIO_S16LSB) &&
			     (SDL_BYTEORDER == SDL_LIL_ENDIAN)) ||
			     ((obtained.format == AUDIO_S16MSB) &&
			     (SDL_BYTEORDER == SDL_BIG_ENDIAN)) ) {
				/* Supported */
				break;
			}
			/* Unsupported, fall through */;
		default:
			/* Not supported -- force SDL to do our bidding */
			SDL_CloseAudio();
			if ( SDL_OpenAudio(&desired, NULL) < 0 ) {
        			Con_Printf("Couldn't open SDL audio: %s\n",
							SDL_GetError());
				return 0;
			}
			memcpy(&obtained, &desired, sizeof(desired));
			break;
	}
	SDL_PauseAudio(0);

	/* Fill the audio DMA information block */
	snd_shm = &the_shm;
	snd_shm->splitbuffer = 0;
	snd_shm->samplebits = (obtained.format & 0xFF);
	snd_shm->speed = obtained.freq;
	snd_shm->channels = obtained.channels;
	snd_shm->samples = obtained.samples*snd_shm->channels;
	snd_shm->samplepos = 0;
	snd_shm->submission_chunk = 1;
	snd_shm->buffer = NULL;

	snd_inited = 1;
	return 1;
}

int SNDDMA_GetDMAPos(void)
{
	return snd_shm->samplepos;
}

void SNDDMA_Shutdown(void)
{
	if (snd_inited)
	{
		SDL_CloseAudio();
		snd_inited = 0;
	}
}

