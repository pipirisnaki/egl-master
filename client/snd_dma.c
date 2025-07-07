/*
Copyright (C) 1997-2001 Id Software, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

//
// snd_dma.c
// Main control for any streaming sound output device
//

#include "snd_local.h"

typedef struct sfxSamplePair_s {
	int				left;
	int				right;
} sfxSamplePair_t;

audioDMA_t				snd_audioDMA;

// Audio channels
static channel_t		snd_dmaOutChannels[MAX_CHANNELS];

// Raw sampling
#define MAX_RAW_SAMPLES	8192
static int				snd_dmaRawEnd;
static sfxSamplePair_t	snd_dmaRawSamples[MAX_RAW_SAMPLES];

// Buffer painting
#define SND_PBUFFER		2048
static sfxSamplePair_t	snd_dmaPaintBuffer[SND_PBUFFER];
static int				snd_dmaScaleTable[32][256];
static int				*snd_dmaMixPointer;
static int				snd_dmaLinearCount;
static int16			*snd_dmaBufferOutput;

static int				snd_dmaSoundTime;	// sample PAIRS
int						snd_dmaPaintedTime;	// sample PAIRS

// Orientation
static vec3_t			snd_dmaOrigin;
static vec3_t			snd_dmaRightVec;

/*
================
DMASnd_ScaleTableInit
================
*/
static void DMASnd_ScaleTableInit (void)
{
	int		i, j;
	int		scale;

	s_volume->modified = qFalse;
	for (i=0 ; i<32 ; i++) {
		scale = i * 8 * 256 * s_volume->floatVal;
		for (j=0 ; j<256 ; j++) {
			snd_dmaScaleTable[i][j] = ((signed char)j) * scale;
		}
	}
}

/*
===============================================================================

	BUFFER WRITING

===============================================================================
*/

/*
================
DMASnd_WriteLinearBlastStereo16
================
*/
static void DMASnd_WriteLinearBlastStereo16 (void)
{
	int		i;
	int		val;

	for (i=0 ; i<snd_dmaLinearCount ; i+=2) {
		val = snd_dmaMixPointer[i]>>8;
		if (val > 0x7fff)
			snd_dmaBufferOutput[i] = 0x7fff;
		else if (val < (int16)0x8000)
			snd_dmaBufferOutput[i] = (int16)0x8000;
		else
			snd_dmaBufferOutput[i] = val;

		val = snd_dmaMixPointer[i+1]>>8;
		if (val > 0x7fff)
			snd_dmaBufferOutput[i+1] = 0x7fff;
		else if (val < (int16)0x8000)
			snd_dmaBufferOutput[i+1] = (int16)0x8000;
		else
			snd_dmaBufferOutput[i+1] = val;
	}
}


/*
================
DMASnd_TransferStereo16
================
*/
static void DMASnd_TransferStereo16 (uint32 *pbuf, int endTime)
{
	int		lpos;
	int		lpaintedtime;
	
	snd_dmaMixPointer = (int *) snd_dmaPaintBuffer;
	lpaintedtime = snd_dmaPaintedTime;

	while (lpaintedtime < endTime) {
		// Handle recirculating buffer issues
		lpos = lpaintedtime & ((snd_audioDMA.samples>>1)-1);

		snd_dmaBufferOutput = (int16 *) pbuf + (lpos<<1);
		snd_dmaLinearCount = (snd_audioDMA.samples>>1) - lpos;
		if (lpaintedtime + snd_dmaLinearCount > endTime)
			snd_dmaLinearCount = endTime - lpaintedtime;

		snd_dmaLinearCount <<= 1;

		// Write a linear blast of samples
		DMASnd_WriteLinearBlastStereo16 ();

		snd_dmaMixPointer += snd_dmaLinearCount;
		lpaintedtime += (snd_dmaLinearCount>>1);
	}
}


