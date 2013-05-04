// Copyright (C) 1999-2000 Id Software, Inc.
//
// g_misc.c

#include "g_local.h"


/*QUAKED func_group (0 0 0) ?
Used to group brushes together just for editor convenience.  They are turned into normal brushes by the utilities.
*/


/*QUAKED info_camp (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for calculations in the utilities (spotlights, etc), but removed during gameplay.
*/
void SP_info_camp( gentity_t *self ) {
	G_SetOrigin( self, self->s.origin );
}

/*QUAKED info_waypoint (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a waypoint target for bot patrolling
*/
void SP_info_waypoint( gentity_t *self ) {
}

/*QUAKED info_backpack (0 0.5 0) ?
Used as a teleportation target for backpacks that fall into a nodrop brush
*/
void SP_info_backpack( gentity_t *self ) {
}

/*QUAKED info_null (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for calculations in the utilities (spotlights, etc), but removed during gameplay.
*/
void SP_info_null( gentity_t *self ) {
	G_FreeEntity( self );
}


/*QUAKED info_notnull (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for in-game calculation, like jumppad targets.
target_position does the same thing
*/
void SP_info_notnull( gentity_t *self ){
	G_SetOrigin( self, self->s.origin );
}


/*QUAKED light (0 1 0) (-8 -8 -8) (8 8 8) linear
Non-displayed light.
"light" overrides the default 300 intensity.
Linear checbox gives linear falloff instead of inverse square
Lights pointed at a target will be spotlights.
"radius" overrides the default 64 unit radius of a spotlight at the target point.
*/
void SP_light( gentity_t *self ) {
	G_FreeEntity( self );
}

/*QUAKED info_camera (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional and viewangles target for in-game cutscenes.
*/
void Use_Camera (gentity_t *self, gentity_t *other, gentity_t *activator) {
	char variableInfo[MAX_INFO_STRING];

	//make camera movement info available for client 
	variableInfo[0] = '\0';
	Info_SetValueForKey(variableInfo, "w", va("%f", self->wait));
	Info_SetValueForKey(variableInfo, "t", va("%i", level.time));

	//add origin, viewangles and fov of source camera
	Info_SetValueForKey(variableInfo, "o10", va("%f", self->s.origin[0]));
	Info_SetValueForKey(variableInfo, "o11", va("%f", self->s.origin[1]));
	Info_SetValueForKey(variableInfo, "o12", va("%f", self->s.origin[2]));
	Info_SetValueForKey(variableInfo, "a10", va("%f", self->s.angles[0]));
	Info_SetValueForKey(variableInfo, "a11", va("%f", self->s.angles[1]));
	Info_SetValueForKey(variableInfo, "a12", va("%f", self->s.angles[2]));
	Info_SetValueForKey(variableInfo, "f1", va("%i", self->count));
	
	if ( self->nextTrain && (self->spawnflags & 1) ) {
		//add origin and viewangles of target camera
		Info_SetValueForKey(variableInfo, "m", va("%i", self->spawnflags));
		Info_SetValueForKey(variableInfo, "o20", va("%f", self->nextTrain->s.origin[0]));
		Info_SetValueForKey(variableInfo, "o21", va("%f", self->nextTrain->s.origin[1]));
		Info_SetValueForKey(variableInfo, "o22", va("%f", self->nextTrain->s.origin[2]));
		Info_SetValueForKey(variableInfo, "a20", va("%f", self->nextTrain->s.angles[0]));
		Info_SetValueForKey(variableInfo, "a21", va("%f", self->nextTrain->s.angles[1]));
		Info_SetValueForKey(variableInfo, "a22", va("%f", self->nextTrain->s.angles[2]));
		Info_SetValueForKey(variableInfo, "f2", va("%i", self->nextTrain->count));
	} else {
		Info_SetValueForKey(variableInfo, "m", "0");	//0 means no camera motion
	}
	//G_Printf("%s\n", variableInfo);
	trap_SetConfigstring( CS_CUTSCENE, variableInfo );

	activator->client->ps.pm_type = PM_CUTSCENE;
	self->activator = activator;
	self->nextthink = level.time + (self->wait * 1000);
}

