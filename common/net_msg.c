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
// net_msg.c
//

#include "common.h"

/*
==============================================================================

	SUPPORTING FUNCTIONS

==============================================================================
*/

/*
================
MSG_Init
================
*/
void MSG_Init (netMsg_t *dest, byte *data, size_t length)
{
	assert (length > 0);

	memset (dest, 0, sizeof (*dest));
	dest->data = data;
	dest->maxSize = length;
	dest->bufferSize = length;
}


/*
================
MSG_Clear
================
*/
void MSG_Clear (netMsg_t *dest)
{
#ifdef _DEBUG
	memset (dest->data, 0xcc, dest->bufferSize);
#endif

	dest->curSize = 0;
	dest->overFlowed = qFalse;
}

/*
==============================================================================

	WRITING FUNCTIONS

==============================================================================
*/

/*
================
MSG_GetWriteSpace
================
*/
static void *MSG_GetWriteSpace (netMsg_t *dest, size_t length)
{
	void	*data;

	assert (length > 0);
	if (dest->curSize + length > dest->maxSize) {
		if (!dest->data)
			Com_Error (ERR_FATAL, "MSG_GetWriteSpace: attempted to write %d bytes to an uninitialized buffer!", length);

		if (!dest->allowOverflow) {
			if (length > dest->maxSize)
				Com_Error (ERR_FATAL, "MSG_GetWriteSpace: %i is > full buffer size %d (%d)", length, dest->maxSize, dest->bufferSize);

			Com_Error (ERR_FATAL, "MSG_GetWriteSpace: overflow without allowOverflow set (%d+%d > %d)", dest->curSize, length, dest->maxSize);
		}

		// R1: clear the buffer BEFORE the error!! (for console buffer)
		if (dest->curSize + length >= dest->bufferSize) {
			MSG_Clear (dest);
			Com_Printf (PRNT_WARNING, "MSG_GetWriteSpace: overflow\n");
		}
		else {
			Com_Printf (PRNT_WARNING, "MSG_GetWriteSpace: overflowed maxSize\n");
		}

		dest->overFlowed = qTrue;
	}

	data = dest->data + dest->curSize;
	dest->curSize += length;
	return data;
}


/*
================
MSG_WriteByte
================
*/
void MSG_WriteByte (netMsg_t *dest, int c)
{
	byte	*buf;

	assert (!(c < 0 || c > 255));

	buf = MSG_GetWriteSpace (dest, 1);
	buf[0] = c;
}


/*
================
MSG_WriteChar
================
*/
void MSG_WriteChar (netMsg_t *dest, int c)
{
	byte	*buf;

	assert (!(c < -128 || c > 127));

	buf = MSG_GetWriteSpace (dest, 1);
	buf[0] = c;
}