/*
===================
DMASnd_TransferPaintBuffer
===================
*/
static void DMASnd_TransferPaintBuffer (int endTime)
{
	int		out_idx;
	int		count;
	int		out_mask;
	int		*p;
	int		step;
	int		val;
	uint32	*pbuf;

	pbuf = (uint32 *)snd_audioDMA.buffer;

	if (s_testsound->intVal) {
		int		i;

		// Write a fixed sine wave
		count = (endTime - snd_dmaPaintedTime);
		for (i=0 ; i<count ; i++)
			snd_dmaPaintBuffer[i].left = snd_dmaPaintBuffer[i].right = sin ((snd_dmaPaintedTime+i)*0.1)*20000*256;
	}

	if (snd_audioDMA.sampleBits == 16 && snd_audioDMA.channels == 2) {
		// Optimized case
		DMASnd_TransferStereo16 (pbuf, endTime);
	}
	else {
		// General case
		p = (int *) snd_dmaPaintBuffer;
		count = (endTime - snd_dmaPaintedTime) * snd_audioDMA.channels;
		out_mask = snd_audioDMA.samples - 1; 
		out_idx = snd_dmaPaintedTime * snd_audioDMA.channels & out_mask;
		step = 3 - snd_audioDMA.channels;

		if (snd_audioDMA.sampleBits == 16) {
			int16 *out = (int16 *) pbuf;
			while (count--) {
				val = *p >> 8;
				p += step;
				if (val > 0x7fff)
					val = 0x7fff;
				else if (val < (int16)0x8000)
					val = (int16)0x8000;
				out[out_idx] = val;
				out_idx = (out_idx + 1) & out_mask;
			}
		}
		else if (snd_audioDMA.sampleBits == 8) {
			byte	*out = (byte *) pbuf;
			while (count--) {
				val = *p >> 8;
				p += step;
				if (val > 0x7fff)
					val = 0x7fff;
				else if (val < (int16)0x8000)
					val = (int16)0x8000;
				out[out_idx] = (val>>8) + 128;
				out_idx = (out_idx + 1) & out_mask;
			}
		}
	}
}

/*
===============================================================================

	SPATIALIZATION

===============================================================================
*/

/*
=================
DMASnd_SpatializeOrigin

Used for spatializing channels and autosounds
=================
*/
static void DMASnd_SpatializeOrigin (vec3_t origin, float masterVol, float distMult, int *leftVol, int *rightVol)
{
	float		dot, dist;
	float		leftScale, rightScale, scale;
	vec3_t		sourceVec;

	if (Com_ClientState () != CA_ACTIVE) {
		*leftVol = *rightVol = 255;
		return;
	}

	// Calculate stereo seperation and distance attenuation
	Vec3Subtract (origin, snd_dmaOrigin, sourceVec);

	dist = VectorNormalizef (sourceVec, sourceVec) - SOUND_FULLVOLUME;
	if (dist < 0)
		dist = 0;			// close enough to be at full volume
	dist *= distMult;		// different attenuation levels
	
	dot = DotProduct (snd_dmaRightVec, sourceVec);

	if (snd_audioDMA.channels == 1 || !distMult) {
		// No attenuation = no spatialization
		rightScale = 1.0f;
		leftScale = 1.0f;
	}
	else {
		rightScale = 0.5f * (1.0f + dot);
		leftScale = 0.5f * (1.0f - dot);
	}

	// Add in distance effect
	scale = (1.0 - dist) * rightScale;
	*rightVol = Q_rint (masterVol * scale);
	if (*rightVol < 0)
		*rightVol = 0;

	scale = (1.0 - dist) * leftScale;
	*leftVol = Q_rint (masterVol * scale);
	if (*leftVol < 0)
		*leftVol = 0;
}


