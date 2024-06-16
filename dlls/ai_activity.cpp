//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Activities that are available to all NPCs.
//
//=============================================================================//

#include "cbase.h"
#include "ai_activity.h"
#include "ai_basenpc.h"
#include "stringregistry.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//=============================================================================
// Init static variables
//=============================================================================
CStringRegistry* CAI_BaseNPC::m_pActivitySR	= NULL;
int				 CAI_BaseNPC::m_iNumActivities = 0;

//-----------------------------------------------------------------------------
// Purpose: Add an activity to the activity string registry and increment
//			the acitivty counter
//-----------------------------------------------------------------------------
void CAI_BaseNPC::AddActivityToSR(const char *actName, int actID) 
{
	Assert( m_pActivitySR );
	if ( !m_pActivitySR )
		return;

	// technically order isn't dependent, but it's too damn easy to forget to add new ACT_'s to all three lists.
	static int lastActID = -2;
	Assert( actID >= LAST_SHARED_ACTIVITY || actID == lastActID + 1 || actID == ACT_INVALID );
	lastActID = actID;

	m_pActivitySR->AddString(actName, actID);
	m_iNumActivities++;
}

//-----------------------------------------------------------------------------
// Purpose: Given and activity ID, return the activity name
//-----------------------------------------------------------------------------
const char *CAI_BaseNPC::GetActivityName(int actID) 
{
	Assert( m_pActivitySR );
	if ( !m_pActivitySR )
		return "!!INVALID!!";

	const char *name = m_pActivitySR->GetStringText(actID);	

	if( !name )
	{
		AssertOnce( !"CAI_BaseNPC::GetActivityName() returning NULL!" );
	}

	return name;
}

//-----------------------------------------------------------------------------
// Purpose: Given and activity name, return the activity ID
//-----------------------------------------------------------------------------
int CAI_BaseNPC::GetActivityID(const char* actName) 
{
	Assert( m_pActivitySR );
	if ( !m_pActivitySR )
		return ACT_INVALID;

	return m_pActivitySR->GetStringID(actName);
}

