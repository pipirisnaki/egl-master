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
// sv_send.c
//

#include "sv_local.h"

/*
=============================================================================

	EVENT MESSAGES

=============================================================================
*/

/*
=================
SV_ClientPrintf

Sends text across to be displayed if the level passes
=================
*/
void SV_ClientPrintf (svClient_t *cl, int level, char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];

	if (level < cl->messageLevel)
		return;

	va_start (argptr, fmt);
	vsnprintf (string, sizeof (string), fmt, argptr);
	va_end (argptr);

	MSG_WriteByte (&cl->netChan.message, SVC_PRINT);
	MSG_WriteByte (&cl->netChan.message, level);
	MSG_WriteString (&cl->netChan.message, string);
}


/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void SV_BroadcastPrintf (int level, char *fmt, ...)
{
	va_list		argptr;
	char		string[2048];
	svClient_t	*cl;
	int			i;

	va_start (argptr, fmt);
	vsnprintf (string, sizeof (string), fmt, argptr);
	va_end (argptr);
	
	// Echo to console
	if (dedicated->intVal) {
		char	copy[1024];
		
		// Mask off high bits
		for (i=0 ; i<1023 && string[i] ; i++)
			copy[i] = string[i]&127;
		copy[i] = 0;
		Com_Printf (0, "%s", copy);
	}

	for (i=0, cl=svs.clients ; i<maxclients->intVal ; i++, cl++) {
		if (level < cl->messageLevel)
			continue;
		if (cl->state != SVCS_SPAWNED)
			continue;

		MSG_WriteByte (&cl->netChan.message, SVC_PRINT);
		MSG_WriteByte (&cl->netChan.message, level);
		MSG_WriteString (&cl->netChan.message, string);
	}
}


/*
=================
SV_BroadcastCommand

Sends text to all active clients
=================
*/
void SV_BroadcastCommand (char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];

	if (!Com_ServerState ())
		return;
	va_start (argptr, fmt);
	vsnprintf (string, sizeof (string), fmt, argptr);
	va_end (argptr);

	MSG_WriteByte (&sv.multiCast, SVC_STUFFTEXT);
	MSG_WriteString (&sv.multiCast, string);
	SV_Multicast (NULL, MULTICAST_ALL_R);
}


/*
===============
SV_Unicast

Sends the contents of the mutlicast buffer to a single client
===============
*/
void SV_Unicast (edict_t *ent, qBool reliable)
{
	int			p;
	svClient_t	*client;

	if (!ent)
		return;

	p = NUM_FOR_EDICT(ent);
	if (p < 1 || p > maxclients->intVal)
		return;

	client = svs.clients + (p-1);
	if (reliable) {
		MSG_WriteRaw (&client->netChan.message, sv.multiCast.data, sv.multiCast.curSize);
	}
	else {
		MSG_WriteRaw (&client->datagram, sv.multiCast.data, sv.multiCast.curSize);
	}

	MSG_Clear (&sv.multiCast);
}