/*
==================
MSG_WriteDeltaEntity

Writes part of a packetentities message.
Can delta from either a baseline or a previous packet_entity
==================
*/
void MSG_WriteDeltaEntity (netMsg_t *dest, entityStateOld_t *from, entityStateOld_t *to, qBool force, qBool newEntity)
{
	int		bits;

	if (!to) {
		bits = U_REMOVE;
		if (from->number >= 256)
			bits |= U_NUMBER16 | U_MOREBITS1;

		MSG_WriteByte (dest, bits&255);
		if (bits & 0x0000ff00)
			MSG_WriteByte (dest, (bits>>8)&255);

		if (bits & U_NUMBER16)
			MSG_WriteShort (dest, from->number);
		else
			MSG_WriteByte (dest, from->number);
		return;
	}

	if (!to->number)
		Com_Error (ERR_FATAL, "MSG_WriteDeltaEntity: Unset entity number");
	if (to->number >= MAX_CS_EDICTS)
		Com_Error (ERR_FATAL, "MSG_WriteDeltaEntity: Entity number >= MAX_CS_EDICTS");

	// Send an update
	bits = 0;
	if (to->number >= 256)
		bits |= U_NUMBER16;		// number8 is implicit otherwise

	if (to->origin[0] != from->origin[0])		bits |= U_ORIGIN1;
	if (to->origin[1] != from->origin[1])		bits |= U_ORIGIN2;
	if (to->origin[2] != from->origin[2])		bits |= U_ORIGIN3;

	if (to->angles[0] != from->angles[0])		bits |= U_ANGLE1;		
	if (to->angles[1] != from->angles[1])		bits |= U_ANGLE2;
	if (to->angles[2] != from->angles[2])		bits |= U_ANGLE3;
		
	if (to->skinNum != from->skinNum) {
		if ((uint32)to->skinNum < 256)			bits |= U_SKIN8;
		else if ((uint32)to->skinNum < 0x10000)	bits |= U_SKIN16;
		else									bits |= (U_SKIN8|U_SKIN16);
	}
		
	if (to->frame != from->frame) {
		if (to->frame < 256)	bits |= U_FRAME8;
		else					bits |= U_FRAME16;
	}

	if (to->effects != from->effects) {
		if (to->effects < 256)			bits |= U_EFFECTS8;
		else if (to->effects < 0x8000)	bits |= U_EFFECTS16;
		else							bits |= U_EFFECTS8|U_EFFECTS16;
	}
	
	if (to->renderFx != from->renderFx) {
		if (to->renderFx < 256)			bits |= U_RENDERFX8;
		else if (to->renderFx < 0x8000)	bits |= U_RENDERFX16;
		else							bits |= U_RENDERFX8|U_RENDERFX16;
	}
	
	if (to->solid != from->solid)
		bits |= U_SOLID;

	// Event is not delta compressed, just 0 compressed
	if (to->event)
		bits |= U_EVENT;
	
	if (to->modelIndex != from->modelIndex)		bits |= U_MODEL;
	if (to->modelIndex2 != from->modelIndex2)	bits |= U_MODEL2;
	if (to->modelIndex3 != from->modelIndex3)	bits |= U_MODEL3;
	if (to->modelIndex4 != from->modelIndex4)	bits |= U_MODEL4;

	if (to->sound != from->sound)
		bits |= U_SOUND;

	if (newEntity || to->renderFx & RF_FRAMELERP || to->renderFx & RF_BEAM)
		bits |= U_OLDORIGIN;

	//
	// Write the message
	//
	if (!bits && !force)
		return;		// Nothing to send!

	//----------

	if (bits & 0xff000000)		bits |= U_MOREBITS3 | U_MOREBITS2 | U_MOREBITS1;
	else if (bits & 0x00ff0000)	bits |= U_MOREBITS2 | U_MOREBITS1;
	else if (bits & 0x0000ff00)	bits |= U_MOREBITS1;

	MSG_WriteByte (dest, bits&255);
	if (bits & 0xff000000) {
		MSG_WriteByte (dest, (bits>>8)&255);
		MSG_WriteByte (dest, (bits>>16)&255);
		MSG_WriteByte (dest, (bits>>24)&255);
	}
	else if (bits & 0x00ff0000) {
		MSG_WriteByte (dest, (bits>>8)&255);
		MSG_WriteByte (dest, (bits>>16)&255);
	}
	else if (bits & 0x0000ff00) {
		MSG_WriteByte (dest, (bits>>8)&255);
	}

	//----------

	if (bits & U_NUMBER16)	MSG_WriteShort (dest, to->number);
	else					MSG_WriteByte (dest, to->number);

	if (bits & U_MODEL)		MSG_WriteByte (dest, to->modelIndex);
	if (bits & U_MODEL2)	MSG_WriteByte (dest, to->modelIndex2);
	if (bits & U_MODEL3)	MSG_WriteByte (dest, to->modelIndex3);
	if (bits & U_MODEL4)	MSG_WriteByte (dest, to->modelIndex4);

	if (bits & U_FRAME8)	MSG_WriteByte (dest, to->frame);
	if (bits & U_FRAME16)	MSG_WriteShort (dest, to->frame);

	// Used for laser colors
	if ((bits & U_SKIN8) && (bits & U_SKIN16))	MSG_WriteLong (dest, to->skinNum);
	else if (bits & U_SKIN8)					MSG_WriteByte (dest, to->skinNum);
	else if (bits & U_SKIN16)					MSG_WriteShort (dest, to->skinNum);

	if ((bits & (U_EFFECTS8|U_EFFECTS16)) == (U_EFFECTS8|U_EFFECTS16))		MSG_WriteLong (dest, to->effects);
	else if (bits & U_EFFECTS8)		MSG_WriteByte (dest, to->effects);
	else if (bits & U_EFFECTS16)	MSG_WriteShort (dest, to->effects);

	if ((bits & (U_RENDERFX8|U_RENDERFX16)) == (U_RENDERFX8|U_RENDERFX16))	MSG_WriteLong (dest, to->renderFx);
	else if (bits & U_RENDERFX8)	MSG_WriteByte (dest, to->renderFx);
	else if (bits & U_RENDERFX16)	MSG_WriteShort (dest, to->renderFx);

	if (bits & U_ORIGIN1)	MSG_WriteCoord (dest, to->origin[0]);		
	if (bits & U_ORIGIN2)	MSG_WriteCoord (dest, to->origin[1]);
	if (bits & U_ORIGIN3)	MSG_WriteCoord (dest, to->origin[2]);

	if (bits & U_ANGLE1)	MSG_WriteAngle (dest, to->angles[0]);
	if (bits & U_ANGLE2)	MSG_WriteAngle (dest, to->angles[1]);
	if (bits & U_ANGLE3)	MSG_WriteAngle (dest, to->angles[2]);

	if (bits & U_OLDORIGIN) {
		MSG_WriteCoord (dest, to->oldOrigin[0]);
		MSG_WriteCoord (dest, to->oldOrigin[1]);
		MSG_WriteCoord (dest, to->oldOrigin[2]);
	}

	if (bits & U_SOUND)	MSG_WriteByte (dest, to->sound);
	if (bits & U_EVENT)	MSG_WriteByte (dest, to->event);
	if (bits & U_SOLID)	MSG_WriteShort (dest, to->solid);
}


