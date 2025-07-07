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
// cg_parttrail.c
//

#include "cg_local.h"

/*
=============================================================================

	PARTICLE TRAILS

=============================================================================
*/

/*
===============
CG_BeamTrail
===============
*/
void CG_BeamTrail (vec3_t start, vec3_t end, int color, float size, float alpha, float alphaVel)
{
	vec3_t		dest, move, vec;
	float		len, dec;
	float		run;

	Vec3Copy (start, move);
	Vec3Subtract (end, start, vec);
	len = VectorNormalizeFastf (vec);

	dec = 35.0f - (rand () % 27);
	Vec3Scale (vec, dec, vec);

	for (; len>0 ; Vec3Add (move, vec, move)) {
		len -= dec;

		if ((crand () > 0) || (rand () % 14))
			continue;

		Vec3Copy (move, dest);

		dest[0] += ((rand () % ((int)(size*32) + 1) - (int)(size*16))/32);
		dest[1] += ((rand () % ((int)(size*32) + 1) - (int)(size*16))/32);
		dest[2] += ((rand () % ((int)(size*32) + 1) - (int)(size*16))/32);

		run = (frand () * 50);
		CG_SpawnParticle (
			dest[0],							dest[1],							dest[2],
			0,									0,									0,
			0,									0,									0,
			0,									0,									0,
			palRed (color) + run,				palGreen (color) + run,				palBlue (color) + run,
			palRed (color) + run,				palGreen (color) + run,				palBlue (color) + run,
			alpha,								alphaVel,
			size / 3.0f,						0.1f + (frand () * 0.1f),
			PT_FLAREGLOW,						0,
			0,									qFalse,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_BfgTrail
===============
*/
void CG_BfgTrail (refEntity_t *ent)
{
	int			i;
	float		angle, sr, sp, sy, cr, cp, cy;
	float		ltime, size, dist = 64;
	vec3_t		forward, org;

	// Outer
	CG_SpawnParticle (
		ent->origin[0],					ent->origin[1],					ent->origin[2],
		0,								0,								0,
		0,								0,								0,
		0,								0,								0,
		0,								200,							0,
		0,								200,							0,
		0.66f,							PART_INSTANT,
		60,								60,
		PT_BFG_DOT,						PF_SCALED,
		0,								qFalse,
		PART_STYLE_QUAD,
		0);

	// Core
	CG_FlareEffect (ent->origin, PT_FLAREGLOW, 0 + (frand () * 32), 45, 45, 0xd0, 0xd0, 0.66f, PART_INSTANT);
	CG_FlareEffect (ent->origin, PT_FLAREGLOW, 180 + (frand () * 32), 55, 55, 0xd0, 0xd0, 0.66f, PART_INSTANT);

	CG_FlareEffect (ent->origin, PT_FLAREGLOW, 0 + (frand () * 32), 35, 35, 0xd7, 0xd7, 0.66f, PART_INSTANT);
	CG_FlareEffect (ent->origin, PT_FLAREGLOW, 180 + (frand () * 32), 45, 45, 0xd7, 0xd7, 0.66f, PART_INSTANT);

	ltime = (float)cg.realTime / 1000.0f;
	for (i=0 ; i<(NUMVERTEXNORMALS/2.0f) ; i++) {
		angle = ltime * cg_randVels[i][0];
		sy = (float)sin (angle);
		cy = (float)cos (angle);
		angle = ltime * cg_randVels[i][1];
		sp = (float)sin (angle);
		cp = (float)cos (angle);
		angle = ltime * cg_randVels[i][2];
		sr = (float)sin (angle);
		cr = (float)cos (angle);
	
		Vec3Set (forward, cp*cy, cp*sy, -sp);

		org[0] = ent->origin[0] + (m_byteDirs[i][0] * (float)sin (ltime + i) * 64) + (forward[0] * BEAMLENGTH);
		org[1] = ent->origin[1] + (m_byteDirs[i][1] * (float)sin (ltime + i) * 64) + (forward[0] * BEAMLENGTH);
		org[2] = ent->origin[2] + (m_byteDirs[i][2] * (float)sin (ltime + i) * 64) + (forward[0] * BEAMLENGTH);

		dist = Vec3Dist (org, ent->origin) / 90.0f;

		size = (2 - (dist * 2 + 0.1f)) * 12;
		CG_SpawnParticle (
			org[0],								org[1],								org[2],
			0,									0,									0,
			0,									0,									0,
			0,									0,									0,
			115 + ((1 - (dist * 3)) * 40),		180 + ((1 - (dist * 3)) * 75),		115 + ((1 - (dist * 3)) * 40),
			115 + ((1 - (dist * 3)) * 40),		180 + ((1 - (dist * 3)) * 75),		115 + ((1 - (dist * 3)) * 40),
			0.95f - dist/2.0f,					-100,
			size,								size,
			PT_BFG_DOT,							PF_SCALED|PF_NOCLOSECULL,
			0,									qFalse,
			PART_STYLE_QUAD,
			0);

		// Sparks
		if (!(rand () & 15)) {
			CG_SpawnParticle (
				org[0] + (crand () * 4),			org[1] + (crand () * 4),			org[2] + (crand () * 4),
				0,									0,									0,
				crand () * 16,						crand () * 16,						crand () * 16,
				0,									0,									-20,
				115 + ((1 - (dist * 3)) * 40),		180 + ((1 - (dist * 3)) * 75),		115 + ((1 - (dist * 3)) * 40),
				115 + ((1 - (dist * 3)) * 40),		180 + ((1 - (dist * 3)) * 75),		115 + ((1 - (dist * 3)) * 40),
				1.0f,								-0.9f / (0.4f + (frand () * 0.3f)),
				0.5f + (frand () * 0.25f),			0.4f + (frand () * 0.25f),
				PT_BFG_DOT,							PF_SCALED,
				pSplashThink,						qTrue,
				PART_STYLE_DIRECTION,
				PMAXSPLASHLEN);
		}
	}
}


/*
===============
CG_BlasterGoldTrail
===============
*/
void CG_BlasterGoldTrail (vec3_t start, vec3_t end)
{
	vec3_t		move, vec;
	float		len, orgscale, velscale;
	int			dec, rnum, rnum2;

	// Bubbles
	if (rand () % 2)
		CG_BubbleEffect (start);

	// Dot trail
	Vec3Copy (start, move);
	Vec3Subtract (end, start, vec);
	len = VectorNormalizeFastf (vec);

	dec = 5;
	Vec3Scale (vec, dec, vec);

	if (cg.currGameMod == GAME_MOD_GLOOM) {
		orgscale = 0.5f;
		velscale = 3.0f;
	}
	else {
		orgscale = 0.75f;
		velscale = 4.0f;
	}

	for (; len>0 ; Vec3Add (move, vec, move)) {
		len -= dec;

		rnum = (rand () % 5);
		rnum2 = (rand () % 5);
		CG_SpawnParticle (
			move[0] + (crand () * orgscale),	move[1] + (crand () * orgscale),	move[2] + (crand () * orgscale),
			0,									0,									0,
			crand () * velscale,				crand () * velscale,				crand () * velscale,
			0,									0,									0,
			palRed (0xe0 + rnum),				palGreen (0xe0 + rnum),				palBlue (0xe0 + rnum),
			palRed (0xe0 + rnum2),				palGreen (0xe0 + rnum2),			palBlue (0xe0 + rnum2),
			1.0f,								-1.0f / (0.25f + (crand () * 0.05f)),
			7 + crand (),						frand () * 5,
			PT_BLASTER_RED,						PF_NOCLOSECULL,
			0,									qFalse,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_BlasterGreenTrail
===============
*/
void CG_BlasterGreenTrail (vec3_t start, vec3_t end)
{
	vec3_t		move, vec;
	float		len, orgscale, velscale;
	int			dec, rnum, rnum2;

	// bubbles
	if (rand () % 2)
		CG_BubbleEffect (start);

	Vec3Copy (start, move);
	Vec3Subtract (end, start, vec);
	len = VectorNormalizeFastf (vec);

	dec = 5;
	Vec3Scale (vec, dec, vec);

	if (cg.currGameMod == GAME_MOD_GLOOM) {
		orgscale = 0.5;
		velscale = 3;
	}
	else {
		orgscale = 1;
		velscale = 5;
	}

	for (; len>0 ; Vec3Add (move, vec, move)) {
		len -= dec;

		rnum = (rand () % 5);
		rnum2 = (rand () % 5);

		CG_SpawnParticle (
			move[0] + (crand () * orgscale),	move[1] + (crand () * orgscale),	move[2] + (crand ()* orgscale),
			0,									0,									0,
			crand () * velscale,				crand () * velscale,				crand () * velscale,
			0,									0,									0,
			palRed (0xd0 + rnum),				palGreen (0xd0 + rnum),				palBlue (0xd0 + rnum),
			palRed (0xd0 + rnum2),				palGreen (0xd0 + rnum2),			palBlue (0xd0 + rnum2),
			1.0f,								-1.0f / (0.25f + (crand () * 0.05f)),
			5,									1,
			PT_BLASTER_GREEN,					PF_NOCLOSECULL,
			0,									qFalse,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_BubbleTrail
===============
*/
void CG_BubbleTrail (vec3_t start, vec3_t end)
{
	vec3_t		move, vec;
	float		len;
	float		rnum, rnum2;
	int			i, dec;

	Vec3Copy (start, move);
	Vec3Subtract (end, start, vec);
	len = VectorNormalizeFastf (vec);

	dec = 32;
	Vec3Scale (vec, dec, vec);

	for (i=0 ; i<len ; i+=dec, Vec3Add (move, vec, move)) {
		rnum = 230 + (frand () * 25);
		rnum2 = 230 + (frand () * 25);

		CG_SpawnParticle (
			move[0] + crand (),				move[1] + crand (),				move[2] + crand (),
			0,								0,								0,
			crand () * 4,					crand () * 4,					10 + (crand () * 4),
			0,								0,								0,
			rnum,							rnum,							rnum,
			rnum2,							rnum2,							rnum2,
			0.9f + (crand () * 0.1f),		-1.0f / (3 + (frand () * 0.2f)),
			0.1f + frand (),				0.1f + frand (),
			PT_WATERBUBBLE,					PF_SHADE|PF_LAVAONLY|PF_SLIMEONLY|PF_WATERONLY|PF_NOCLOSECULL,
			0,								qFalse,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_BubbleTrail2

lets you control the number of bubbles by setting the distance between the spawns
===============
*/
void CG_BubbleTrail2 (vec3_t start, vec3_t end, int dist)
{
	vec3_t		move, vec;
	float		len;
	float		rnum, rnum2;
	int			i, dec;

	Vec3Copy (start, move);
	Vec3Subtract (end, start, vec);
	len = VectorNormalizeFastf (vec);

	dec = dist;
	Vec3Scale (vec, dec, vec);

	for (i=0 ; i<len ; i+=dec, Vec3Add (move, vec, move)) {
		rnum = 230 + (frand () * 25);
		rnum2 = 230 + (frand () * 25);

		CG_SpawnParticle (
			move[0] + crand (),				move[1] + crand (),				move[2] + crand (),
			0,								0,								0,
			crand () * 4,					crand () * 4,					10 + (crand () * 4),
			0,								0,								0,
			rnum,							rnum,							rnum,
			rnum2,							rnum2,							rnum2,
			0.9f + (crand () * 0.1f),		-1.0f / (3 + (frand () * 0.1f)),
			0.1f + frand (),				0.1f + frand (),
			PT_WATERBUBBLE,					PF_SHADE|PF_LAVAONLY|PF_SLIMEONLY|PF_WATERONLY|PF_NOCLOSECULL,
			0,								qFalse,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_DebugTrail
===============
*/
void CG_DebugTrail (vec3_t start, vec3_t end)
{
	vec3_t		move, vec, right, up;
	float		len, dec, rnum, rnum2;

	Vec3Copy (start, move);
	Vec3Subtract (end, start, vec);
	len = VectorNormalizeFastf (vec);

	MakeNormalVectorsf (vec, right, up);

	dec = 3;
	Vec3Scale (vec, dec, vec);
	Vec3Copy (start, move);

	for (; len>0 ; Vec3Add (move, vec, move)) {
		len -= dec;

		rnum = frand () * 40;
		rnum2 = frand () * 40;
		CG_SpawnParticle (
			move[0],						move[1],						move[2],
			0,								0,								0,
			0,								0,								0,
			0,								0,								0,
			0,								rnum,							215 + rnum,
			0,								rnum2,							215 + rnum2,
			1.0f,							-0.1f,
			3.0f,							1.0f,
			PT_BLASTER_BLUE,				PF_SCALED|PF_NOCLOSECULL,
			0,								qFalse,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_FlagTrail
===============
*/
void CG_FlagTrail (vec3_t start, vec3_t end, int flags)
{
	vec3_t		move, vec;
	float		len, dec;
	float		clrmod;

	Vec3Copy (start, move);
	Vec3Subtract (end, start, vec);
	len = VectorNormalizeFastf (vec);

	dec = 4;
	Vec3Scale (vec, dec, vec);

	// red
	if (flags & EF_FLAG1) {
		for (; len>0 ; Vec3Add (move, vec, move)) {
			len -= dec;

			clrmod = (rand () % 2) ? (rand () % 170) : 0.0f;
			CG_SpawnParticle (
				move[0] + (crand () * 6),		move[1] + (crand () * 6),		move[2] + (crand () * 6),
				0,								0,								0,
				(crand () * 8),					(crand () * 8),					(crand () * 8),
				0,								0,								0,
				140 + (frand () * 50) + clrmod,	clrmod,							clrmod,
				140 + (frand () * 50) + clrmod,	clrmod,							clrmod,
				1.0f,							-1.0f / (0.8f + (frand () * 0.2f)),
				5,								2,
				PT_FLAREGLOW,					PF_SCALED|PF_NOCLOSECULL,
				0,								qFalse,
				PART_STYLE_QUAD,
				0);
		}
	}

	// blue
	if (flags & EF_FLAG2) {
		for (; len>0 ; Vec3Add (move, vec, move)) {
			len -= dec;

			clrmod = (rand () % 2) ? (rand () % 170) : 0.0f;
			CG_SpawnParticle (
				move[0] + (crand () * 6),		move[1] + (crand () * 6),		move[2] + (crand () * 6),
				0,								0,								0,
				crand () * 8,					crand () * 8,					crand () * 8,
				0,								0,								0,
				clrmod,							clrmod + (frand () * 70),		clrmod + 230 + (frand () * 50),
				clrmod,							clrmod + (frand () * 70),		clrmod + 230 + (frand () * 50),
				1.0f,							-1.0f / (0.8f + (frand () * 0.2f)),
				5,								2,
				PT_FLAREGLOW,					PF_SCALED|PF_NOCLOSECULL,
				0,								qFalse,
				PART_STYLE_QUAD,
				0);
		}
	}
}


/*
===============
CG_GibTrail
===============
*/
void CG_GibTrail (vec3_t start, vec3_t end, int flags)
{
	vec3_t		move, vec;
	float		len, dec;
	int			i, partFlags;

	// Bubbles
	if (!(rand () % 10))
		CG_BubbleEffect (start);

	// Blotches and more drips
	Vec3Copy (start, move);
	Vec3Subtract (end, start, vec);
	len = VectorNormalizeFastf (vec);

	dec = 3.25f;
	Vec3Scale (vec, dec, vec);

	//
	// EF_GIB
	//
	if (flags == EF_GIB) {
		for ( ; len > 0 ; Vec3Add (move, vec, move)) {
			len -= dec;
			// Floating blotches
			if (!(rand () & 31)) {
				partFlags = PF_SCALED|PF_ALPHACOLOR;
				if (rand () & 7)
					partFlags |= PF_NODECAL;

				CG_SpawnParticle (
					move[0] + crand (),				move[1] + crand (),				move[2] + crand (),
					0,								0,								0,
					crand (),						crand (),						crand () - 40,
					0,								0,								0,
					115 + (frand () * 5),			125 + (frand () * 10),			120 + (frand () * 10),
					0,								0,								0,
					1.0f,							-0.5f / (0.4f + (frand () * 0.3f)),
					7.5f + (crand () * 2),			9 + (crand () * 2),
					pRandBloodTrail (),				partFlags,
					pBloodThink,					qTrue,
					PART_STYLE_QUAD,
					frand () * 360);
			}

			// Drip
			for (i=0 ; i<((cg.goreScale+0.1f)*10)*0.2f ; i++) {
				if (!(rand () & 15)) {
					partFlags = PF_ALPHACOLOR|PF_GRAVITY;
					if (rand () & 1)
						partFlags |= PF_NODECAL;

					CG_SpawnParticle (
						move[0] + crand (),				move[1] + crand (),				move[2] + crand () - (frand () * 4),
						0,								0,								0,
						crand () * 50,					crand () * 50,					crand () * 50 + 20,
						crand () * 10,					crand () * 10,					-50,
						230 + (frand () * 5),			245 + (frand () * 10),			245 + (frand () * 10),
						0,								0,								0,
						1.0f,							-0.5f / (0.6f + (frand ()* 0.3f)),
						1.25f + (frand () * 0.2f),		1.35f + (frand () * 0.2f),
						pRandBloodDrip (),				partFlags,
						pBloodDripThink,				qTrue,
						PART_STYLE_DIRECTION,
						PMAXBLDDRIPLEN*0.5f + (frand () * PMAXBLDDRIPLEN));
				}
			}
		}

		return;
	}

	//
	// EF_GREENGIB
	//
	if (flags == EF_GREENGIB) {
		for ( ; len > 0 ; Vec3Add (move, vec, move)) {
			len -= dec;
			// Floating blotches
			if (!(rand () & 31)) {
				partFlags = PF_SCALED|PF_ALPHACOLOR|PF_GREENBLOOD;
				if (rand () & 7)
					partFlags |= PF_NODECAL;

				CG_SpawnParticle (
					move[0] + crand (),				move[1] + crand (),				move[2] + crand (),
					0,								0,								0,
					crand (),						crand (),						crand () - 40,
					0,								0,								0,
					20,								50 + (frand () * 90),			20,
					0,								0 + (frand () * 90),			0,
					0.8f + (frand () * 0.2f),		-1.0f / (0.5f + (frand () * 0.3f)),
					4 + (crand () * 2),				6 + (crand () * 2),
					pRandGrnBloodTrail (),			partFlags,
					pBloodThink,					qTrue,
					PART_STYLE_QUAD,
					frand () * 360);
			}

			// Drip
			for (i=0 ; i<((cg.goreScale+0.1f)*10)*0.2f ; i++) {
				if (!(rand () & 15)) {
					partFlags = PF_ALPHACOLOR|PF_GRAVITY|PF_GREENBLOOD;
					if (rand () & 1)
						partFlags |= PF_NODECAL;

					CG_SpawnParticle (
						move[0] + crand (),				move[1] + crand (),				move[2] + crand () - (frand () * 4),
						0,								0,								0,
						crand () * 50,					crand () * 50,					crand () * 50 + 20,
						crand () * 10,					crand () * 10,					-50,
						30,								70 + (frand () * 90),			30,
						0,								0,								0,
						1.0f,							-0.5f / (0.6f + (frand () * 0.3f)),
						1.25f + (frand () * 0.2f),		1.35f + (frand () * 0.2f),
						pRandGrnBloodDrip (),			partFlags,
						pBloodDripThink,				qTrue,
						PART_STYLE_DIRECTION,
						PMAXBLDDRIPLEN*0.5f + (frand () * PMAXBLDDRIPLEN));
				}
			}
		}

		return;
	}
}

// FIXME: implement 0.3.2 particle thunks
static void pGrenadeSmokeThink(struct cgParticle_s *p, vec3_t org, vec3_t angle, vec4_t color, float *size, float *orient, float *time)
{
	pSmokeThink(p, org, angle, color, size, orient, time);
	pLight70Think(p, org, angle, color, size, orient, time);
}

/*
===============
CG_GrenadeTrail
===============
*/
void CG_GrenadeTrail (vec3_t start, vec3_t end)
{
	vec3_t		move, vec;
	float		len, dec;
	float		rnum, rnum2;

	// Smoke
	Vec3Copy (start, move);
	Vec3Subtract (end, start, vec);
	len = VectorNormalizeFastf (vec);

	// Didn't move far enough to deserve a trail
	if (len <= 0.001f)
		return;

	// Bubbles
	CG_BubbleEffect (start);

	// Smoke trail
	dec = 60;
	Vec3Scale (vec, dec, vec);

	for (; len>0 ; Vec3Add (move, vec, move)) {
		len -= dec;

		rnum = 60 + (frand () * 50);
		rnum2 = 70 + (frand () * 50);
		CG_SpawnParticle (
			move[0] + (crand () * 2),		move[1] + (crand () * 2),		move[2] + (crand () * 2),
			0,								0,								0,
			crand () * 3,					crand () * 3,					crand () * 3,
			0,								0,								5,
			rnum,							rnum,							rnum,
			rnum2,							rnum2,							rnum2,
			0.8f + (crand () * 0.1f),		-3.5f / (1.0f + (cg.smokeLingerScale*10.0f) + (crand () * 0.15f)),
			6 + (crand () * 3),				20 + (crand () * 5),
			pRandSmoke (),					PF_NOCLOSECULL,
			pGrenadeSmokeThink,				qTrue,
			PART_STYLE_QUAD,
			frand () * 360);
	}
}


/*
===============
CG_Heatbeam
===============
*/
void CG_Heatbeam (vec3_t start, vec3_t forward)
{
	vec3_t		move, vec, right, up, dir, end;
	int			len, rnum, rnum2;
	float		c, s, ltime, rstep;
	float		i, step, start_pt, rot, variance;

	step = 32;

	Vec3MA (start, 4096, forward, end);

	Vec3Copy (start, move);
	Vec3Subtract (end, start, vec);
	len = (int)VectorNormalizeFastf (vec);

	Vec3Copy (cg.refDef.rightVec, right);
	Vec3Copy (cg.refDef.viewAxis[2], up);
	Vec3MA (move, -0.5f, right, move);
	Vec3MA (move, -0.5f, up, move);

	ltime = (float) cg.realTime / 1000.0f;
	start_pt = (float)fmod (ltime * 96.0f, step);
	Vec3MA (move, start_pt, vec, move);

	Vec3Scale (vec, step, vec);

	rstep = M_PI / 10.0f;
	for (i=start_pt ; i<len ; i+=step) {
		if (i == 0)
			i = 0.01f;
		if (i > step * 5)
			break;

		variance = 0.5;
		for (rot=0 ; rot<(M_PI * 2.0f) ; rot += rstep) {
			c = (float)cos (rot) * variance;
			s = (float)sin (rot) * variance;
			
			// trim it so it looks like it's starting at the origin
			if (i < 10) {
				Vec3Scale (right, c*(i/10.0f), dir);
				Vec3MA (dir, s*(i/10.0f), up, dir);
			}
			else {
				Vec3Scale (right, c, dir);
				Vec3MA (dir, s, up, dir);
			}

			rnum = (rand () % 5);
			rnum2 = (rand () % 5);
			CG_SpawnParticle (
				move[0] + dir[0]*3,				move[1] + dir[1]*3,				move[2] + dir[2]*3,
				0,								0,								0,
				0,								0,								0,
				0,								0,								0,
				palRed (223 - rnum),			palGreen (223 - rnum),			palBlue (223 - rnum),
				palRed (223 - rnum2),			palGreen (223 - rnum2),			palBlue (223 - rnum2),
				0.5,							-1000.0,
				1.0f,							1.0f,
				PT_GENERIC_GLOW,				PF_SCALED|PF_NOCLOSECULL,
				0,								qFalse,
				PART_STYLE_QUAD,
				0);
		}

		Vec3Add (move, vec, move);
	}
}


/*
===============
CG_IonripperTrail
===============
*/
void CG_IonripperTrail (vec3_t start, vec3_t end)
{
	vec3_t	move, vec;
	vec3_t	tmpstart, tmpend;
	float	dec, len;
	int		left = 0, rnum, rnum2;
	float	tnum;

	// gloom optional spiral
	Vec3Copy (start, tmpstart);
	Vec3Copy (end, tmpend);
	if (cg.currGameMod == GAME_MOD_GLOOM) {
		float		c, s;
		vec3_t		dir, right, up;
		int			i;

		// I AM LAME ASS -- HEAR MY ROAR!
		tmpstart[2] += 12;
		tmpend[2] += 12;
		// I AM LAME ASS -- HEAR MY ROAR!

		if (glm_blobtype->intVal) {
			Vec3Copy (tmpstart, move);
			Vec3Subtract (tmpend, tmpstart, vec);
			len = VectorNormalizeFastf (vec);

			MakeNormalVectorsf (vec, right, up);

			for (i=0 ; i<len ; i++) {
				if (i&1)
					continue;

				c = (float)cos (i);
				s = (float)sin (i);

				Vec3Scale (right, c, dir);
				Vec3MA (dir, s, up, dir);

				tnum = (rand () % 2) ? 110 + (frand () * 125) : 0;
				CG_SpawnParticle (
					move[0] + (dir[0] * 1.15f),		move[1] + (dir[1] * 1.15f),		move[2] + (dir[2] * 1.15f),
					0,								0,								0,
					dir[0] * 40,					dir[1] * 40,					dir[2] * 40,
					0,								0,								0,
					tnum,							tnum + 60 + (frand () * 130),	tnum + (frand () * 30),
					tnum,							tnum + 60 + (frand () * 130),	tnum + (frand () * 30),
					0.9f,							-1.0f / (0.3f + (frand () * 0.2f)),
					3.5f,							1.8f,
					PT_IONTAIL,						PF_SCALED|PF_NOCLOSECULL,
					0,								qFalse,
					PART_STYLE_QUAD,
					0);

				Vec3Add (move, vec, move);
			}
		}
	}

	// Bubbles
	CG_BubbleEffect (tmpstart);

	// Trail
	Vec3Copy (tmpstart, move);
	Vec3Subtract (tmpend, tmpstart, vec);
	len = VectorNormalizeFastf (vec);

	dec = 5;
	Vec3Scale (vec, dec, vec);

	for (; len>0 ; Vec3Add (move, vec, move)) {
		len -= dec;

		rnum = rand () % 5;
		rnum2 = rand () % 5;
		CG_SpawnParticle (
			move[0],							move[1],							move[2],
			0,									0,									0,
			left ? 15.0f : -15.0f,				0,									0,
			0,									0,									0,
			palRed (0xe4 + rnum),				palGreen (0xe4 + rnum),				palBlue (0xe4 + rnum),
			palRed (0xe4 + rnum2),				palGreen (0xe4 + rnum2),			palBlue (0xe4 + rnum2),
			0.85f,								-1.0f / (0.33f + (frand () * 0.2f)),
			8,									3,
			PT_IONTIP,							PF_SCALED|PF_NOCLOSECULL,
			0,									qFalse,
			PART_STYLE_QUAD,
			0);

		left = !left;
	}
}


/*
===============
CG_QuadTrail
===============
*/
void CG_QuadTrail (vec3_t start, vec3_t end)
{
	vec3_t		move, vec;
	float		dec, len, rnum;

	Vec3Copy (start, move);
	Vec3Subtract (end, start, vec);
	len = VectorNormalizeFastf (vec);

	dec = 4;
	Vec3Scale (vec, dec, vec);

	for (; len>0 ; Vec3Add (move, vec, move)) {
		len -= dec;

		rnum = (rand () % 2) ? (frand () * 200) : 0;
		CG_SpawnParticle (
			move[0] + (crand () * 6),		move[1] + (crand () * 6),		move[2] + (crand () * 6),
			0,								0,								0,
			crand () * 8,					crand () * 8,					crand () * 8,
			0,								0,								0,
			rnum,							rnum + (frand () * 55),			rnum + 200 + (frand () * 50),
			rnum,							rnum + (frand () * 55),			rnum + 200 + (frand () * 50),
			1.0f,							-1.0f / (0.8f + (frand () * 0.2f)),
			5,								2,
			PT_FLAREGLOW,					PF_SCALED|PF_NOCLOSECULL,
			0,								qFalse,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_RailTrail
===============
*/
void CG_RailTrail (vec3_t start, vec3_t end)
{
	float		dec, len, dist;
	trace_t		tr;
	vec3_t		vec, move;
	vec3_t		angle;
	vec3_t		length;

	// Bubbles
	if (cg.currGameMod != GAME_MOD_LOX && cg.currGameMod != GAME_MOD_GIEX) // FIXME: yay hack
		CG_BubbleTrail (start, end);

	// Beam
	Vec3Subtract (end, start, length);
	CG_SpawnParticle (
		start[0],						start[1],						start[2],
		length[0],						length[1],						length[2],
		0,								0,								0,
		0,								0,								0,
		cg_railCoreRed->floatVal * 255,	cg_railCoreGreen->floatVal * 255,	cg_railCoreBlue->floatVal * 255,
		cg_railCoreRed->floatVal * 255,	cg_railCoreGreen->floatVal * 255,	cg_railCoreBlue->floatVal * 255,
		1.0f,							-0.7f,
		1.2f,							1.4f,
		PT_RAIL_CORE,					0,
		0,								qFalse,
		PART_STYLE_BEAM,
		0);

	// Spots up the center
	Vec3Copy (start, move);
	Vec3Subtract (end, start, vec);
	len = VectorNormalizeFastf (vec);

	dec = 20;
	Vec3Scale (vec, dec, vec);
	Vec3Copy (start, move);

	for (; len>0 ; Vec3Add (move, vec, move)) {
		len -= dec;

		CG_SpawnParticle (
			move[0],						move[1],						move[2],
			0,								0,								0,
			0,								0,								0,
			0,								0,								0,
			cg_railCoreRed->floatVal*255,	cg_railCoreGreen->floatVal*255,	cg_railCoreBlue->floatVal*255,
			cg_railCoreRed->floatVal*255,	cg_railCoreGreen->floatVal*255,	cg_railCoreBlue->floatVal*255,
			0.33f,							-1.0f,
			1.2f,							1.4f,
			PT_GENERIC_GLOW,				PF_NOCLOSECULL,
			NULL,							qFalse,
			PART_STYLE_QUAD,
			frand () * 360);
	}

	// Spirals
	if (cg_railSpiral->intVal) {
		Vec3Copy (start, move);
		Vec3Subtract (end, start, vec);
		len = VectorNormalizeFastf (vec);

		dist = Vec3Dist (start, end);
		dist++;

		dec = 4;
		Vec3Scale (vec, dec, vec);
		Vec3Copy (start, move);

		for (; len>0 ; Vec3Add (move, vec, move)) {
			CG_SpawnParticle (
				move[0],						move[1],						move[2],
				0,								0,								0,
				0,								0,								0,
				crand () * 2,					crand () * 2,					crand () * 2,
				cg_railSpiralRed->floatVal*255,	cg_railSpiralGreen->floatVal*255,	cg_railSpiralBlue->floatVal*255,
				cg_railSpiralRed->floatVal*255,	cg_railSpiralGreen->floatVal*255,	cg_railSpiralBlue->floatVal*255,
				0.75f + (crand () * 0.1f),		-(0.5f + ((len/dist)*0.4f)),
				5 + crand (),					15 + (crand () * 3),
				PT_RAIL_SPIRAL,					PF_NOCLOSECULL,
				NULL,							qFalse,
				PART_STYLE_QUAD,
				frand () * 360);

			len -= dec;
		}
	}

	Vec3Copy (start, move);
	Vec3Subtract (end, start, vec);
	VectorNormalizeFastf (vec);

	Vec3Scale (vec, 2, vec);
	Vec3Add (move, vec, move);

	// Make a decal
	tr = cgi.CM_Trace (move, end, 1, 1);
	if (tr.fraction < 1) {
		CG_SparkEffect (tr.endPos, tr.plane.normal, 10, 10, 20, 2, 3);

		// wave
		VecToAngleRolled (tr.plane.normal, frand () * 360, angle);

		CG_SpawnParticle (
			tr.endPos[0] + tr.plane.normal[0],	tr.endPos[1] + tr.plane.normal[0],	tr.endPos[2] + tr.plane.normal[0],
			angle[0],							angle[1],							angle[2],
			0,									0,									0,
			0,									0,									0,
			cg_railCoreRed->floatVal*255,			cg_railCoreGreen->floatVal*255,		cg_railCoreBlue->floatVal*255,
			cg_railCoreRed->floatVal*255,			cg_railCoreGreen->floatVal*255,		cg_railCoreBlue->floatVal*255,
			0.7f + (crand () * 0.1f),			-1.0f / (0.3f + (frand () * 0.1f)),
			5 + (crand () * 2),					30 + (crand () * 5),
			PT_RAIL_WAVE,						PF_SCALED,
			NULL,								qFalse,
			PART_STYLE_ANGLED,
			0);

		// fast-fading mark
		CG_SpawnDecal (
			tr.endPos[0],						tr.endPos[1],						tr.endPos[2],
			tr.plane.normal[0],					tr.plane.normal[1],					tr.plane.normal[2],
			cg_railCoreRed->floatVal*255,		cg_railCoreGreen->floatVal*255,		cg_railCoreBlue->floatVal*255,
			cg_railCoreRed->floatVal*255,		cg_railCoreGreen->floatVal*255,		cg_railCoreBlue->floatVal*255,
			1.0f,								0,
			10 + crand (),
			DT_RAIL_WHITE,						DF_FIXED_LIFE,
			0,									qFalse,
			0.25f + (frand () * 0.25f),			frand () * 360.0f);

		// burn mark
		CG_SpawnDecal (
			tr.endPos[0],						tr.endPos[1],							tr.endPos[2],
			tr.plane.normal[0],					tr.plane.normal[1],						tr.plane.normal[2],
			(cg_railCoreRed->floatVal*128)+128,	(cg_railCoreGreen->floatVal*128)+128,	(cg_railCoreBlue->floatVal*128)+128,
			0,									0,										0,
			0.9f + (crand () * 0.1f),			0.8f,
			10 + crand (),
			DT_RAIL_BURNMARK,					DF_ALPHACOLOR,
			0,									qFalse,
			0,									frand () * 360.0f);

		// "flashing" glow marks
		CG_SpawnDecal (
			tr.endPos[0],					tr.endPos[1],					tr.endPos[2],
			tr.plane.normal[0],				tr.plane.normal[1],				tr.plane.normal[2],
			cg_railCoreRed->floatVal*255,	cg_railCoreGreen->floatVal*255,	cg_railCoreBlue->floatVal*255,
			cg_railCoreRed->floatVal*255,	cg_railCoreGreen->floatVal*255,	cg_railCoreBlue->floatVal*255,
			1.0f,							0,
			30,
			DT_RAIL_GLOWMARK,				DF_FIXED_LIFE,
			0,								qFalse,
			0.25f + (frand () * 0.25f),		frand () * 360.0f);

		if (!cg_railSpiral->intVal) {
			CG_SpawnDecal (
				tr.endPos[0],					tr.endPos[1],						tr.endPos[2],
				tr.plane.normal[0],				tr.plane.normal[1],					tr.plane.normal[2],
				cg_railSpiralRed->floatVal*255,	cg_railSpiralGreen->floatVal*255,	cg_railSpiralBlue->floatVal*255,
				cg_railSpiralRed->floatVal*255,	cg_railSpiralGreen->floatVal*255,	cg_railSpiralBlue->floatVal*255,
				1.0f,							0,
				12,
				DT_RAIL_GLOWMARK,				DF_FIXED_LIFE,
				0,								qFalse,
				0.25f + (frand () * 0.25f),		frand () * 360.0f);
		}
	}
}


/*
===============
CG_RocketTrail
===============
*/
void CG_RocketTrail (vec3_t start, vec3_t end)
{
	int			fire;
	vec3_t		move, vec;
	float		len, dec;
	float		rnum, rnum2;
	qBool		inWater;

	// Check if in water
	if (cgi.CM_PointContents (start, 0) & MASK_WATER)
		inWater = qTrue;
	else
		inWater = qFalse;

	// Distance and vectors for trails
	Vec3Copy (start, move);
	Vec3Subtract (start, end, vec);
	len = VectorNormalizeFastf (vec);

	dec = 2.5f + (inWater ? 1 : 0);
	Vec3Scale (vec, dec, vec);

	// Didn't move far enough to deserve a trail
	if (len <= 0.001f)
		return;

	// Bubbles
	if (inWater)
		CG_BubbleTrail (start, end);

	// Flare glow
	if (inWater) {
		CG_SpawnParticle (
			start[0],						start[1],						start[2],
			0,								0,								0,
			0,								0,								0,
			0,								0,								0,
			35.0f,							30.0f,							230.0f,
			35.0f,							30.0f,							230.0f,
			0.5f + (crand () * 0.2f),		PART_INSTANT,
			12 + crand (),					15,
			PT_FLAREGLOW,					0,
			0,								qFalse,
			PART_STYLE_QUAD,
			0);
	}
	else {
		CG_SpawnParticle (
			start[0],						start[1],						start[2],
			0,								0,								0,
			0,								0,								0,
			0,								0,								0,
			255.0f,							170.0f,							100.0f,
			255.0f,							170.0f,							100.0f,
			0.5f + (crand () * 0.2f),		PART_INSTANT,
			12 + crand (),					15,
			PT_FLAREGLOW,					0,
			0,								qFalse,
			PART_STYLE_QUAD,
			0);
	}

	// White flare core
	CG_SpawnParticle (
		start[0],						start[1],						start[2],
		0,								0,								10,
		0,								0,								0,
		0,								0,								0,
		255.0f,							255.0f,							255.0f,
		255.0f,							255.0f,							255.0f,
		0.5f + (crand () * 0.2f),		PART_INSTANT,
		10 + crand (),					15,
		PT_FLAREGLOW,					0,
		0,								qFalse,
		PART_STYLE_QUAD,
		0);

	// Trail elements
	fire = inWater ? PT_BLUEFIRE : pRandFire ();
	for (; len>0 ; Vec3Add (move, vec, move)) {
		len -= dec;

		if (rand () & 1) {
			// Fire
			CG_SpawnParticle (
				move[0] + crand (),					move[1] + crand (),					move[2] + crand (),
				0,									0,									0,
				crand () * 20,						crand () * 20,						crand () * 20,
				0,									0,									0,
				235 + (frand () * 20),				235 + (frand () * 20),				235 + (frand () * 20),
				235 + (frand () * 20),				235 + (frand () * 20),				235 + (frand () * 20),
				0.9f + (crand () * 0.1f),			-1.0f / (0.2f + (crand () * 0.05f)),
				3 + (frand () * 4),					10 + (frand () * 5),
				fire,								PF_NOCLOSECULL,
				pFireThink,							qTrue,
				PART_STYLE_QUAD,
				frand () * 360);

			// Embers
			if (!inWater && !(rand () % 5))
				CG_SpawnParticle (
					move[0],							move[1],							move[2],
					0,									0,									0,
					crand (),							crand (),							crand (),
					0,									0,									0,
					235 + (frand () * 20),				235 + (frand () * 20),				235 + (frand () * 20),
					235 + (frand () * 20),				235 + (frand () * 20),				235 + (frand () * 20),
					0.8f + (crand () * 0.1f),			-1.0f / (0.15f + (crand () * 0.05f)),
					5 + (crand () * 4.5f),				2 + (frand () * 3),
					pRandEmbers (),						PF_NOCLOSECULL,
					pFireThink,							qTrue,
					PART_STYLE_QUAD,
					frand () * 360);
		}
		else {
			// Fire
			CG_SpawnParticle (
				move[0] + crand (),					move[1] + crand (),					move[2] + crand (),
				0,									0,									0,
				0,									0,									0,
				crand (),							crand (),							crand (),
				235 + (frand () * 20),				235 + (frand () * 20),				235 + (frand () * 20),
				235 + (frand () * 20),				235 + (frand () * 20),				235 + (frand () * 20),
				0.9f + (crand () * 0.1f),			-1.0f / (0.25f + (crand () * 0.05f)),
				5 + (frand () * 4),					4 + (frand () * 5),
				fire,								PF_NOCLOSECULL,
				pFireThink,							qTrue,
				PART_STYLE_QUAD,
				frand () * 360);

			// Dots
			if (!inWater && !(rand () % 5))
				CG_SpawnParticle (
					move[0] + crand ()*2,				move[1] + crand ()*2,				move[2] + crand ()*2,
					0,									0,									0,
					0,									0,									0,
					crand () * 128,						crand () * 128,						crand () * 128,
					225 + (frand () * 30),				225 + (frand () * 30),				35 + (frand () * 100),
					225 + (frand () * 30),				225 + (frand () * 30),				35 + (frand () * 100),
					0.9f + (crand () * 0.1f),			-1.0f / (0.25f + (crand () * 0.05f)),
					1 + (crand () * 0.5f),				0.5f + (crand () * 0.25f),
					PT_GENERIC,							PF_NOCLOSECULL,
					pFireThink,							qTrue,
					PART_STYLE_QUAD,
					frand () * 360);
		}
	}

	// Smoke
	Vec3Copy (start, move);
	Vec3Subtract (start, end, vec);
	len = VectorNormalizeFastf (vec);

	dec = 50;
	Vec3Scale (vec, dec, vec);

	for (; len>0 ; Vec3Add (move, vec, move)) {
		len -= dec;

		rnum = 60 + (frand () * 50);
		rnum2 = 70 + (frand () * 50);
		CG_SpawnParticle (
			move[0] + crand (),					move[1] + crand (),					move[2] + crand (),
			0,									0,									0,
			crand () * 2,						crand () * 2,						crand () * 2,
			0,									0,									7 + (frand () * 10),
			rnum,								rnum,								rnum,
			rnum2,								rnum2,								rnum2,
			0.4f + (crand () * 0.1f),			-1.5f / (1.0f + (cg.smokeLingerScale*10.0f) + (crand () * 0.15f)),
			9 + (crand () * 4),					40 + (frand () * 20),
			pRandSmoke (),						0,
			pLight70Think,						qFalse,
			PART_STYLE_QUAD,
			frand () * 360.0f);
	}
}


/*
===============
CG_TagTrail
===============
*/
void CG_TagTrail (vec3_t start, vec3_t end)
{
	vec3_t		move, vec;
	float		len, dec;

	Vec3Copy (start, move);
	Vec3Subtract (end, start, vec);
	len = VectorNormalizeFastf (vec);

	dec = 5;
	Vec3Scale (vec, dec, vec);

	while (len >= 0) {
		len -= dec;

		CG_SpawnParticle (
			move[0] + (crand () * 16),		move[1] + (crand () * 16),		move[2] + (crand () * 16),
			0,								0,								0,
			crand () * 5,					crand () * 5,					crand () * 5,
			0,								0,								0,
			palRed (220),					palGreen (220),					palBlue (220),
			palRed (220),					palGreen (220),					palBlue (220),
			1.0f,							-1.0f / (0.8f + (frand () * 0.2f)),
			1.0f,							1.0f,
			PT_GENERIC,						PF_SCALED,
			0,								qFalse,
			PART_STYLE_QUAD,
			0);

		Vec3Add (move, vec, move);
	}
}


/*
===============
CG_TrackerTrail
===============
*/
void CG_TrackerTrail (vec3_t start, vec3_t end)
{
	vec3_t		move, vec, porg;
	vec3_t		forward, right, up, angle_dir;
	float		len, dec, dist;

	Vec3Copy (start, move);
	Vec3Subtract (end, start, vec);
	len = VectorNormalizeFastf (vec);

	Vec3Copy (vec, forward);
	VecToAngles (forward, angle_dir);
	Angles_Vectors (angle_dir, forward, right, up);

	dec = 3;
	Vec3Scale (vec, dec, vec);

	for (; len>0 ; Vec3Add (move, vec, move)) {
		len -= dec;

		dist = DotProduct (move, forward);
		Vec3MA (move, 8 * (float)cos (dist), up, porg);

		CG_SpawnParticle (
			porg[0],						porg[1],						porg[2],
			0,								0,								0,
			0,								0,								5,
			0,								0,								0,
			0,								0,								0,
			0,								0,								0,
			1.0,							-2.0,
			1.0f,							1.0f,
			PT_GENERIC,						PF_SCALED,
			0,								qFalse,
			PART_STYLE_QUAD,
			0);
	}
}
