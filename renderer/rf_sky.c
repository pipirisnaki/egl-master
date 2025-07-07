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
// rf_sky.c
// Sky clipping and rendering
//

#include "rf_local.h"

typedef struct skyState_s {
	qBool			loaded;

	char			baseName[MAX_QPATH];
	float			rotation;
	vec3_t			axis;

	mesh_t			meshes[6];
	material_t		*materials[6];

	vec2_t			coords[6][4];
	vec3_t			verts[6][4];
} skyState_t;

static skyState_t		r_skyState;

/*
=============================================================================

	SKY

=============================================================================
*/

static const float	r_skyClip[6][3] = {
	{1, 1, 0},		{1, -1, 0},		{0, -1, 1},		{0, 1, 1},		{1, 0, 1},		{-1, 0, 1} 
};
static const int	r_skySTToVec[6][3] = {
	{3, -1, 2},		{-3, 1, 2},		{1, 3, 2},		{-1, -3, 2},	{-2, -1, 3},	{2, -1, -3}
};
static const int	r_skyVecToST[6][3] = {
	{-2, 3, 1},		{2, 3, -1},		{1, 3, 2},		{-1, 3, -2},	{-2, -1, 3},	{-2, 1, -3}
};
static const int	r_skyTexOrder[6] = {0, 2, 1, 3, 4, 5};