/*
================
MSG_WriteDeltaUsercmd
================
*/
void MSG_WriteDeltaUsercmd (netMsg_t *dest, userCmd_t *from, userCmd_t *cmd, int protocolMinorVersion)
{
	int bits;
	int buttons;

	// Send the movement message
	bits = 0;
	buttons = 0;

	if (cmd->angles[0] != from->angles[0])		bits |= CM_ANGLE1;
	if (cmd->angles[1] != from->angles[1])		bits |= CM_ANGLE2;
	if (cmd->angles[2] != from->angles[2])		bits |= CM_ANGLE3;
	if (cmd->forwardMove != from->forwardMove)	bits |= CM_FORWARD;
	if (cmd->sideMove != from->sideMove)		bits |= CM_SIDE;
	if (cmd->upMove != from->upMove)			bits |= CM_UP;
	if (cmd->buttons != from->buttons) {
		buttons = cmd->buttons;
		bits |= CM_BUTTONS;
	}
	if (cmd->impulse != from->impulse)			bits |= CM_IMPULSE;

	MSG_WriteByte (dest, bits);

	//waste not what precious bytes we have...
	if (protocolMinorVersion >= MINOR_VERSION_R1Q2_UCMD_UPDATES) {
		if (bits & CM_BUTTONS) {
			if ((bits & CM_FORWARD) && (cmd->forwardMove % 5) == 0)
				buttons |= BUTTON_UCMD_DBLFORWARD;
			if ((bits & CM_SIDE) && (cmd->sideMove % 5) == 0)
				buttons |= BUTTON_UCMD_DBLSIDE;
			if ((bits & CM_UP) && (cmd->upMove % 5) == 0)
				buttons |= BUTTON_UCMD_DBLUP;

			if ((bits & CM_ANGLE1) && (cmd->angles[0] % 64) == 0 && (abs(cmd->angles[0] / 64)) < 128)
				buttons |= BUTTON_UCMD_DBL_ANGLE1;
			if ((bits & CM_ANGLE2) && (cmd->angles[1] % 256) == 0)
				buttons |= BUTTON_UCMD_DBL_ANGLE2;

			MSG_WriteByte (dest, buttons);
		}
	}

	if (bits & CM_ANGLE1) {
		if (buttons & BUTTON_UCMD_DBL_ANGLE1)
			MSG_WriteChar (dest, cmd->angles[0] / 64);
		else
			MSG_WriteShort (dest, cmd->angles[0]);
	}
	if (bits & CM_ANGLE2) {
		if (buttons & BUTTON_UCMD_DBL_ANGLE2)
			MSG_WriteChar (dest, cmd->angles[1] / 256);
		else
			MSG_WriteShort (dest, cmd->angles[1]);
	}
	if (bits & CM_ANGLE3)	MSG_WriteShort (dest, cmd->angles[2]);
	
	if (bits & CM_FORWARD) {
		if (buttons & BUTTON_UCMD_DBLFORWARD)
			MSG_WriteChar (dest, cmd->forwardMove / 5);
		else
			MSG_WriteShort (dest, cmd->forwardMove);
	}
	if (bits & CM_SIDE) {
		if (buttons & BUTTON_UCMD_DBLSIDE)
			MSG_WriteChar (dest, cmd->sideMove / 5);
		else
			MSG_WriteShort (dest, cmd->sideMove);
	}
	if (bits & CM_UP) {
		if (buttons & BUTTON_UCMD_DBLUP)
			MSG_WriteChar (dest, cmd->upMove / 5);
		else
			MSG_WriteShort (dest, cmd->upMove);
	}

	if (protocolMinorVersion < MINOR_VERSION_R1Q2_UCMD_UPDATES) {
		if (bits & CM_BUTTONS)
			MSG_WriteByte (dest, cmd->buttons);
	}

	if (bits & CM_IMPULSE)	MSG_WriteByte (dest, cmd->impulse);

	MSG_WriteByte (dest, cmd->msec);
	MSG_WriteByte (dest, cmd->lightLevel);
}