/*
=================
DMASnd_SpatializeChannel
=================
*/
static void DMASnd_SpatializeChannel (channel_t *ch)
{
	vec3_t		origin, velocity;

	switch (ch->psType) {
	case PSND_FIXED:
		Vec3Copy (ch->origin, origin);
		break;

	case PSND_ENTITY:
		CL_CGModule_GetEntitySoundOrigin (ch->entNum, origin, velocity);
		break;

	case PSND_LOCAL:
		// Anything coming from the view entity will always be full volume
		ch->leftVol = ch->masterVol;
		ch->rightVol = ch->masterVol;
		return;
	}

	// Spatialize fixed/entity sounds
	DMASnd_SpatializeOrigin (origin, ch->masterVol, ch->distMult, &ch->leftVol, &ch->rightVol);
}

/*
===============================================================================

	CHANNELS

===============================================================================
*/

/*
=================
DMASnd_PickChannel
=================
*/
static channel_t *DMASnd_PickChannel (int entNum, entChannel_t entChannel)
{
	int			i;
	int			firstToDie;
	int			lifeLeft;
	channel_t	*ch;

	firstToDie = -1;
	lifeLeft = 0x7fffffff;

	// Check for replacement sound, or find the best one to replace
	for (i=0, ch=snd_dmaOutChannels ; i<MAX_CHANNELS ; ch++, i++) {
		// Channel 0 never overrides
		if (entChannel != 0 && ch->entNum == entNum && ch->entChannel == entChannel) {
			// Always override sound from same entity
			firstToDie = i;
			break;
		}

		// Don't let monster sounds override player sounds
		if (ch->entNum == cl.playerNum+1 && entNum != cl.playerNum+1 && ch->sfx)
			continue;

		// Replace the oldest sound
		if (ch->endTime - snd_dmaPaintedTime < lifeLeft) {
			lifeLeft = ch->endTime - snd_dmaPaintedTime;
			firstToDie = i;
		}
   }

	if (firstToDie == -1)
		return NULL;

	ch = &snd_dmaOutChannels[firstToDie];
	memset (ch, 0, sizeof (channel_t));
	return ch;
}

/*
===============================================================================

	PLAYSOUNDS

===============================================================================
*/

/*
===============
DMASnd_IssuePlaysound

Take the next playsound and begin it on the channel. This is never
called directly by Snd_Play*, but only by the update loop.
===============
*/
static void DMASnd_IssuePlaysounds (int endTime)
{
	channel_t	*ch;
	sfxCache_t	*sc;
	playSound_t	*ps;

	for ( ; ; ) {
		ps = snd_pendingPlays.next;
		if (ps == &snd_pendingPlays)
			break;	// No more pending sounds
		if (ps->beginTime > snd_dmaPaintedTime) {
			if (ps->beginTime < endTime)
				endTime = ps->beginTime;	// Stop here
			break;
		}

		if (s_show->intVal)
			Com_Printf (0, "Issue %i\n", ps->beginTime);

		// Pick a channel to play on
		ch = DMASnd_PickChannel (ps->entNum, ps->entChannel);
		if (!ch) {
			Snd_FreePlaysound (ps);
			return;
		}

		// Spatialize
		if (ps->attenuation == ATTN_STATIC)
			ch->distMult = ps->attenuation * 0.001f;
		else
			ch->distMult = ps->attenuation * 0.0005f;

		ch->masterVol = ps->volume;
		ch->entNum = ps->entNum;
		ch->entChannel = ps->entChannel;
		ch->sfx = ps->sfx;
		Vec3Copy (ps->origin, ch->origin);
		ch->psType = ps->type;

		DMASnd_SpatializeChannel (ch);

		ch->position = 0;
		sc = Snd_LoadSound (ch->sfx);
		ch->endTime = snd_dmaPaintedTime + sc->length;

		// Free the playsound
		Snd_FreePlaysound (ps);
	}
}

/*
===============================================================================

	SOUND PLAYING

===============================================================================
*/

/*
==================
DMASnd_ClearBuffer
==================
*/
static void DMASnd_ClearBuffer (void)
{
	int		clear;

	// Clear the buffers
	snd_dmaRawEnd = 0;
	if (snd_audioDMA.sampleBits == 8)
		clear = 0x80;
	else
		clear = 0;

	SndImp_BeginPainting ();
	if (snd_audioDMA.buffer)
		memset (snd_audioDMA.buffer, clear, snd_audioDMA.samples * snd_audioDMA.sampleBits/8);
	SndImp_Submit ();
}


