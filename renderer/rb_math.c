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
// rb_math.c
//

#include "rb_local.h"

/*
==============
RB_FastSin
==============
*/
float RB_FastSin (float t)
{
	return FTABLE_EVALUATE(rb_sinTable, t);
}


/*
==============
RB_TableForFunc
==============
*/
float *RB_TableForFunc (matTableFunc_t func)
{
	switch (func) {
	case MAT_FUNC_SIN:				return rb_sinTable;
	case MAT_FUNC_TRIANGLE:			return rb_triangleTable;
	case MAT_FUNC_SQUARE:			return rb_squareTable;
	case MAT_FUNC_SAWTOOTH:			return rb_sawtoothTable;
	case MAT_FUNC_INVERSESAWTOOTH:	return rb_inverseSawtoothTable;
	case MAT_FUNC_NOISE:				return rb_noiseTable;
	}

	// Assume error
	Com_Error (ERR_DROP, "RB_TableForFunc: unknown function");

	return NULL;
}


/*
===============
Matrix4_Copy2D
===============
*/
void Matrix4_Copy2D (const mat4x4_t m1, mat4x4_t m2)
{
	m2[0] = m1[0];
	m2[1] = m1[1];
	m2[4] = m1[4];
	m2[5] = m1[5];
	m2[12] = m1[12];
	m2[13] = m1[13];
}


/*
===============
Matrix4_Multiply2D
===============
*/
void Matrix4_Multiply2D (const mat4x4_t m1, const mat4x4_t m2, mat4x4_t out)
{
	out[0]  = m1[0] * m2[0] + m1[4] * m2[1];
	out[1]  = m1[1] * m2[0] + m1[5] * m2[1];
	out[4]  = m1[0] * m2[4] + m1[4] * m2[5];
	out[5]  = m1[1] * m2[4] + m1[5] * m2[5];
	out[12] = m1[0] * m2[12] + m1[4] * m2[13] + m1[12];
	out[13] = m1[1] * m2[12] + m1[5] * m2[13] + m1[13];
}


/*
===============
Matrix4_Scale2D
===============
*/
void Matrix4_Scale2D (mat4x4_t m, float x, float y)
{
	m[0] *= x;
	m[1] *= x;
	m[4] *= y;
	m[5] *= y;
}


/*
===============
Matrix4_Stretch2D
===============
*/
void Matrix4_Stretch2D (mat4x4_t m, float s, float t)
{
	m[0] *= s;
	m[1] *= s;
	m[4] *= s;
	m[5] *= s;
	m[12] = s * m[12] + t;
	m[13] = s * m[13] + t;
}


/*
===============
Matrix4_Translate2D
===============
*/
void Matrix4_Translate2D (mat4x4_t m, float x, float y)
{
	m[12] += x;
	m[13] += y;
}