/*
=================
R_ClipSkySurface
=================
*/
static void ClipSkyPolygon (int nump, vec3_t vecs, int stage)
{
	const float	*norm;
	float	*v, d, e;
	float	dists[SKY_MAXCLIPVERTS];
	int		sides[SKY_MAXCLIPVERTS];
	int		newc[2], i, j;
	vec3_t	newv[2][SKY_MAXCLIPVERTS];
	qBool	front, back;

	if (nump > SKY_MAXCLIPVERTS-2)
		Com_Error (ERR_DROP, "ClipSkyPolygon: SKY_MAXCLIPVERTS");

	if (stage == 6) {
		// Fully clipped, so draw it
		vec3_t	vt, av;
		float	s, t, dv;
		int		axis;
		float	*vp;

		// Decide which face it maps to
		Vec3Clear (vt);
		for (i=0, vp=vecs ; i<nump ; i++, vp+=3)
			Vec3Add (vp, vt, vt);

		Vec3Set (av, (float)fabs (vt[0]), (float)fabs (vt[1]), (float)fabs (vt[2]));

		if (av[0] > av[1] && av[0] > av[2])
			axis = (vt[0] < 0) ? 1 : 0;
		else if (av[1] > av[2] && av[1] > av[0])
			axis = (vt[1] < 0) ? 3 : 2;
		else
			axis = (vt[2] < 0) ? 5 : 4;

		// Project new texture coords
		for (i=0 ; i<nump ; i++, vecs+=3) {
			j = r_skyVecToST[axis][2];
			dv = (j > 0) ? vecs[j - 1] : -vecs[-j - 1];

			if (dv < 0.001)
				continue;	// Don't divide by zero

			dv = 1.0f / dv;

			j = r_skyVecToST[axis][0];
			s = (j < 0) ? -vecs[-j -1] * dv : vecs[j-1] * dv;

			j = r_skyVecToST[axis][1];
			t = (j < 0) ? -vecs[-j -1] * dv : vecs[j-1] * dv;

			if (s < r_currentList->skyMins[axis][0])
				r_currentList->skyMins[axis][0] = s;
			if (t < r_currentList->skyMins[axis][1])
				r_currentList->skyMins[axis][1] = t;

			if (s > r_currentList->skyMaxs[axis][0])
				r_currentList->skyMaxs[axis][0] = s;
			if (t > r_currentList->skyMaxs[axis][1])
				r_currentList->skyMaxs[axis][1] = t;
		}

		return;
	}

	front = back = qFalse;
	norm = r_skyClip[stage];
	for (i=0, v=vecs ; i<nump ; i++, v+=3) {
		d = DotProduct (v, norm);
		if (d > LARGE_EPSILON) {
			front = qTrue;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -LARGE_EPSILON) {
			back = qTrue;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if (!front || !back) {
		// Not clipped
		ClipSkyPolygon (nump, vecs, stage+1);
		return;
	}

	// Clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	Vec3Copy (vecs, (vecs+(i*3)));
	newc[0] = newc[1] = 0;

	for (i=0, v=vecs ; i<nump ; i++, v+=3) {
		switch (sides[i]) {
		case SIDE_FRONT:
			Vec3Copy (v, newv[0][newc[0]]);
			newc[0]++;
			break;

		case SIDE_BACK:
			Vec3Copy (v, newv[1][newc[1]]);
			newc[1]++;
			break;

		case SIDE_ON:
			Vec3Copy (v, newv[0][newc[0]]);
			newc[0]++;
			Vec3Copy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON
		|| sides[i+1] == SIDE_ON
		|| sides[i+1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j=0 ; j<3 ; j++) {
			e = v[j] + d * (v[j+3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// Continue
	ClipSkyPolygon (newc[0], newv[0][0], stage+1);
	ClipSkyPolygon (newc[1], newv[1][0], stage+1);
}
void R_ClipSkySurface (mBspSurface_t *surf)
{
	vec3_t	*vert;
	vec3_t	verts[4];
	int		*index, i;

	// Don't draw twice
	surf->visFrame = ri.frameCount;

	// Calculate vertex values for sky box
	vert = surf->mesh->vertexArray;
	index = surf->mesh->indexArray;
	for (i=0 ; i<surf->mesh->numIndexes ; i+=3, index+=3) {
		Vec3Subtract (vert[index[0]], ri.def.viewOrigin, verts[0]);
		Vec3Subtract (vert[index[1]], ri.def.viewOrigin, verts[1]);
		Vec3Subtract (vert[index[2]], ri.def.viewOrigin, verts[2]);

		ClipSkyPolygon (3, verts[0], 0);
	}
}


/*
==============
R_AddSkyToList
==============
*/
void R_AddSkyToList (void)
{
	if (!r_skyState.loaded)
		return;

	// FIXME
	R_AddMeshToList (r_skyState.materials[r_skyTexOrder[0]], 0, NULL, NULL, MBT_SKY, r_skyState.verts);
}


/*
==============
R_ClearSky
==============
*/
void R_ClearSky (void)
{
	int		i;

	if (!r_skyState.loaded)
		return;

	for (i=0 ; i<6 ; i++)
		Clear2DBounds (r_currentList->skyMins[i], r_currentList->skyMaxs[i]);
}


/*
==============
R_DrawSky
==============
*/
static void R_StoreSkyVerts (int side, int vertNum, float s, float t)
{
	vec3_t		v, b;
	int			j, k;

	// Coords
	r_skyState.coords[side][vertNum][0] = clamp ((s + 1) * 0.5f, 0, 1);
	r_skyState.coords[side][vertNum][1] = 1.0f - clamp ((t + 1) * 0.5f, 0, 1);

	// Verts
	Vec3Set (b, s * SKY_BOXSIZE, t * SKY_BOXSIZE, SKY_BOXSIZE);

	for (j=0 ; j<3 ; j++) {
		k = r_skySTToVec[side][j];
		if (k < 0)
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
	}

	r_skyState.verts[side][vertNum][0] = v[0] + ri.def.viewOrigin[0];
	r_skyState.verts[side][vertNum][1] = v[1] + ri.def.viewOrigin[1];
	r_skyState.verts[side][vertNum][2] = v[2] + ri.def.viewOrigin[2];
}
void R_DrawSky (meshBuffer_t *mb)
{
	int			i;

	if (r_skyState.rotation) {
		// Check for sky visibility
		for (i=0 ; i<6 ; i++) {
			if (r_currentList->skyMins[i][0] < r_currentList->skyMaxs[i][0]
			&& r_currentList->skyMins[i][1] < r_currentList->skyMaxs[i][1])
				break;
		}
		if (i == 6)
			return;	// Nothing visible

		// Rotation matrix
		qglPushMatrix ();
		qglRotatef (ri.def.time * r_skyState.rotation, r_skyState.axis[0], r_skyState.axis[1], r_skyState.axis[2]);
	}

	for (i=0 ; i<6 ; i++) {
		if (r_skyState.rotation) {
			// Hack, forces full sky to draw when rotating
			r_currentList->skyMins[i][0] = -1;
			r_currentList->skyMins[i][1] = -1;
			r_currentList->skyMaxs[i][0] = 1;
			r_currentList->skyMaxs[i][1] = 1;
		}
		else {
			if (r_currentList->skyMins[i][0] >= r_currentList->skyMaxs[i][0]
			|| r_currentList->skyMins[i][1] >= r_currentList->skyMaxs[i][1])
				continue;
		}

		// Push and render
		R_StoreSkyVerts (i, 0, r_currentList->skyMins[i][0], r_currentList->skyMins[i][1]);
		R_StoreSkyVerts (i, 1, r_currentList->skyMins[i][0], r_currentList->skyMaxs[i][1]);
		R_StoreSkyVerts (i, 2, r_currentList->skyMaxs[i][0], r_currentList->skyMaxs[i][1]);
		R_StoreSkyVerts (i, 3, r_currentList->skyMaxs[i][0], r_currentList->skyMins[i][1]);

		mb->mat = r_skyState.materials[r_skyTexOrder[i]];
		RB_PushMesh (&r_skyState.meshes[i], MF_NONBATCHED|MF_TRIFAN|mb->mat->features);
		RB_RenderMeshBuffer (mb, qFalse);
	}

	if (r_skyState.rotation)
		qglPopMatrix ();
}


/*
=================
R_CheckLoadSky

Returns qTrue if there are ANY sky surfaces in the map, called on map load
=================
*/
static qBool R_CheckLoadSky (mBspNode_t *node)
{
	mBspSurface_t	*surf, **mark;
	mBspLeaf_t		*leaf;
	int				i;

	if (node->c.q2_contents == CONTENTS_SOLID)
		return qFalse;		// Solid

	// Recurse down the children
	if (node->c.q2_contents == -1)
		return R_CheckLoadSky (node->children[0]) || R_CheckLoadSky (node->children[1]);

	// If this is a leaf node, draw it
	leaf = (mBspLeaf_t *)node;
	if (!leaf->q2_numMarkSurfaces)
		return qFalse;

	// Search
	for (i=0, mark=leaf->q2_firstMarkSurface ; i<leaf->q2_numMarkSurfaces ; i++, mark++) {
		surf = *mark;
		if (surf->q2_texInfo->flags & SURF_TEXINFO_SKY)
			return qTrue;
	}

	return qFalse;
}


/*
============
R_SetSky
============
*/
void R_SetSky (char *name, float rotate, vec3_t axis)
{
	char	pathName[MAX_QPATH];
	int		i;

	if (ri.scn.worldModel->type == MODEL_Q3BSP) {
		r_skyState.loaded = qTrue;
	}
	else {
		r_skyState.loaded = R_CheckLoadSky (ri.scn.worldModel->bspModel.nodes);
		if (!r_skyState.loaded)
			return;
	}

	Q_strncpyz (r_skyState.baseName, name, sizeof (r_skyState.baseName));
	r_skyState.rotation = rotate;
	Vec3Copy (axis, r_skyState.axis);

	for (i=0 ; i<6 ; i++) {
		Q_snprintfz (pathName, sizeof (pathName), "env/%s%s.tga", r_skyState.baseName, r_skyNameSuffix[i]);
		r_skyState.materials[i] = R_RegisterSky (pathName);

		if (!r_skyState.materials[i])
			r_skyState.materials[i] = r_noMaterialSky;
	}
}

/*
=============================================================================

	CONSOLE COMMANDS

=============================================================================
*/

/*
=================
R_SetSky_f

Set a specific sky and rotation speed
=================
*/
static void R_SetSky_f (void)
{
	float	rotate;
	vec3_t	axis;

	if (!r_skyState.loaded) {
		Com_Printf (0, "No sky surfaces!\n");
		return;
	}

	if (Cmd_Argc () < 2) {
		Com_Printf (0, "Usage: sky <basename> <rotate> [axis x y z]\n");
		Com_Printf (0, "Currently: sky <%s> <%.1f> [%.1f %.1f %.1f]\n", r_skyState.baseName, r_skyState.rotation, r_skyState.axis[0], r_skyState.axis[1], r_skyState.axis[2]);
		return;
	}

	if (Cmd_Argc () > 2)
		rotate = (float)atof (Cmd_Argv (2));
	else
		rotate = 0;

	if (Cmd_Argc () == 6)
		Vec3Set (axis, (float)atof (Cmd_Argv (3)), (float)atof (Cmd_Argv (4)), (float)atof (Cmd_Argv (5)));
	else
		Vec3Set (axis, 0, 0, 1);

	R_SetSky (Cmd_Argv (1), rotate, axis);
}

/*
=============================================================================

	INIT / SHUTDOWN

=============================================================================
*/

static void	*cmd_sky;

/*
==================
R_SkyInit
==================
*/
void R_SkyInit (void)
{
	int		i;

	// Commands
	cmd_sky = Cmd_AddCommand ("sky",		R_SetSky_f,		"Changes the sky env basename");

	// Init sky meshes
	for (i=0 ; i<6 ; i++) {
		r_skyState.meshes[i].numIndexes = 0;
		r_skyState.meshes[i].numVerts = 4;

		r_skyState.meshes[i].colorArray = NULL;
		r_skyState.meshes[i].coordArray = r_skyState.coords[i];
		r_skyState.meshes[i].indexArray = NULL;
		r_skyState.meshes[i].lmCoordArray = NULL;
		r_skyState.meshes[i].normalsArray = NULL;
		r_skyState.meshes[i].sVectorsArray = NULL;
		r_skyState.meshes[i].tVectorsArray = NULL;
		r_skyState.meshes[i].trNeighborsArray = NULL;
		r_skyState.meshes[i].trNormalsArray = NULL;
		r_skyState.meshes[i].vertexArray = r_skyState.verts[i];
	}
}


/*
==================
R_SkyShutdown
==================
*/
void R_SkyShutdown (void)
{
	// Remove commands
	Cmd_RemoveCommand ("sky", cmd_sky);
}