/*
==================
DMASnd_StopAllSounds
==================
*/
void DMASnd_StopAllSounds (void)
{
	// Clear all the channels
	memset (snd_dmaOutChannels, 0, sizeof (snd_dmaOutChannels));

	// Clear the buffers
	DMASnd_ClearBuffer ();
}

/*
===============================================================================

	CHANNEL MIXING

===============================================================================
*/

/*
==================
DMASnd_AddLoopSounds

Entities with an ent->sound field will generated looped sounds that are automatically
started, stopped, and merged together as the entities are sent to the client
==================
*/
static void DMASnd_AddLoopSounds (void)
{
	int				i, j;
	int				sounds[MAX_CS_EDICTS];
	int				left, right;
	int				leftTotal, rightTotal;
	channel_t		*ch;
	sfx_t			*sfx;
	sfxCache_t		*sc;
	int				num;
	entityState_t	*ent;
	vec3_t			origin, velocity;

	if (cl_paused->intVal || Com_ClientState () != CA_ACTIVE || !cls.soundPrepped)
		return;

	// Build a sound list
	for (i=0 ; i<cl.frame.numEntities ; i++) {
		num = (cl.frame.parseEntities + i)&(MAX_PARSEENTITIES_MASK);
		ent = &cl_parseEntities[num];
		sounds[i] = ent->sound;
	}

	// Add sounds from that list
	for (i=0 ; i<cl.frame.numEntities ; i++) {
		if (!sounds[i])
			continue;

		if (!cl.soundCfgStrings[sounds[i]] && cl.configStrings[CS_SOUNDS+sounds[i]][0])
			cl.soundCfgStrings[sounds[i]] = Snd_RegisterSound (cl.configStrings[CS_SOUNDS+sounds[i]]);

		sfx = cl.soundCfgStrings[sounds[i]];
		if (!sfx)
			continue;	// Bad sound effect

		sc = sfx->cache;
		if (!sc)
			continue;

		num = (cl.frame.parseEntities + i) & MAX_PARSEENTITIES_MASK;
		ent = &cl_parseEntities[num];

		// Get the entity sound origin
		CL_CGModule_GetEntitySoundOrigin (ent->number, origin, velocity);

		// Find the total contribution of all sounds of this type
		DMASnd_SpatializeOrigin (origin, 255.0f, SOUND_LOOPATTENUATE, &leftTotal, &rightTotal);
		for (j=i+1 ; j<cl.frame.numEntities ; j++) {
			if (sounds[j] != sounds[i])
				continue;
			sounds[j] = 0; // FIXME: this is kinda weird...
			// It will literally cut off audio entirely in certain situations, but without it
			// sounds can become fucking blaring if abundant enough.

			num = (cl.frame.parseEntities + j)&(MAX_PARSEENTITIES_MASK);
			ent = &cl_parseEntities[num];

			DMASnd_SpatializeOrigin (origin, 255.0f, SOUND_LOOPATTENUATE, &left, &right);
			leftTotal += left;
			rightTotal += right;
		}

		if (leftTotal == 0 && rightTotal == 0)
			continue;	// Not audible

		// Allocate a channel
		ch = DMASnd_PickChannel (0, 0);
		if (!ch)
			return;

		// Clamp high
		if (leftTotal > 255)
			leftTotal = 255;
		if (rightTotal > 255)
			rightTotal = 255;

		ch->leftVol = leftTotal;
		ch->rightVol = rightTotal;
		ch->autoSound = qTrue;	// Remove next frame
		ch->sfx = sfx;
		ch->position = snd_dmaPaintedTime % sc->length;
		ch->endTime = snd_dmaPaintedTime + sc->length - ch->position;
	}
}