#define ADD_ACTIVITY_TO_SR(activityname) AddActivityToSR(#activityname,activityname)

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InitDefaultActivitySR(void) 
{
	ADD_ACTIVITY_TO_SR( ACT_INVALID );
	ADD_ACTIVITY_TO_SR( ACT_RESET );
	ADD_ACTIVITY_TO_SR( ACT_IDLE );
	ADD_ACTIVITY_TO_SR( ACT_TRANSITION );
	ADD_ACTIVITY_TO_SR( ACT_COVER );
	ADD_ACTIVITY_TO_SR( ACT_COVER_MED );
	ADD_ACTIVITY_TO_SR( ACT_COVER_LOW );
	ADD_ACTIVITY_TO_SR( ACT_WALK );
	ADD_ACTIVITY_TO_SR( ACT_WALK_AIM );
	ADD_ACTIVITY_TO_SR( ACT_WALK_CROUCH );
	ADD_ACTIVITY_TO_SR( ACT_WALK_CROUCH_AIM );
	ADD_ACTIVITY_TO_SR( ACT_RUN );
	ADD_ACTIVITY_TO_SR( ACT_RUN_AIM );
	ADD_ACTIVITY_TO_SR( ACT_RUN_CROUCH );
	ADD_ACTIVITY_TO_SR( ACT_RUN_CROUCH_AIM );
	ADD_ACTIVITY_TO_SR( ACT_RUN_PROTECTED );
	ADD_ACTIVITY_TO_SR( ACT_SCRIPT_CUSTOM_MOVE );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK1 );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK2 );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK1_LOW );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK2_LOW );
	ADD_ACTIVITY_TO_SR( ACT_DIESIMPLE );
	ADD_ACTIVITY_TO_SR( ACT_DIEBACKWARD );
	ADD_ACTIVITY_TO_SR( ACT_DIEFORWARD );
	ADD_ACTIVITY_TO_SR( ACT_DIEVIOLENT );
	ADD_ACTIVITY_TO_SR( ACT_DIERAGDOLL );
	ADD_ACTIVITY_TO_SR( ACT_FLY );
	ADD_ACTIVITY_TO_SR( ACT_HOVER );
	ADD_ACTIVITY_TO_SR( ACT_GLIDE );
	ADD_ACTIVITY_TO_SR( ACT_SWIM );
	ADD_ACTIVITY_TO_SR( ACT_JUMP );
	ADD_ACTIVITY_TO_SR( ACT_HOP );
	ADD_ACTIVITY_TO_SR( ACT_LEAP );
	ADD_ACTIVITY_TO_SR( ACT_LAND );
	ADD_ACTIVITY_TO_SR( ACT_CLIMB_UP );
	ADD_ACTIVITY_TO_SR( ACT_CLIMB_DOWN );
	ADD_ACTIVITY_TO_SR( ACT_CLIMB_DISMOUNT );
	ADD_ACTIVITY_TO_SR( ACT_SHIPLADDER_UP );
	ADD_ACTIVITY_TO_SR( ACT_SHIPLADDER_DOWN );
	ADD_ACTIVITY_TO_SR( ACT_STRAFE_LEFT );
	ADD_ACTIVITY_TO_SR( ACT_STRAFE_RIGHT );
	ADD_ACTIVITY_TO_SR( ACT_ROLL_LEFT );
	ADD_ACTIVITY_TO_SR( ACT_ROLL_RIGHT );
	ADD_ACTIVITY_TO_SR( ACT_TURN_LEFT );
	ADD_ACTIVITY_TO_SR( ACT_TURN_RIGHT );
	ADD_ACTIVITY_TO_SR( ACT_CROUCH );
	ADD_ACTIVITY_TO_SR( ACT_CROUCHIDLE );
	ADD_ACTIVITY_TO_SR( ACT_STAND );
	ADD_ACTIVITY_TO_SR( ACT_USE );
	ADD_ACTIVITY_TO_SR( ACT_SIGNAL1 );
	ADD_ACTIVITY_TO_SR( ACT_SIGNAL2 );
	ADD_ACTIVITY_TO_SR( ACT_SIGNAL3 );

	ADD_ACTIVITY_TO_SR( ACT_SIGNAL_ADVANCE );
	ADD_ACTIVITY_TO_SR( ACT_SIGNAL_FORWARD );
	ADD_ACTIVITY_TO_SR( ACT_SIGNAL_GROUP );
	ADD_ACTIVITY_TO_SR( ACT_SIGNAL_HALT );
	ADD_ACTIVITY_TO_SR( ACT_SIGNAL_LEFT );
	ADD_ACTIVITY_TO_SR( ACT_SIGNAL_RIGHT );
	ADD_ACTIVITY_TO_SR( ACT_SIGNAL_TAKECOVER );

	ADD_ACTIVITY_TO_SR( ACT_LOOKBACK_RIGHT );
	ADD_ACTIVITY_TO_SR( ACT_LOOKBACK_LEFT );
	ADD_ACTIVITY_TO_SR( ACT_COWER );
	ADD_ACTIVITY_TO_SR( ACT_SMALL_FLINCH );
	ADD_ACTIVITY_TO_SR( ACT_BIG_FLINCH );
	ADD_ACTIVITY_TO_SR( ACT_MELEE_ATTACK1 );
	ADD_ACTIVITY_TO_SR( ACT_MELEE_ATTACK2 );
	ADD_ACTIVITY_TO_SR( ACT_RELOAD );
	ADD_ACTIVITY_TO_SR( ACT_RELOAD_LOW );
	ADD_ACTIVITY_TO_SR( ACT_ARM );
	ADD_ACTIVITY_TO_SR( ACT_DISARM );
	ADD_ACTIVITY_TO_SR( ACT_PICKUP_GROUND );
	ADD_ACTIVITY_TO_SR( ACT_PICKUP_RACK );
	ADD_ACTIVITY_TO_SR( ACT_IDLE_ANGRY );

	ADD_ACTIVITY_TO_SR( ACT_IDLE_RELAXED );
	ADD_ACTIVITY_TO_SR( ACT_IDLE_STIMULATED );
	ADD_ACTIVITY_TO_SR( ACT_IDLE_AGITATED );

	ADD_ACTIVITY_TO_SR( ACT_WALK_RELAXED );
	ADD_ACTIVITY_TO_SR( ACT_WALK_STIMULATED );
	ADD_ACTIVITY_TO_SR( ACT_WALK_AGITATED );

	ADD_ACTIVITY_TO_SR( ACT_RUN_RELAXED );
	ADD_ACTIVITY_TO_SR( ACT_RUN_STIMULATED );
	ADD_ACTIVITY_TO_SR( ACT_RUN_AGITATED );

	ADD_ACTIVITY_TO_SR( ACT_IDLE_AIM_RELAXED );
	ADD_ACTIVITY_TO_SR( ACT_IDLE_AIM_STIMULATED );
	ADD_ACTIVITY_TO_SR( ACT_IDLE_AIM_AGITATED );

	ADD_ACTIVITY_TO_SR( ACT_WALK_AIM_RELAXED );
	ADD_ACTIVITY_TO_SR( ACT_WALK_AIM_STIMULATED );
	ADD_ACTIVITY_TO_SR( ACT_WALK_AIM_AGITATED );

	ADD_ACTIVITY_TO_SR( ACT_RUN_AIM_RELAXED );
	ADD_ACTIVITY_TO_SR( ACT_RUN_AIM_STIMULATED );
	ADD_ACTIVITY_TO_SR( ACT_RUN_AIM_AGITATED );

	ADD_ACTIVITY_TO_SR( ACT_WALK_HURT );
	ADD_ACTIVITY_TO_SR( ACT_RUN_HURT );
	ADD_ACTIVITY_TO_SR( ACT_SPECIAL_ATTACK1 );
	ADD_ACTIVITY_TO_SR( ACT_SPECIAL_ATTACK2 );
	ADD_ACTIVITY_TO_SR( ACT_COMBAT_IDLE );
	ADD_ACTIVITY_TO_SR( ACT_WALK_SCARED );
	ADD_ACTIVITY_TO_SR( ACT_RUN_SCARED );
	ADD_ACTIVITY_TO_SR( ACT_VICTORY_DANCE );
	
	ADD_ACTIVITY_TO_SR( ACT_DIE_HEADSHOT );
	ADD_ACTIVITY_TO_SR( ACT_DIE_CHESTSHOT );
	ADD_ACTIVITY_TO_SR( ACT_DIE_GUTSHOT );
	ADD_ACTIVITY_TO_SR( ACT_DIE_BACKSHOT );

	ADD_ACTIVITY_TO_SR( ACT_FLINCH_HEAD );
	ADD_ACTIVITY_TO_SR( ACT_FLINCH_CHEST );
	ADD_ACTIVITY_TO_SR( ACT_FLINCH_STOMACH );
	ADD_ACTIVITY_TO_SR( ACT_FLINCH_LEFTARM );
	ADD_ACTIVITY_TO_SR( ACT_FLINCH_RIGHTARM );
	ADD_ACTIVITY_TO_SR( ACT_FLINCH_LEFTLEG );
	ADD_ACTIVITY_TO_SR( ACT_FLINCH_RIGHTLEG );
	ADD_ACTIVITY_TO_SR( ACT_FLINCH_PHYSICS );

	ADD_ACTIVITY_TO_SR( ACT_IDLE_ON_FIRE );
	ADD_ACTIVITY_TO_SR( ACT_WALK_ON_FIRE );
	ADD_ACTIVITY_TO_SR( ACT_RUN_ON_FIRE );

	ADD_ACTIVITY_TO_SR( ACT_RAPPEL_LOOP );

	ADD_ACTIVITY_TO_SR( ACT_180_LEFT );
	ADD_ACTIVITY_TO_SR( ACT_180_RIGHT );
	ADD_ACTIVITY_TO_SR( ACT_90_LEFT );
	ADD_ACTIVITY_TO_SR( ACT_90_RIGHT );
	ADD_ACTIVITY_TO_SR( ACT_STEP_LEFT );
	ADD_ACTIVITY_TO_SR( ACT_STEP_RIGHT );
	ADD_ACTIVITY_TO_SR( ACT_STEP_BACK );
	ADD_ACTIVITY_TO_SR( ACT_STEP_FORE );

	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK1 );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK2 );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_MELEE_ATTACK1 );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_MELEE_ATTACK2 );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK1_LOW );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK2_LOW );

	ADD_ACTIVITY_TO_SR( ACT_MELEE_ATTACK_SWING_GESTURE );

	ADD_ACTIVITY_TO_SR( ACT_GESTURE_SMALL_FLINCH );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_BIG_FLINCH );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_FLINCH_BLAST );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_FLINCH_HEAD );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_FLINCH_CHEST );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_FLINCH_STOMACH );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_FLINCH_LEFTARM );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_FLINCH_RIGHTARM );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_FLINCH_LEFTLEG );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_FLINCH_RIGHTLEG );

	ADD_ACTIVITY_TO_SR( ACT_GESTURE_TURN_LEFT );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_TURN_RIGHT );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_TURN_LEFT45 );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_TURN_RIGHT45 );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_TURN_LEFT90 );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_TURN_RIGHT90 );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_TURN_LEFT45_FLAT );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_TURN_RIGHT45_FLAT );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_TURN_LEFT90_FLAT );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_TURN_RIGHT90_FLAT );

	ADD_ACTIVITY_TO_SR( ACT_BARNACLE_HIT );
	ADD_ACTIVITY_TO_SR( ACT_BARNACLE_PULL );
	ADD_ACTIVITY_TO_SR( ACT_BARNACLE_CHOMP );
	ADD_ACTIVITY_TO_SR( ACT_BARNACLE_CHEW );

	ADD_ACTIVITY_TO_SR( ACT_DO_NOT_DISTURB );

	ADD_ACTIVITY_TO_SR( ACT_VM_DRAW );
	ADD_ACTIVITY_TO_SR( ACT_VM_HOLSTER );
	ADD_ACTIVITY_TO_SR( ACT_VM_IDLE );
	ADD_ACTIVITY_TO_SR( ACT_VM_FIDGET );
	ADD_ACTIVITY_TO_SR( ACT_VM_PULLBACK );
	ADD_ACTIVITY_TO_SR( ACT_VM_PULLBACK_HIGH );
	ADD_ACTIVITY_TO_SR( ACT_VM_PULLBACK_LOW );
	ADD_ACTIVITY_TO_SR( ACT_VM_THROW );
	ADD_ACTIVITY_TO_SR( ACT_VM_PULLPIN );
	ADD_ACTIVITY_TO_SR( ACT_VM_PRIMARYATTACK );
	ADD_ACTIVITY_TO_SR( ACT_VM_SECONDARYATTACK );
	ADD_ACTIVITY_TO_SR( ACT_VM_RELOAD );
	ADD_ACTIVITY_TO_SR( ACT_VM_DRYFIRE );
	ADD_ACTIVITY_TO_SR( ACT_VM_HITLEFT );
	ADD_ACTIVITY_TO_SR( ACT_VM_HITLEFT2 );
	ADD_ACTIVITY_TO_SR( ACT_VM_HITRIGHT );
	ADD_ACTIVITY_TO_SR( ACT_VM_HITRIGHT2 );
	ADD_ACTIVITY_TO_SR( ACT_VM_HITCENTER );
	ADD_ACTIVITY_TO_SR( ACT_VM_HITCENTER2 );
	ADD_ACTIVITY_TO_SR( ACT_VM_MISSLEFT );
	ADD_ACTIVITY_TO_SR( ACT_VM_MISSLEFT2 );
	ADD_ACTIVITY_TO_SR( ACT_VM_MISSRIGHT );
	ADD_ACTIVITY_TO_SR( ACT_VM_MISSRIGHT2 );
	ADD_ACTIVITY_TO_SR( ACT_VM_MISSCENTER );
	ADD_ACTIVITY_TO_SR( ACT_VM_MISSCENTER2 );
	ADD_ACTIVITY_TO_SR( ACT_VM_HAULBACK );
	ADD_ACTIVITY_TO_SR( ACT_VM_SWINGHARD );
	ADD_ACTIVITY_TO_SR( ACT_VM_SWINGMISS );
	ADD_ACTIVITY_TO_SR( ACT_VM_SWINGHIT );
	ADD_ACTIVITY_TO_SR( ACT_VM_IDLE_TO_LOWERED );
	ADD_ACTIVITY_TO_SR( ACT_VM_IDLE_LOWERED );
	ADD_ACTIVITY_TO_SR( ACT_VM_LOWERED_TO_IDLE );
	ADD_ACTIVITY_TO_SR( ACT_VM_RECOIL1 );
	ADD_ACTIVITY_TO_SR( ACT_VM_RECOIL2 );
	ADD_ACTIVITY_TO_SR( ACT_VM_RECOIL3 );

	ADD_ACTIVITY_TO_SR( ACT_VM_ATTACH_SILENCER );
	ADD_ACTIVITY_TO_SR( ACT_VM_DETACH_SILENCER );