/*
================
MSG_WriteDir
================
*/
void MSG_WriteDir (netMsg_t *dest, vec3_t dir)
{
	byte	best;

	best = DirToByte (dir);
	MSG_WriteByte (dest, best);
}


/*
================
MSG_WriteFloat
================
*/
void MSG_WriteFloat (netMsg_t *dest, float f)
{
	union {
		float	f;
		int		l;
	} dat;

	dat.f = f;
	dat.l = LittleLong (dat.l);

	MSG_WriteRaw (dest, &dat.l, 4);
}


/*
================
MSG_WriteInt3
================
*/
void MSG_WriteInt3 (netMsg_t *dest, int c)
{
	byte	*buf;

	buf = MSG_GetWriteSpace (dest, 3);
	buf[0] = c&0xff;
	buf[1] = (c>>8)&0xff;
	buf[2] = (c>>16)&0xff;
}


/*
================
MSG_WriteLong
================
*/
void MSG_WriteLong (netMsg_t *dest, int c)
{
	byte	*buf;

	buf = MSG_GetWriteSpace (dest, 4);
	buf[0] = c&0xff;
	buf[1] = (c>>8)&0xff;
	buf[2] = (c>>16)&0xff;
	buf[3] = c>>24;
}


/*
================
MSG_WriteRaw
================
*/
void MSG_WriteRaw (netMsg_t *dest, void *data, size_t length)
{
	assert (length > 0);
	memcpy (MSG_GetWriteSpace (dest, length), data, length);		
}


/*
================
MSG_WriteShort
================
*/
void MSG_WriteShort (netMsg_t *dest, int c)
{
	byte	*buf;

	buf = MSG_GetWriteSpace (dest, 2);
	buf[0] = c&0xff;
	buf[1] = c>>8;
}


/*
================
MSG_WriteString
================
*/
void MSG_WriteString (netMsg_t *dest, char *s)
{
	if (!s)
		MSG_WriteRaw (dest, "", 1);
	else
		MSG_WriteRaw (dest, s, (int)strlen(s)+1);
}


/*
================
MSG_WriteStringCat
================
*/
void MSG_WriteStringCat (netMsg_t *dest, char *data)
{
	size_t		len;
	
	len = strlen (data) + 1;
	assert (len > 1);

	if (dest->curSize) {
		if (dest->data[dest->curSize-1])
			memcpy ((byte *)MSG_GetWriteSpace (dest, len), data, len); // no trailing 0
		else
			memcpy ((byte *)MSG_GetWriteSpace (dest, len-1)-1, data, len); // write over trailing 0
	}
	else {
		memcpy ((byte *)MSG_GetWriteSpace (dest, len), data, len);
	}
}

/*
==============================================================================

	READING FUNCTIONS

==============================================================================
*/

/*
================
MSG_BeginReading
================
*/
void MSG_BeginReading (netMsg_t *src)
{
	src->readCount = 0;
}


/*
================
MSG_ReadByte
================
*/
int MSG_ReadByte (netMsg_t *src)
{
	int	c;

	if (src->readCount+1 > src->curSize)
		c = -1;
	else
		c = (byte)src->data[src->readCount];
	src->readCount++;

	return c;
}


/*
================
MSG_ReadChar

returns -1 if no more characters are available
================
*/
int MSG_ReadChar (netMsg_t *src)
{
	int	c;

	if (src->readCount+1 > src->curSize)
		c = -1;
	else
		c = (signed char)src->data[src->readCount];
	src->readCount++;

	return c;
}