/*
================
DMASnd_PaintChannelFrom8
================
*/
static void DMASnd_PaintChannelFrom8 (channel_t *ch, sfxCache_t *sc, int count, int offset)
{
	int		data;
	int		*lScale, *rScale;
	byte	*sfx;
	int		i;
	sfxSamplePair_t	*samp;

	// Clamp
	if (ch->leftVol > 255)
		ch->leftVol = 255;
	if (ch->rightVol > 255)
		ch->rightVol = 255;

	// Left/right scale
	lScale = snd_dmaScaleTable[ch->leftVol >> 3];
	rScale = snd_dmaScaleTable[ch->rightVol >> 3];
	sfx = (byte *)sc->data + ch->position;

	samp = &snd_dmaPaintBuffer[offset];
	for (i=0 ; i<count ; i++, samp++) {
		data = sfx[i];
		samp->left += lScale[data];
		samp->right += rScale[data];
	}
	
	ch->position += count;
}


/*
================
DMASnd_PaintChannelFrom16
================
*/
static void DMASnd_PaintChannelFrom16 (channel_t *ch, sfxCache_t *sc, int count, int offset)
{
	int		data, i;
	int		left, right;
	int		leftVol, rightVol;
	signed short	*sfx;
	sfxSamplePair_t	*samp;

	leftVol = ch->leftVol * (s_volume->floatVal*256);
	rightVol = ch->rightVol * (s_volume->floatVal*256);
	sfx = (signed short *)sc->data + ch->position;

	samp = &snd_dmaPaintBuffer[offset];
	for (i=0 ; i<count ; i++, samp++) {
		data = sfx[i];
		left = (data * leftVol)>>8;
		right = (data * rightVol)>>8;
		samp->left += left;
		samp->right += right;
	}

	ch->position += count;
}


/*
================
DMASnd_PaintChannels
================
*/
static void DMASnd_PaintChannels (int endTime)
{
	channel_t	*ch;
	sfxCache_t	*sc;
	int			lTime, count;
	int			end, i;

	while (snd_dmaPaintedTime < endTime) {
		// If snd_dmaPaintBuffer is smaller than DMA buffer
		end = endTime;
		if (endTime - snd_dmaPaintedTime > SND_PBUFFER)
			end = snd_dmaPaintedTime + SND_PBUFFER;

		// Start any playsounds
		DMASnd_IssuePlaysounds (end);

		// Clear the paint buffer
		if (snd_dmaRawEnd < snd_dmaPaintedTime) {
			memset (snd_dmaPaintBuffer, 0, (end - snd_dmaPaintedTime) * sizeof (sfxSamplePair_t));
		}
		else {
			// Copy from the streaming sound source
			int		stop, s;

			stop = (end < snd_dmaRawEnd) ? end : snd_dmaRawEnd;

			for (i=snd_dmaPaintedTime ; i<stop ; i++) {
				s = i & (MAX_RAW_SAMPLES-1);
				snd_dmaPaintBuffer[i-snd_dmaPaintedTime] = snd_dmaRawSamples[s];
			}

			for ( ; i<end ; i++) {
				snd_dmaPaintBuffer[i-snd_dmaPaintedTime].left =
				snd_dmaPaintBuffer[i-snd_dmaPaintedTime].right = 0;
			}
		}

		// Paint in the channels
		for (i=0, ch=snd_dmaOutChannels ; i<MAX_CHANNELS ; ch++, i++) {
			lTime = snd_dmaPaintedTime;
		
			while (lTime < end) {
				if (!ch->sfx || (!ch->leftVol && !ch->rightVol))
					break;

				// Max painting is to the end of the buffer
				count = end - lTime;

				// Might be stopped by running out of data
				if (ch->endTime - lTime < count)
					count = ch->endTime - lTime;
		
				sc = Snd_LoadSound (ch->sfx);
				if (!sc)
					break;

				if (count > 0 && ch->sfx) {	
					if (sc->width == 1)
						DMASnd_PaintChannelFrom8 (ch, sc, count, lTime - snd_dmaPaintedTime);
					else
						DMASnd_PaintChannelFrom16 (ch, sc, count, lTime - snd_dmaPaintedTime);
	
					lTime += count;
				}

				// If at end of loop, restart
				if (lTime >= ch->endTime) {
					if (ch->autoSound) {
						// Autolooping sounds always go back to start
						ch->position = 0;
						ch->endTime = lTime + sc->length;
					}
					else if (sc->loopStart >= 0) {
						ch->position = sc->loopStart;
						ch->endTime = lTime + sc->length - ch->position;
					}
					else {
						// Channel just stopped
						ch->sfx = NULL;
					}
				}
			}
															  
		}

		// Transfer out according to DMA format
		DMASnd_TransferPaintBuffer (end);
		snd_dmaPaintedTime = end;
	}
}


