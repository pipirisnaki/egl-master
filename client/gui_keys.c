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
// gui_keys.c
// FIXME TODO:
// - Check for arrow keys/keypad arrows
//

#include "gui_local.h"

/*
=================
GUI_KeyUp
=================
*/
void GUI_KeyUp (keyNum_t keyNum)
{
	gui_t		*gui;

	if (!cl_guiState.inputWindow)
		return;

	gui = cl_guiState.inputWindow->shared->cursor.curWindow;
	if (!gui)
		return;
}


/*
=================
GUI_KeyDown
=================
*/
static void GUI_r_WantEnter (gui_t *gui)
{
	gui_t	*child;
	uint32	i;

	if (FRVALUE (gui, FR_WANT_ENTER))
		GUI_QueueTrigger (gui, WEV_ACTION);

	for (i=0, child=gui->childList ; i<gui->numChildren ; child++, i++)
		GUI_r_WantEnter (child);
}
void GUI_KeyDown (keyNum_t keyNum)
{
	gui_t	*gui;

	if (!cl_guiState.inputWindow)
		return;

	gui = cl_guiState.inputWindow->shared->cursor.curWindow;
	if (!gui)
		return;

	switch (keyNum) {
	case K_ESCAPE:
		GUI_QueueTrigger (gui, WEV_ESCAPE);
		break;

	case K_ENTER:
		GUI_r_WantEnter (gui->owner);
	case K_MOUSE1:
		GUI_QueueTrigger (gui, WEV_ACTION);
		break;

	case K_UPARROW:
	case K_KP_UPARROW:
	case K_DOWNARROW:
	case K_KP_DOWNARROW:
	case K_LEFTARROW:
	case K_KP_LEFTARROW:
	case K_RIGHTARROW:
	case K_KP_RIGHTARROW:
		GUI_AdjustCursor (keyNum);
		break;
	case K_BADKEY:
	case K_TAB:
	case K_SPACE:
	case K_BACKSPACE:
	case K_ALT:
	case K_CTRL:
	case K_SHIFT:
	case K_LSHIFT:
	case K_RSHIFT:
	case K_CAPSLOCK:
	case K_F1:
	case K_F2:
	case K_F3:
	case K_F4:
	case K_F5:
	case K_F6:
	case K_F7:
	case K_F8:
	case K_F9:
	case K_F10:
	case K_F11:
	case K_F12:
	case K_INS:
	case K_DEL:
	case K_PGDN:
	case K_PGUP:
	case K_HOME:
	case K_END:
	case K_KP_HOME:
	case K_KP_PGUP:
	case K_KP_FIVE:
	case K_KP_END:
	case K_KP_PGDN:
	case K_KP_ENTER:
	case K_KP_INS:
	case K_KP_DEL:
	case K_KP_SLASH:
	case K_KP_MINUS:
	case K_KP_PLUS:
	case K_MOUSE2:
	case K_MOUSE3:
	case K_MOUSE4:
	case K_MOUSE5:
	case K_JOY1:
	case K_JOY2:
	case K_JOY3:
	case K_JOY4:
	case K_AUX1:
	case K_AUX2:
	case K_AUX3:
	case K_AUX4:
	case K_AUX5:
	case K_AUX6:
	case K_AUX7:
	case K_AUX8:
	case K_AUX9:
	case K_AUX10:
	case K_AUX11:
	case K_AUX12:
	case K_AUX13:
	case K_AUX14:
	case K_AUX15:
	case K_AUX16:
	case K_AUX17:
	case K_AUX18:
	case K_AUX19:
	case K_AUX20:
	case K_AUX21:
	case K_AUX22:
	case K_AUX23:
	case K_AUX24:
	case K_AUX25:
	case K_AUX26:
	case K_AUX27:
	case K_AUX28:
	case K_AUX29:
	case K_AUX30:
	case K_AUX31:
	case K_AUX32:
	case K_MWHEELDOWN:
	case K_MWHEELUP:
	case K_MWHEELLEFT:
	case K_MWHEELRIGHT:
	case K_PAUSE:
	case K_MAXKEYS:
		break;
	}
}