void Think_Camera (gentity_t *self) {
	int i;

	if ( self->nextTrain ) {
		//jump to next camera
		self->nextTrain->use( self->nextTrain, self->activator, self->activator );
	} else {
		//cutscene should end so give player normal control
		self->activator->client->ps.pm_type = PM_NORMAL;

		//move player back to original location
		VectorCopy(self->activator->orgOrigin, self->activator->client->ps.origin);
	
		//give movement control back to bots
		if ( self->parent->spawnflags & 1 ) {
			for ( i = 0 ; i < level.maxclients ; i++ ) {
				if ( level.clients[i].pers.connected != CON_DISCONNECTED && level.clients[i].ps.pm_type != PM_DEAD )
					level.clients[i].ps.pm_type = PM_NORMAL;
			}
		}

		//link the player back into the world
		trap_LinkEntity( self->activator );

		//enable synchronousClients if it was enabled before the cutscene kicked in. The skill field is abused to
		//temporarily store the pre-cutscene value of g_synchronousclients
		if (g_allowSyncCutscene.integer == 0)
			trap_Cvar_Set("g_synchronousClients", va("%1.0f", self->activator->skill ));
	}
}

void SP_info_camera( gentity_t *self ) {	
	G_SpawnFloat( "wait", "1", &self->wait );
	G_SpawnInt( "fov", "90", &self->count );	//abusing self->count here to store the FOV for the camera

	self->use = Use_Camera;
	self->think = Think_Camera;
	G_SetOrigin( self, self->s.origin );
}



/*
=================================================================================

TELEPORTERS

=================================================================================
*/

/*=========
Sets player's location without spitting out the player
===========*/
void TeleportPlayerNoKnockback( gentity_t *player, vec3_t origin, vec3_t angles ) {
	gentity_t	*tent;

	// use temp events at source and destination to prevent the effect
	// from getting dropped by a second player event
	if ( player->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		tent = G_TempEntity( player->client->ps.origin, EV_PLAYER_TELEPORT_OUT );
		tent->s.clientNum = player->s.clientNum;

		tent = G_TempEntity( origin, EV_PLAYER_TELEPORT_IN );
		tent->s.clientNum = player->s.clientNum;
	}

	// unlink to make sure it can't possibly interfere with G_KillBox
	trap_UnlinkEntity (player);

	VectorCopy ( origin, player->client->ps.origin );
	player->client->ps.origin[2] += 1;

	// spit the player out
	//AngleVectors( angles, player->client->ps.velocity, NULL, NULL );
	//VectorScale( player->client->ps.velocity, 400, player->client->ps.velocity );
	//player->client->ps.pm_time = 160;		// hold time
	//player->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;

	// toggle the teleport bit so the client knows to not lerp
	player->client->ps.eFlags ^= EF_TELEPORT_BIT;

	// set angles
	SetClientViewAngle( player, angles );

	player->s.angles[0] = atof(va("%.4f", player->s.angles[0]));
	player->s.angles[1] = atof(va("%.4f", player->s.angles[1]));
	player->s.angles[2] = atof(va("%.4f", player->s.angles[2]));

	// kill anything at the destination
	if ( player->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		G_KillBox (player);
	}

	// save results of pmove
	BG_PlayerStateToEntityState( &player->client->ps, &player->s, qtrue );

	// use the precise origin for linking
	VectorCopy( player->client->ps.origin, player->r.currentOrigin );

	if ( player->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		trap_LinkEntity (player);
	}
}

void TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles ) {
	gentity_t	*tent;

	// use temp events at source and destination to prevent the effect
	// from getting dropped by a second player event
	if ( player->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		tent = G_TempEntity( player->client->ps.origin, EV_PLAYER_TELEPORT_OUT );
		tent->s.clientNum = player->s.clientNum;

		tent = G_TempEntity( origin, EV_PLAYER_TELEPORT_IN );
		tent->s.clientNum = player->s.clientNum;
	}

	// unlink to make sure it can't possibly interfere with G_KillBox
	trap_UnlinkEntity (player);

	VectorCopy ( origin, player->client->ps.origin );
	player->client->ps.origin[2] += 1;

	// spit the player out
	AngleVectors( angles, player->client->ps.velocity, NULL, NULL );
	VectorScale( player->client->ps.velocity, 400, player->client->ps.velocity );
	player->client->ps.pm_time = 160;		// hold time
	player->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;

	// toggle the teleport bit so the client knows to not lerp
	player->client->ps.eFlags ^= EF_TELEPORT_BIT;

	// set angles
	SetClientViewAngle( player, angles );

	// kill anything at the destination
	if ( player->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		G_KillBox (player);
	}

	// save results of pmove
	BG_PlayerStateToEntityState( &player->client->ps, &player->s, qtrue );

	// use the precise origin for linking
	VectorCopy( player->client->ps.origin, player->r.currentOrigin );

	if ( player->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		trap_LinkEntity (player);
	}
}