/*
============
DMASnd_RawSamples

Cinematic streaming and voice over network
============
*/
void DMASnd_RawSamples (int samples, int rate, int width, int channels, byte *data)
{
	int		i;
	int		src, dst;
	float	scale;

	if (snd_dmaRawEnd < snd_dmaPaintedTime)
		snd_dmaRawEnd = snd_dmaPaintedTime;
	scale = (float)rate / snd_audioDMA.speed;

	switch (channels) {
	case 1:
		switch (width) {
		case 1:
			for (i=0 ; ; i++) {
				src = i*scale;
				if (src >= samples)
					break;
				dst = snd_dmaRawEnd & (MAX_RAW_SAMPLES-1);
				snd_dmaRawEnd++;
				snd_dmaRawSamples[dst].left = (((byte *)data)[src]-128) << 16;
				snd_dmaRawSamples[dst].right = (((byte *)data)[src]-128) << 16;
			}
			break;

		case 2:
			for (i=0 ; ; i++) {
				src = i*scale;
				if (src >= samples)
					break;
				dst = snd_dmaRawEnd & (MAX_RAW_SAMPLES-1);
				snd_dmaRawEnd++;
				snd_dmaRawSamples[dst].left = LittleShort(((int16 *)data)[src]) << 8;
				snd_dmaRawSamples[dst].right = LittleShort(((int16 *)data)[src]) << 8;
			}
			break;
		}
		break;

	case 2:
		switch (width) {
		case 1:
			for (i=0 ; ; i++) {
				src = i*scale;
				if (src >= samples)
					break;
				dst = snd_dmaRawEnd & (MAX_RAW_SAMPLES-1);
				snd_dmaRawEnd++;
				snd_dmaRawSamples[dst].left = ((char *)data)[src*2] << 16;
				snd_dmaRawSamples[dst].right = ((char *)data)[src*2+1] << 16;
			}
			break;

		case 2:
			if (scale == 1.0) {
				for (i=0 ; i<samples ; i++) {
					dst = snd_dmaRawEnd & (MAX_RAW_SAMPLES-1);
					snd_dmaRawEnd++;
					snd_dmaRawSamples[dst].left = LittleShort(((int16 *)data)[i*2]) << 8;
					snd_dmaRawSamples[dst].right = LittleShort(((int16 *)data)[i*2+1]) << 8;
				}
			}
			else {
				for (i=0 ; ; i++) {
					src = i*scale;
					if (src >= samples)
						break;

					dst = snd_dmaRawEnd & (MAX_RAW_SAMPLES-1);
					snd_dmaRawEnd++;
					snd_dmaRawSamples[dst].left = LittleShort(((int16 *)data)[src*2]) << 8;
					snd_dmaRawSamples[dst].right = LittleShort(((int16 *)data)[src*2+1]) << 8;
				}
			}
			break;
		}
		break;
	}
}