//===========================
// HL2 Specific Activities
//===========================

	// SLAM Specialty Activities
	ADD_ACTIVITY_TO_SR( ACT_SLAM_STICKWALL_IDLE );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_STICKWALL_ND_IDLE );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_STICKWALL_ATTACH );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_STICKWALL_ATTACH2 );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_STICKWALL_ND_ATTACH );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_STICKWALL_ND_ATTACH2 );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_STICKWALL_DETONATE );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_STICKWALL_DETONATOR_HOLSTER );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_STICKWALL_DRAW );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_STICKWALL_ND_DRAW );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_STICKWALL_TO_THROW );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_STICKWALL_TO_THROW_ND );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_STICKWALL_TO_TRIPMINE_ND );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_THROW_IDLE );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_THROW_ND_IDLE );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_THROW_THROW );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_THROW_THROW2 );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_THROW_THROW_ND );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_THROW_THROW_ND2 );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_THROW_DRAW );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_THROW_ND_DRAW );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_THROW_TO_STICKWALL );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_THROW_TO_STICKWALL_ND );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_THROW_DETONATE );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_THROW_DETONATOR_HOLSTER );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_THROW_TO_TRIPMINE_ND );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_TRIPMINE_IDLE );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_TRIPMINE_DRAW );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_TRIPMINE_ATTACH );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_TRIPMINE_ATTACH2 );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_TRIPMINE_TO_STICKWALL_ND );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_TRIPMINE_TO_THROW_ND );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_DETONATOR_IDLE );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_DETONATOR_DRAW );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_DETONATOR_DETONATE );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_DETONATOR_HOLSTER );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_DETONATOR_STICKWALL_DRAW );
	ADD_ACTIVITY_TO_SR( ACT_SLAM_DETONATOR_THROW_DRAW );

	// SHOTGUN Specialty Activities
	ADD_ACTIVITY_TO_SR( ACT_SHOTGUN_RELOAD_START );
	ADD_ACTIVITY_TO_SR( ACT_SHOTGUN_RELOAD_FINISH );
	ADD_ACTIVITY_TO_SR( ACT_SHOTGUN_PUMP );

	// SMG2 special activities
	ADD_ACTIVITY_TO_SR( ACT_SMG2_IDLE2 );
	ADD_ACTIVITY_TO_SR( ACT_SMG2_FIRE2 );
	ADD_ACTIVITY_TO_SR( ACT_SMG2_DRAW2 );
	ADD_ACTIVITY_TO_SR( ACT_SMG2_RELOAD2 );
	ADD_ACTIVITY_TO_SR( ACT_SMG2_DRYFIRE2 );
	ADD_ACTIVITY_TO_SR( ACT_SMG2_TOAUTO );
	ADD_ACTIVITY_TO_SR( ACT_SMG2_TOBURST );

	// Physcannon special activities
	ADD_ACTIVITY_TO_SR( ACT_PHYSCANNON_UPGRADE );

	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK_AR1 );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK_AR2 );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK_AR2_LOW );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK_AR2_GRENADE );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK_HMG1 );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK_ML );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK_SMG1 );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK_SMG1_LOW );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK_SMG2 );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK_SHOTGUN );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK_SHOTGUN_LOW );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK_PISTOL );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK_PISTOL_LOW );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK_SLAM );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK_TRIPWIRE );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK_THROW );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK_SNIPER_RIFLE );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_ATTACK_RPG );
	ADD_ACTIVITY_TO_SR( ACT_MELEE_ATTACK_SWING );

	ADD_ACTIVITY_TO_SR( ACT_RANGE_AIM_LOW );

	ADD_ACTIVITY_TO_SR( ACT_RANGE_AIM_SMG1_LOW );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_AIM_PISTOL_LOW );
	ADD_ACTIVITY_TO_SR( ACT_RANGE_AIM_AR2_LOW );

	ADD_ACTIVITY_TO_SR( ACT_COVER_PISTOL_LOW );
	ADD_ACTIVITY_TO_SR( ACT_COVER_SMG1_LOW );

	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK_AR1 );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK_AR2 );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK_AR2_GRENADE );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK_HMG1 );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK_ML );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK_SMG1 );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK_SMG1_LOW );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK_SMG2 );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK_SHOTGUN );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK_PISTOL );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK_PISTOL_LOW );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK_SLAM );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK_TRIPWIRE );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK_THROW );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RANGE_ATTACK_SNIPER_RIFLE );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_MELEE_ATTACK_SWING );

	ADD_ACTIVITY_TO_SR( ACT_IDLE_RIFLE );
	ADD_ACTIVITY_TO_SR( ACT_IDLE_SMG1 );
	ADD_ACTIVITY_TO_SR( ACT_IDLE_ANGRY_SMG1 );
	ADD_ACTIVITY_TO_SR( ACT_IDLE_PISTOL );
	ADD_ACTIVITY_TO_SR( ACT_IDLE_ANGRY_PISTOL );
	ADD_ACTIVITY_TO_SR( ACT_IDLE_ANGRY_SHOTGUN );

	ADD_ACTIVITY_TO_SR( ACT_IDLE_PACKAGE );
	ADD_ACTIVITY_TO_SR( ACT_WALK_PACKAGE );
	ADD_ACTIVITY_TO_SR( ACT_IDLE_SUITCASE );
	ADD_ACTIVITY_TO_SR( ACT_WALK_SUITCASE );

	ADD_ACTIVITY_TO_SR( ACT_IDLE_SMG1_RELAXED );
	ADD_ACTIVITY_TO_SR( ACT_IDLE_SMG1_STIMULATED );

	ADD_ACTIVITY_TO_SR( ACT_WALK_RIFLE_RELAXED );
	ADD_ACTIVITY_TO_SR( ACT_RUN_RIFLE_RELAXED );
	ADD_ACTIVITY_TO_SR( ACT_WALK_RIFLE_STIMULATED );
	ADD_ACTIVITY_TO_SR( ACT_RUN_RIFLE_STIMULATED );

	ADD_ACTIVITY_TO_SR( ACT_IDLE_AIM_RIFLE_STIMULATED );
	ADD_ACTIVITY_TO_SR( ACT_WALK_AIM_RIFLE_STIMULATED );
	ADD_ACTIVITY_TO_SR( ACT_RUN_AIM_RIFLE_STIMULATED );

	ADD_ACTIVITY_TO_SR( ACT_IDLE_SHOTGUN_RELAXED );
	ADD_ACTIVITY_TO_SR( ACT_IDLE_SHOTGUN_STIMULATED );
	ADD_ACTIVITY_TO_SR( ACT_IDLE_SHOTGUN_AGITATED );

	// Policing activities
	ADD_ACTIVITY_TO_SR( ACT_WALK_ANGRY );
	ADD_ACTIVITY_TO_SR( ACT_POLICE_HARASS1 );
	ADD_ACTIVITY_TO_SR( ACT_POLICE_HARASS2 );

	// Manned guns
	ADD_ACTIVITY_TO_SR( ACT_IDLE_MANNEDGUN );

	// Melee weapon activities
	ADD_ACTIVITY_TO_SR( ACT_IDLE_MELEE );
	ADD_ACTIVITY_TO_SR( ACT_IDLE_ANGRY_MELEE );

	// RPG activities
	ADD_ACTIVITY_TO_SR( ACT_IDLE_RPG_RELAXED );
	ADD_ACTIVITY_TO_SR( ACT_IDLE_RPG ); 
	ADD_ACTIVITY_TO_SR( ACT_IDLE_ANGRY_RPG );
	ADD_ACTIVITY_TO_SR( ACT_COVER_LOW_RPG ); 
	ADD_ACTIVITY_TO_SR( ACT_WALK_RPG );
	ADD_ACTIVITY_TO_SR( ACT_RUN_RPG ); 
	ADD_ACTIVITY_TO_SR( ACT_WALK_CROUCH_RPG ); 
	ADD_ACTIVITY_TO_SR( ACT_RUN_CROUCH_RPG ); 
	ADD_ACTIVITY_TO_SR( ACT_WALK_RPG_RELAXED ); 
	ADD_ACTIVITY_TO_SR( ACT_RUN_RPG_RELAXED ); 

	ADD_ACTIVITY_TO_SR( ACT_WALK_RIFLE );
	ADD_ACTIVITY_TO_SR( ACT_WALK_AIM_RIFLE );
	ADD_ACTIVITY_TO_SR( ACT_WALK_CROUCH_RIFLE );
	ADD_ACTIVITY_TO_SR( ACT_WALK_CROUCH_AIM_RIFLE );
	ADD_ACTIVITY_TO_SR( ACT_RUN_RIFLE );
	ADD_ACTIVITY_TO_SR( ACT_RUN_AIM_RIFLE );
	ADD_ACTIVITY_TO_SR( ACT_RUN_CROUCH_RIFLE );
	ADD_ACTIVITY_TO_SR( ACT_RUN_CROUCH_AIM_RIFLE );

	ADD_ACTIVITY_TO_SR( ACT_WALK_AIM_SHOTGUN );
	ADD_ACTIVITY_TO_SR( ACT_RUN_AIM_SHOTGUN );

	ADD_ACTIVITY_TO_SR( ACT_WALK_AIM_PISTOL );
	ADD_ACTIVITY_TO_SR( ACT_RUN_AIM_PISTOL );

	ADD_ACTIVITY_TO_SR( ACT_RELOAD_PISTOL );
	ADD_ACTIVITY_TO_SR( ACT_RELOAD_PISTOL_LOW );
	ADD_ACTIVITY_TO_SR( ACT_RELOAD_SMG1 );
	ADD_ACTIVITY_TO_SR( ACT_RELOAD_SMG1_LOW );

	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RELOAD );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RELOAD_PISTOL );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_RELOAD_SMG1 );

	// Busy animations
	ADD_ACTIVITY_TO_SR( ACT_BUSY_LEAN_LEFT );
	ADD_ACTIVITY_TO_SR( ACT_BUSY_LEAN_LEFT_ENTRY );
	ADD_ACTIVITY_TO_SR( ACT_BUSY_LEAN_LEFT_EXIT );
	ADD_ACTIVITY_TO_SR( ACT_BUSY_LEAN_BACK );
	ADD_ACTIVITY_TO_SR( ACT_BUSY_LEAN_BACK_ENTRY );
	ADD_ACTIVITY_TO_SR( ACT_BUSY_LEAN_BACK_EXIT );
	ADD_ACTIVITY_TO_SR( ACT_BUSY_SIT_GROUND );
	ADD_ACTIVITY_TO_SR( ACT_BUSY_SIT_GROUND_ENTRY );
	ADD_ACTIVITY_TO_SR( ACT_BUSY_SIT_GROUND_EXIT );
	ADD_ACTIVITY_TO_SR( ACT_BUSY_SIT_CHAIR );
	ADD_ACTIVITY_TO_SR( ACT_BUSY_SIT_CHAIR_ENTRY );
	ADD_ACTIVITY_TO_SR( ACT_BUSY_SIT_CHAIR_EXIT );
	ADD_ACTIVITY_TO_SR( ACT_BUSY_STAND );
	ADD_ACTIVITY_TO_SR( ACT_BUSY_QUEUE );

	ADD_ACTIVITY_TO_SR( ACT_DIE_BARNACLE_SWALLOW );
	ADD_ACTIVITY_TO_SR( ACT_GESTURE_BARNACLE_STRANGLE );
	ADD_ACTIVITY_TO_SR( ACT_PHYSCANNON_DETACH );

	ADD_ACTIVITY_TO_SR( ACT_DIE_FRONTSIDE );
	ADD_ACTIVITY_TO_SR( ACT_DIE_RIGHTSIDE );
	ADD_ACTIVITY_TO_SR( ACT_DIE_BACKSIDE );
	ADD_ACTIVITY_TO_SR( ACT_DIE_LEFTSIDE );

	ADD_ACTIVITY_TO_SR( ACT_OPEN_DOOR );