/*
=================
SV_Multicast

Sends the contents of sv.multiCast to a subset of the clients,
then clears sv.multiCast.

MULTICAST_ALL	same as broadcast (origin can be NULL)
MULTICAST_PVS	send to clients potentially visible from org
MULTICAST_PHS	send to clients potentially hearable from org
=================
*/
void SV_Multicast (vec3_t origin, multiCast_t to)
{
	svClient_t	*client;
	byte		*mask;
	int			leafNum, cluster;
	int			j;
	qBool		reliable;
	int			area1, area2;

	reliable = qFalse;

	if ((to != MULTICAST_ALL_R) && (to != MULTICAST_ALL)) {
		leafNum = CM_PointLeafnum (origin);
		area1 = CM_LeafArea (leafNum);
	}
	else {
		leafNum = 0;	// Just to avoid compiler warnings
		area1 = 0;
	}

	// If doing a serverrecord, store everything
	if (svs.demoFile) {
		MSG_WriteRaw (&svs.demoMultiCast, sv.multiCast.data, sv.multiCast.curSize);
	}
	
	switch (to) {
	case MULTICAST_ALL_R:
		reliable = qTrue;	// Intentional fallthrough
	case MULTICAST_ALL:
		leafNum = 0;
		mask = NULL;
		break;

	case MULTICAST_PHS_R:
		reliable = qTrue;	// Intentional fallthrough
	case MULTICAST_PHS:
		leafNum = CM_PointLeafnum (origin);
		cluster = CM_LeafCluster (leafNum);
		mask = CM_ClusterPHS (cluster);
		break;

	case MULTICAST_PVS_R:
		reliable = qTrue;	// Intentional fallthrough
	case MULTICAST_PVS:
		leafNum = CM_PointLeafnum (origin);
		cluster = CM_LeafCluster (leafNum);
		mask = CM_ClusterPVS (cluster);
		break;

	default:
		mask = NULL;
		Com_Error (ERR_FATAL, "SV_Multicast: bad to:%i", to);
	}

	// Send the data to all relevent clients
	for (j=0, client=svs.clients ; j<maxclients->intVal ; j++, client++) {
		if (client->state == SVCS_FREE || client->state == SVCS_ZOMBIE)
			continue;
		if (client->state != SVCS_SPAWNED && !reliable)
			continue;

		if (mask) {
			leafNum = CM_PointLeafnum (client->edict->s.origin);
			cluster = CM_LeafCluster (leafNum);
			area2 = CM_LeafArea (leafNum);
			if (!CM_AreasConnected (area1, area2))
				continue;
			if (mask && (!(mask[cluster>>3] & (1<<(cluster&7)))))
				continue;
		}

		if (reliable) {
			MSG_WriteRaw (&client->netChan.message, sv.multiCast.data, sv.multiCast.curSize);
		}
		else {
			MSG_WriteRaw (&client->datagram, sv.multiCast.data, sv.multiCast.curSize);
		}
	}

	MSG_Clear (&sv.multiCast);
}


/*
==================
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

If channel & 8, the sound will be sent to everyone, not just
things in the PHS.

FIXME: if entity isn't in PHS, they must be forced to be sent or
have the origin explicitly sent.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)

timeOffset can range from 0.0 to 0.1 to cause sounds to be started
later in the frame than they normally would.

If origin is NULL, the origin is determined from the entity origin
or the midpoint of the entity box for bmodels.
==================
*/
#ifndef		SOUND_FULLVOLUME // snd_dma.c
#define 	SOUND_FULLVOLUME	80
#endif	//	SOUND_FULLVOLUME