/*
============
DMASnd_Update

Called once each time through the main loop
============
*/
void DMASnd_Update (refDef_t *rd)
{
	int			total, i;
	uint32		endTime, samples;
	channel_t	*ch;
	int			samplePos;
	static int	oldSamplePos;
	static int	buffers;
	int			fullSamples;

	if (rd) {
		Vec3Copy (rd->viewOrigin, snd_dmaOrigin);
		Vec3Copy (rd->rightVec, snd_dmaRightVec);
	}
	else {
		Vec3Clear (snd_dmaOrigin);
		Vec3Clear (snd_dmaRightVec);
	}

	// Don't play sounds while the screen is disabled
	if (cls.disableScreen || !snd_isActive) {
		DMASnd_ClearBuffer ();
		return;
	}

	// Rebuild scale tables if volume is modified
	if (s_volume->modified)
		DMASnd_ScaleTableInit ();

	// Update spatialization for dynamic sounds
	for (i=0, ch=snd_dmaOutChannels ; i<MAX_CHANNELS ; ch++, i++) {
		if (!ch->sfx)
			continue;

		if (ch->autoSound) {
			// Autosounds are regenerated fresh each frame
			memset (ch, 0, sizeof (channel_t));
			continue;
		}

		// Respatialize channel
		DMASnd_SpatializeChannel (ch);
		if (!ch->leftVol && !ch->rightVol) {
			memset (ch, 0, sizeof (channel_t));
			continue;
		}
	}

	// Add loopsounds
	DMASnd_AddLoopSounds ();

	// Debugging output
	if (s_show->intVal) {
		total = 0;
		for (i=0, ch=snd_dmaOutChannels ; i<MAX_CHANNELS ; ch++, i++) {
			if (ch->sfx && (ch->leftVol || ch->rightVol)) {
				Com_Printf (0, "%3i %3i %s\n", ch->leftVol, ch->rightVol, ch->sfx->name);
				total++;
			}
		}

		Com_Printf (0, "----(%i)---- painted: %i\n", total, snd_dmaPaintedTime);
	}

	// Mix some sound
	SndImp_BeginPainting ();
	if (!snd_audioDMA.buffer)
		return;

	// Update DMA time
	fullSamples = snd_audioDMA.samples / snd_audioDMA.channels;

	/*
	** It is possible to miscount buffers if it has wrapped twice between
	** calls to Snd_Update. Oh well
	*/
	samplePos = SndImp_GetDMAPos ();
	if (samplePos < oldSamplePos) {
		buffers++;	// Buffer wrapped
		
		if (snd_dmaPaintedTime > 0x40000000) {
			// Time to chop things off to avoid 32 bit limits
			buffers = 0;
			snd_dmaPaintedTime = fullSamples;
			DMASnd_StopAllSounds ();
		}
	}

	oldSamplePos = samplePos;
	snd_dmaSoundTime = buffers*fullSamples + samplePos/snd_audioDMA.channels;

	// Check to make sure that we haven't overshot
	if (snd_dmaPaintedTime < snd_dmaSoundTime) {
		Com_DevPrintf (PRNT_WARNING, "Snd_Update: overflow\n");
		snd_dmaPaintedTime = snd_dmaSoundTime;
	}

	// Mix ahead of current position
	endTime = snd_dmaSoundTime + s_mixahead->floatVal * snd_audioDMA.speed;

	// Mix to an even submission block size
	endTime = (endTime + snd_audioDMA.submissionChunk-1) & ~(snd_audioDMA.submissionChunk-1);
	samples = snd_audioDMA.samples >> (snd_audioDMA.channels-1);
	if (endTime - snd_dmaSoundTime > samples)
		endTime = snd_dmaSoundTime + samples;

	DMASnd_PaintChannels (endTime);
	SndImp_Submit ();
}

/*
==============================================================================

	INIT / SHUTDOWN
 
==============================================================================
*/

/*
================
DMASnd_Init
================
*/
qBool DMASnd_Init (void)
{
	if (!SndImp_Init ())
		return qFalse;

	DMASnd_ScaleTableInit ();

	snd_dmaSoundTime = 0;
	snd_dmaPaintedTime = 0;

	return qTrue;
}


/*
================
DMASnd_Shutdown
================
*/
void DMASnd_Shutdown (void)
{
	SndImp_Shutdown ();

	snd_dmaSoundTime = 0;
	snd_dmaPaintedTime = 0;
}