/*QUAKED misc_teleporter_dest (1 0 0) (-32 -32 -24) (32 32 -16)
Point teleporters at these.
Now that we don't have teleport destination pads, this is just
an info_notnull
*/
void SP_misc_teleporter_dest( gentity_t *ent ) {
}


//===========================================================

/*QUAKED misc_model (1 0 0) (-16 -16 -16) (16 16 16)
"model"		arbitrary .md3 file to display
*/
void SP_misc_model( gentity_t *ent ) {

#if 0
	ent->s.modelindex = G_ModelIndex( ent->model );
	VectorSet (ent->mins, -16, -16, -16);
	VectorSet (ent->maxs, 16, 16, 16);
	trap_LinkEntity (ent);

	G_SetOrigin( ent, ent->s.origin );
	VectorCopy( ent->s.angles, ent->s.apos.trBase );
#else
	G_FreeEntity( ent );
#endif
}

//===========================================================

void locateCamera( gentity_t *ent ) {
	vec3_t		dir;
	gentity_t	*target;
	gentity_t	*owner;

	owner = G_PickTarget( ent->target );
	if ( !owner ) {
		G_Printf( "Couldn't find target for misc_partal_surface\n" );
		G_FreeEntity( ent );
		return;
	}
	ent->r.ownerNum = owner->s.number;

	// frame holds the rotate speed
	if ( owner->spawnflags & 1 ) {
		ent->s.frame = 25;
	} else if ( owner->spawnflags & 2 ) {
		ent->s.frame = 75;
	}

	// swing camera ?
	if ( owner->spawnflags & 4 ) {
		// set to 0 for no rotation at all
		ent->s.powerups = 0;
	}
	else {
		ent->s.powerups = 1;
	}

	// clientNum holds the rotate offset
	ent->s.clientNum = owner->s.clientNum;

	VectorCopy( owner->s.origin, ent->s.origin2 );

	// see if the portal_camera has a target
	target = G_PickTarget( owner->target );
	if ( target ) {
		VectorSubtract( target->s.origin, owner->s.origin, dir );
		VectorNormalize( dir );
	} else {
		G_SetMovedir( owner->s.angles, dir );
	}

	ent->s.eventParm = DirToByte( dir );
}

/*QUAKED misc_portal_surface (0 0 1) (-8 -8 -8) (8 8 8)
The portal surface nearest this entity will show a view from the targeted misc_portal_camera, or a mirror view if untargeted.
This must be within 64 world units of the surface!
*/
void SP_misc_portal_surface(gentity_t *ent) {
	VectorClear( ent->r.mins );
	VectorClear( ent->r.maxs );
	trap_LinkEntity (ent);

	ent->r.svFlags = SVF_PORTAL;
	ent->s.eType = ET_PORTAL;

	if ( !ent->target ) {
		VectorCopy( ent->s.origin, ent->s.origin2 );
	} else {
		ent->think = locateCamera;
		ent->nextthink = level.time + 100;
	}
}

/*QUAKED misc_portal_camera (0 0 1) (-8 -8 -8) (8 8 8) slowrotate fastrotate noswing
The target for a misc_portal_director.  You can set either angles or target another entity to determine the direction of view.
"roll" an angle modifier to orient the camera around the target vector;
*/
void SP_misc_portal_camera(gentity_t *ent) {
	float	roll;

	VectorClear( ent->r.mins );
	VectorClear( ent->r.maxs );
	trap_LinkEntity (ent);

	G_SpawnFloat( "roll", "0", &roll );

	ent->s.clientNum = roll/360.0 * 256;
}

/*
======================================================================

  SHOOTERS

======================================================================
*/