void SV_StartSound (vec3_t origin, edict_t *entity, int channel, int soundIndex, float vol, float attenuation, float timeOffset)
{
	int			sendChan, flags, i, ent;
	int			cluster, leafNum, area1 = 0, area2;
	float		leftVol, rightVol, distanceMult;
	svClient_t	*client;
	byte		*mask;
	vec3_t		sourceVec, listenerRight;
	vec3_t		originVec;
	float		dot, dist;
	float		rightScale, leftScale;
	qBool		usePHS;

	if (vol < 0 || vol > 1.0f)
		Com_Error (ERR_FATAL, "SV_StartSound: volume = %f", vol);

	if (attenuation < 0 || attenuation > 4)
		Com_Error (ERR_FATAL, "SV_StartSound: attenuation = %f", attenuation);

	if (timeOffset < 0 || timeOffset > 0.255f)
		Com_Error (ERR_FATAL, "SV_StartSound: timeOffset = %f", timeOffset);

	ent = NUM_FOR_EDICT (entity);

	// No PHS flag
	if (channel & 8 || attenuation == ATTN_NONE) {
		// If the sound doesn't attenuate, send it to everyone (global radio chatter, voiceovers, etc)
		usePHS = qFalse;
		channel &= 7;
	}
	else
		usePHS = qTrue;

	sendChan = (ent<<3) | (channel&7);

	flags = 0;
	if (vol != DEFAULT_SOUND_PACKET_VOLUME)
		flags |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		flags |= SND_ATTENUATION;

	/*
	** the client doesn't know that bmodels have weird origins
	** the origin can also be explicitly set
	*/
	if (entity->svFlags & SVF_NOCLIENT || entity->solid == SOLID_BSP || origin)
		flags |= SND_POS;

	// always send the entity number for channel overrides
	flags |= SND_ENT;

	if (timeOffset)
		flags |= SND_OFFSET;

	// use the entity origin/bmodel origin if the origin is specified
	if (!origin) {
		if (entity->solid == SOLID_BSP) {
			originVec[0] = entity->s.origin[0] + 0.5f * (entity->mins[0] + entity->maxs[0]);
			originVec[1] = entity->s.origin[1] + 0.5f * (entity->mins[1] + entity->maxs[1]);
			originVec[2] = entity->s.origin[2] + 0.5f * (entity->mins[2] + entity->maxs[2]);
		}
		else
			Vec3Copy (entity->s.origin, originVec);
		origin = originVec;
	}

	if (usePHS) {
		leafNum = CM_PointLeafnum (origin);
		area1 = CM_LeafArea (leafNum);
	}

	// Cycle through the different targets and do attenuation calculations
	for (i=0, client=svs.clients ; i<maxclients->intVal ; i++, client++) {
		if (client->state == SVCS_FREE || client->state == SVCS_ZOMBIE)
			continue;

		if (client->state != SVCS_SPAWNED && !(channel & CHAN_RELIABLE))
			continue;

		if (usePHS) {
			leafNum = CM_PointLeafnum (client->edict->s.origin);
			area2 = CM_LeafArea (leafNum);
			cluster = CM_LeafCluster (leafNum);
			mask = CM_ClusterPHS (cluster);

			if (!CM_AreasConnected (area1, area2))
				continue; // leafs aren't connected

			if (mask && (!(mask[cluster>>3] & (1<<(cluster&7)))))
				continue; // different pvs cluster
		}

		Vec3Subtract (origin, client->edict->s.origin, sourceVec);
		distanceMult = attenuation * ((attenuation == ATTN_STATIC) ? 0.001f : 0.0005f);

		dist = VectorNormalizef (sourceVec, sourceVec) - SOUND_FULLVOLUME;
		if (dist < 0)
			dist = 0;			// close enough to be at full volume
		dist *= distanceMult;	// different attenuation levels

		Angles_Vectors (client->edict->s.angles, NULL, listenerRight, NULL);
		dot = DotProduct (listenerRight, sourceVec);

		if (!distanceMult) {
			// no attenuation = no spatialization
			rightScale = 1.0f;
			leftScale = 1.0f;
		}
		else {
			rightScale = 0.5f * (1.0f + dot);
			leftScale = 0.5f * (1.0f - dot);
		}

		// add in distance effect
		rightVol = (vol * ((1.0f - dist) * rightScale));
		leftVol = (vol * ((1.0f - dist) * leftScale));

		if (rightVol <= 0 && leftVol <= 0)
			continue; // silent

		MSG_WriteByte (&sv.multiCast, SVC_SOUND);
		MSG_WriteByte (&sv.multiCast, flags);
		MSG_WriteByte (&sv.multiCast, soundIndex);

		if (flags & SND_VOLUME)			MSG_WriteByte (&sv.multiCast, vol*255);
		if (flags & SND_ATTENUATION)	MSG_WriteByte (&sv.multiCast, attenuation*64);
		if (flags & SND_OFFSET)			MSG_WriteByte (&sv.multiCast, timeOffset*1000);
		if (flags & SND_ENT)			MSG_WriteShort (&sv.multiCast, sendChan);
		if (flags & SND_POS)			MSG_WritePos (&sv.multiCast, origin);

		SV_Unicast (client->edict, (channel & CHAN_RELIABLE)?qTrue:qFalse);
	}
}

/*
===============================================================================

	FRAME UPDATES

===============================================================================
*/