//===========================
// TF2 Specific Activities
//===========================
	ADD_ACTIVITY_TO_SR( ACT_STARTDYING );
	ADD_ACTIVITY_TO_SR( ACT_DYINGLOOP );
	ADD_ACTIVITY_TO_SR( ACT_DYINGTODEAD );

	ADD_ACTIVITY_TO_SR( ACT_RIDE_MANNED_GUN );

	// All viewmodels
	ADD_ACTIVITY_TO_SR( ACT_VM_SPRINT_ENTER );
	ADD_ACTIVITY_TO_SR( ACT_VM_SPRINT_IDLE );
	ADD_ACTIVITY_TO_SR( ACT_VM_SPRINT_LEAVE );

	// Looping weapon firing
	ADD_ACTIVITY_TO_SR( ACT_FIRE_START );
	ADD_ACTIVITY_TO_SR( ACT_FIRE_LOOP );
	ADD_ACTIVITY_TO_SR( ACT_FIRE_END );

	ADD_ACTIVITY_TO_SR( ACT_CROUCHING_GRENADEIDLE );
	ADD_ACTIVITY_TO_SR( ACT_CROUCHING_GRENADEREADY );
	ADD_ACTIVITY_TO_SR( ACT_CROUCHING_PRIMARYATTACK );
	ADD_ACTIVITY_TO_SR( ACT_OVERLAY_GRENADEIDLE );
	ADD_ACTIVITY_TO_SR( ACT_OVERLAY_GRENADEREADY );
	ADD_ACTIVITY_TO_SR( ACT_OVERLAY_PRIMARYATTACK );
	ADD_ACTIVITY_TO_SR( ACT_OVERLAY_SHIELD_UP );
	ADD_ACTIVITY_TO_SR( ACT_OVERLAY_SHIELD_DOWN );
	ADD_ACTIVITY_TO_SR( ACT_OVERLAY_SHIELD_UP_IDLE );
	ADD_ACTIVITY_TO_SR( ACT_OVERLAY_SHIELD_ATTACK );
	ADD_ACTIVITY_TO_SR( ACT_OVERLAY_SHIELD_KNOCKBACK );
	ADD_ACTIVITY_TO_SR( ACT_SHIELD_UP );
	ADD_ACTIVITY_TO_SR( ACT_SHIELD_DOWN );
	ADD_ACTIVITY_TO_SR( ACT_SHIELD_UP_IDLE );
	ADD_ACTIVITY_TO_SR( ACT_SHIELD_ATTACK );
	ADD_ACTIVITY_TO_SR( ACT_SHIELD_KNOCKBACK );
	ADD_ACTIVITY_TO_SR( ACT_CROUCHING_SHIELD_UP );
	ADD_ACTIVITY_TO_SR( ACT_CROUCHING_SHIELD_DOWN );
	ADD_ACTIVITY_TO_SR( ACT_CROUCHING_SHIELD_UP_IDLE );
	ADD_ACTIVITY_TO_SR( ACT_CROUCHING_SHIELD_ATTACK );
	ADD_ACTIVITY_TO_SR( ACT_CROUCHING_SHIELD_KNOCKBACK );

	ADD_ACTIVITY_TO_SR( ACT_TURNRIGHT45 );
	ADD_ACTIVITY_TO_SR( ACT_TURNLEFT45 );
	ADD_ACTIVITY_TO_SR( ACT_TURN );

	// TF2 object animations
	ADD_ACTIVITY_TO_SR( ACT_OBJ_ASSEMBLING );
	ADD_ACTIVITY_TO_SR( ACT_OBJ_DISMANTLING );
	ADD_ACTIVITY_TO_SR( ACT_OBJ_STARTUP );
	ADD_ACTIVITY_TO_SR( ACT_OBJ_RUNNING );
	ADD_ACTIVITY_TO_SR( ACT_OBJ_IDLE );
	ADD_ACTIVITY_TO_SR( ACT_OBJ_PLACING );
	ADD_ACTIVITY_TO_SR( ACT_OBJ_DETERIORATING );
}