void Use_Shooter( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	vec3_t		dir;
	float		deg;
	vec3_t		up, right;
	gentity_t	*bolt;

	if ( !ent->r.linked )
		return;

	// see if we have a target
	if ( ent->enemy ) {
		VectorSubtract( ent->enemy->r.currentOrigin, ent->s.origin, dir );
		VectorNormalize( dir );
	} else if ( ent->spawnflags & 1 ) {
		if ( !level.player )
			return;

		VectorSubtract( level.player->r.currentOrigin, ent->s.origin, dir );
		VectorNormalize( dir );
	} else {
		VectorCopy( ent->movedir, dir );
	}

	// randomize a bit
	PerpendicularVector( up, dir );
	CrossProduct( up, dir, right );

	deg = crandom() * ent->random;
	VectorMA( dir, deg, up, dir );

	deg = crandom() * ent->random;
	VectorMA( dir, deg, right, dir );

	VectorNormalize( dir );

	switch ( ent->s.weapon ) {
		case WP_GRENADE_LAUNCHER:
			bolt = fire_grenade( ent, ent->s.origin, dir );
			bolt->damage = ent->damage;
			bolt->splashDamage = ent->damage;
			break;
		case WP_ROCKET_LAUNCHER:
			bolt = fire_rocket( ent, ent->s.origin, dir );
			bolt->damage = ent->damage;
			bolt->splashDamage = ent->damage;
			break;
		case WP_PLASMAGUN:
			bolt = fire_plasma( ent, ent->s.origin, dir );
			bolt->damage = ent->damage;
			bolt->splashDamage = ent->damage * 0.75;
			break;
		case WP_BFG:
			bolt = fire_bfg( ent, ent->s.origin, dir );
			bolt->damage = ent->damage;
			bolt->splashDamage = ent->damage;
			break;
	}

	G_AddEvent( ent, EV_FIRE_WEAPON, 0 );
}


static void InitShooter_Finish( gentity_t *ent ) {
	ent->enemy = G_PickTarget( ent->target );
	ent->think = 0;
	ent->nextthink = 0;
}

void InitShooter( gentity_t *ent, int weapon ) {
	ent->use = Use_Shooter;
	ent->s.weapon = weapon;

	RegisterItem( BG_FindItemForWeapon( weapon ) );

	G_SetMovedir( ent->s.angles, ent->movedir );

	if ( !ent->random ) {
		ent->random = 1.0;
	}
	ent->random = sin( M_PI * ent->random / 180 );
	// target might be a moving object, so we can't set movedir for it
	if ( ent->target ) {
		ent->think = InitShooter_Finish;
		ent->nextthink = level.time + 500;
	}
	trap_LinkEntity( ent );
}

/*QUAKED shooter_rocket (1 0 0) (-16 -16 -16) (16 16 16) TARGET_PLAYER
Fires at either the target or the current direction.
"random" the number of degrees of deviance from the taget. (1.0 default)
*/
void SP_shooter_rocket( gentity_t *ent ) {
	G_SpawnInt( "dmg", "100", &ent->damage );
	InitShooter( ent, WP_ROCKET_LAUNCHER );
}

/*QUAKED shooter_plasma (1 0 0) (-16 -16 -16) (16 16 16) TARGET_PLAYER
Fires at either the target or the current direction.
"random" is the number of degrees of deviance from the taget. (1.0 default)
*/
void SP_shooter_plasma( gentity_t *ent ) {
	G_SpawnInt( "dmg", "20", &ent->damage );
	InitShooter( ent, WP_PLASMAGUN);
}

/*QUAKED shooter_grenade (1 0 0) (-16 -16 -16) (16 16 16) TARGET_PLAYER
Fires at either the target or the current direction.
"random" is the number of degrees of deviance from the taget. (1.0 default)
*/
void SP_shooter_grenade( gentity_t *ent ) {
	G_SpawnInt( "dmg", "100", &ent->damage );
	InitShooter( ent, WP_GRENADE_LAUNCHER);
}

/*QUAKED shooter_bfg (1 0 0) (-16 -16 -16) (16 16 16) TARGET_PLAYER
Fires at either the target or the current direction.
"random" is the number of degrees of deviance from the taget. (1.0 default)
*/
void SP_shooter_bfg( gentity_t *ent ) {
	G_SpawnInt( "dmg", "100", &ent->damage );
	InitShooter( ent, WP_BFG );
}