/*
=======================
SV_SendClientDatagram
=======================
*/
qBool SV_SendClientDatagram (svClient_t *client)
{
	byte		msgBuf[MAX_SV_MSGLEN];
	netMsg_t	msg;

	SV_BuildClientFrame (client);

	MSG_Init (&msg, msgBuf, sizeof (msgBuf));
	msg.allowOverflow = qTrue;

	// Send over all the relevant entityStateOld_t and the playerState_t
	SV_WriteFrameToClient (client, &msg);

	// Copy the accumulated multicast datagram for this client out to the message it is
	// necessary for this to be after the WriteEntities so that entity references will be current
	if (client->datagram.overFlowed) {
		Com_Printf (PRNT_WARNING, "WARNING: datagram overflowed for %s\n", client->name);
	}
	else if (client->datagram.curSize) {
		MSG_WriteRaw (&msg, client->datagram.data, client->datagram.curSize);
	}

	MSG_Clear (&client->datagram);

	if (msg.overFlowed) {
		// Must have room left for the packet header
		Com_Printf (PRNT_WARNING, "WARNING: msg overflowed for %s\n", client->name);
		MSG_Clear (&msg);
	}

	// Send the datagram
	Netchan_Transmit (&client->netChan, msg.curSize, msg.data);

	// Record the size for rate estimation
	client->messageSize[sv.frameNum % RATE_MESSAGES] = msg.curSize;

	return qTrue;
}


/*
==================
SV_DemoCompleted
==================
*/
static void SV_DemoCompleted (void)
{
	if (sv.demoFile) {
		FS_CloseFile (sv.demoFile);
		sv.demoFile = 0;
	}

	SV_Nextserver ();
}


/*
=======================
SV_RateDrop

Returns qTrue if the client is over its current
bandwidth estimation and should not be sent another packet
=======================
*/
qBool SV_RateDrop (svClient_t *c)
{
	size_t	total;
	int		i;

	// never drop over the loopback
	if (c->netChan.remoteAddress.naType == NA_LOOPBACK)
		return qFalse;

	total=0;
	for (i=0 ; i<RATE_MESSAGES ; i++)
		total += c->messageSize[i];

	if (total > c->rate) {
		c->surpressCount++;
		c->messageSize[sv.frameNum % RATE_MESSAGES] = 0;
		return qTrue;
	}

	return qFalse;
}


/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages (void)
{
	int			i;
	svClient_t	*c;
	int			msgLen;
	byte		msgBuf[MAX_SV_MSGLEN];
	int			r;

	msgLen = 0;

	// Read the next demo message if needed
	if (Com_ServerState () == SS_DEMO && sv.demoFile) {
		if (sv_paused->intVal) {
			msgLen = 0;
		}
		else {
			// Get the next message
			r = (int) FS_Read (&msgLen, sizeof (r), sv.demoFile);
			if (r != 4) {
				SV_DemoCompleted ();
				return;
			}

			msgLen = LittleLong (msgLen);
			if (msgLen == -1) {
				SV_DemoCompleted ();
				return;
			}

			if (msgLen > MAX_SV_MSGLEN)
				Com_Error (ERR_DROP, "SV_SendClientMessages: msgLen > MAX_SV_MSGLEN");

			r = (int) FS_Read (msgBuf, msgLen, sv.demoFile);
			if (r != msgLen) {
				SV_DemoCompleted ();
				return;
			}
		}
	}

	// Send a message to each connected client
	for (i=0, c=svs.clients ; i<maxclients->intVal ; i++, c++) {
		if (!c->state)
			continue;

		// If the reliable message overflowed, drop the client
		if (c->netChan.message.overFlowed) {
			MSG_Clear (&c->netChan.message);
			MSG_Clear (&c->datagram);
			SV_BroadcastPrintf (PRINT_HIGH, "%s overflowed\n", c->name);
			SV_DropClient (c);
		}

		switch (Com_ServerState ()) {
		case SS_CINEMATIC:
		case SS_DEMO:
		case SS_PIC:
			Netchan_Transmit (&c->netChan, msgLen, msgBuf);
			break;

		default:
			if (c->state == SVCS_SPAWNED) {
				// Don't overrun bandwidth
				if (SV_RateDrop (c))
					continue;

				SV_SendClientDatagram (c);
			}
			else {
				// Just update reliable	if needed
				if (c->netChan.message.curSize	|| Sys_Milliseconds () - c->netChan.lastSent > 100)
					Netchan_Transmit (&c->netChan, 0, NULL);
			}
			break;
		}
	}
}