/*
================
MSG_ReadData
================
*/
void MSG_ReadData (netMsg_t *src, void *data, size_t len)
{
	size_t		i;

	for (i=0 ; i<len ; i++)
		((byte *)data)[i] = MSG_ReadByte (src);
}


/*
================
MSG_ReadDeltaUsercmd
================
*/
void MSG_ReadDeltaUsercmd (netMsg_t *src, userCmd_t *from, userCmd_t *move)
{
	int bits;

	memcpy (move, from, sizeof (*move));

	bits = MSG_ReadByte (src);
		
	// Read current angles
	if (bits & CM_ANGLE1)	move->angles[0] = MSG_ReadShort (src);
	if (bits & CM_ANGLE2)	move->angles[1] = MSG_ReadShort (src);
	if (bits & CM_ANGLE3)	move->angles[2] = MSG_ReadShort (src);
		
	// Read movement
	if (bits & CM_FORWARD)	move->forwardMove = MSG_ReadShort (src);
	if (bits & CM_SIDE)		move->sideMove = MSG_ReadShort (src);
	if (bits & CM_UP)		move->upMove = MSG_ReadShort (src);
	
	// Read buttons
	if (bits & CM_BUTTONS)
		move->buttons = MSG_ReadByte (src);

	if (bits & CM_IMPULSE)
		move->impulse = MSG_ReadByte (src);

	// Read time to run command
	move->msec = MSG_ReadByte (src);

	// Read the light level
	move->lightLevel = MSG_ReadByte (src);
}


/*
================
MSG_ReadDir
================
*/
void MSG_ReadDir (netMsg_t *src, vec3_t dir)
{
	byte	b;

	b = MSG_ReadByte (src);
	ByteToDir (b, dir);
}


/*
================
MSG_ReadFloat
================
*/
float MSG_ReadFloat (netMsg_t *src)
{
	union {
		byte	b[4];
		float	f;
		int		l;
	} dat;

	if (src->readCount+4 > src->curSize) {
		dat.f = -1;
	}
	else {
		dat.b[0] = src->data[src->readCount];
		dat.b[1] = src->data[src->readCount+1];
		dat.b[2] = src->data[src->readCount+2];
		dat.b[3] = src->data[src->readCount+3];
	}
	src->readCount += 4;

	dat.l = LittleLong (dat.l);

	return dat.f;	
}


/*
================
MSG_ReadInt3
================
*/
int MSG_ReadInt3 (netMsg_t *src)
{
	int	c;

	if (src->readCount+3 > src->curSize) {
		c = -1;
	}
	else {
		c = src->data[src->readCount]
		| (src->data[src->readCount+1]<<8)
		| (src->data[src->readCount+2]<<16)
		| ((src->data[src->readCount+2] & 0x80) ? ~0xFFFFFF : 0);
	}
	src->readCount += 3;

	return c;
}


/*
================
MSG_ReadLong
================
*/
int MSG_ReadLong (netMsg_t *src)
{
	int	c;

	if (src->readCount+4 > src->curSize) {
		c = -1;
	}
	else {
		c = src->data[src->readCount]
		+ (src->data[src->readCount+1]<<8)
		+ (src->data[src->readCount+2]<<16)
		+ (src->data[src->readCount+3]<<24);
	}
	src->readCount += 4;

	return c;
}


/*
================
MSG_ReadShort
================
*/
int MSG_ReadShort (netMsg_t *src)
{
	int	c;

	if (src->readCount+2 > src->curSize) {
		c = -1;
	}
	else {
		c = (int16)(src->data[src->readCount]
		+ (src->data[src->readCount+1]<<8));
	}
	src->readCount += 2;

	return c;
}


/*
================
MSG_ReadString
================
*/
char *MSG_ReadString (netMsg_t *src)
{
	static char	string[2048];
	int			l, c;

	l = 0;
	do {
		c = MSG_ReadByte (src);
		if (c == -1 || c == 0)
			break;

		string[l] = c;
		l++;
	} while (l < sizeof (string)-1);

	string[l] = 0;

	return string;
}


/*
================
MSG_ReadStringLine
================
*/
char *MSG_ReadStringLine (netMsg_t *src)
{
	static char	string[2048];
	int			l, c;

	l = 0;
	do {
		c = MSG_ReadByte (src);
		if (c == -1 || c == 0 || c == '\n')
			break;

		string[l] = c;
		l++;
	} while (l < sizeof (string)-1);

	string[l] = 0;

	return string;
}
