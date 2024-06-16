//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: The various ammo types for HL2	
//
//=============================================================================//

#include "cbase.h"
#include "props.h"
#include "items.h"
#include "item_dynamic_resupply.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ITEM_CRATE_MODEL_NAME  "models/items/item_item_crate.mdl"


//-----------------------------------------------------------------------------
// A breakable crate that drops items
//-----------------------------------------------------------------------------
class CItem_ItemCrate : public CPhysicsProp
{
public:
	DECLARE_CLASS( CItem_ItemCrate, CPhysicsProp );
	DECLARE_DATADESC();

	void Precache( void );
	void Spawn( void );

	virtual int		OnTakeDamage( const CTakeDamageInfo &info );

	void InputKill( inputdata_t &data );

	virtual void VPhysicsCollision( int index, gamevcollisionevent_t *pEvent );

protected:
	virtual void OnBreak( const Vector &vecVelocity, const AngularImpulse &angVel, CBaseEntity *pBreaker );

private:
	// Crate types. Add more!
	enum CrateType_t
	{
		CRATE_SPECIFIC_ITEM = 0,

		CRATE_TYPE_COUNT,
	};

private:
	CrateType_t m_CrateType;
	string_t m_strItemClass;
	int m_nItemCount;
};


LINK_ENTITY_TO_CLASS(item_item_crate, CItem_ItemCrate);


//-----------------------------------------------------------------------------
// Save/load: 
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CItem_ItemCrate )

	DEFINE_KEYFIELD( m_CrateType, FIELD_INTEGER, "CrateType" ),	
	DEFINE_KEYFIELD( m_strItemClass, FIELD_STRING, "ItemClass" ),	
	DEFINE_KEYFIELD( m_nItemCount, FIELD_INTEGER, "ItemCount" ),	
	DEFINE_INPUTFUNC( FIELD_VOID, "Kill", InputKill ),

END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItem_ItemCrate::Precache( void )
{
	// Set this here to quiet base prop warnings
	PrecacheModel( ITEM_CRATE_MODEL_NAME );
	SetModel( ITEM_CRATE_MODEL_NAME );

	BaseClass::Precache();
	if ( m_CrateType == CRATE_SPECIFIC_ITEM )
	{
		if ( NULL_STRING != m_strItemClass )
		{
			// Don't precache if this is a null string. 
			UTIL_PrecacheOther( STRING(m_strItemClass) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItem_ItemCrate::Spawn( void )
{ 
	DisableAutoFade();
	SetModelName( MAKE_STRING( ITEM_CRATE_MODEL_NAME ) );

	if ( NULL_STRING == m_strItemClass )
	{
		Warning( "CItem_ItemCrate(%i):  CRATE_SPECIFIC_ITEM with NULL ItemClass string (deleted)!!!\n", entindex() );
		UTIL_Remove( this );
		return;
	}

	Precache( );
	SetModel( ITEM_CRATE_MODEL_NAME );
	AddEFlags( EFL_NO_ROTORWASH_PUSH );
	BaseClass::Spawn( );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void CItem_ItemCrate::InputKill( inputdata_t &data )
{
	UTIL_Remove( this );
}


//-----------------------------------------------------------------------------
// Item crates blow up immediately
//-----------------------------------------------------------------------------
int CItem_ItemCrate::OnTakeDamage( const CTakeDamageInfo &info )
{
	if ( info.GetDamageType() & DMG_AIRBOAT )
	{
		CTakeDamageInfo dmgInfo = info;
		dmgInfo.ScaleDamage( 10.0 );
		return BaseClass::OnTakeDamage( dmgInfo );
	}

	return BaseClass::OnTakeDamage( info );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItem_ItemCrate::VPhysicsCollision( int index, gamevcollisionevent_t *pEvent )
{
	float flDamageScale = 1.0f;
	if ( FClassnameIs( pEvent->pEntities[!index], "prop_vehicle_airboat" )	)
	{
		flDamageScale = 100.0f;
	}

	m_impactEnergyScale *= flDamageScale;
	BaseClass::VPhysicsCollision( index, pEvent );
	m_impactEnergyScale /= flDamageScale;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItem_ItemCrate::OnBreak( const Vector &vecVelocity, const AngularImpulse &angImpulse, CBaseEntity *pBreaker )
{
	// FIXME: We could simply store the name of an entity to put into the crate 
	// as a string entered in by worldcraft. Should we?	I'd do it for sure
	// if it was easy to get a dropdown with all entity types in it.

	for ( int i = 0; i < m_nItemCount; ++i )
	{
		CBaseEntity *pSpawn = NULL;
		switch( m_CrateType )
		{
		case CRATE_SPECIFIC_ITEM:
			pSpawn = CreateEntityByName( STRING(m_strItemClass) );
			break;

		default:
			break;
		}

		if ( !pSpawn )
			return;

		// Give a little randomness...
		Vector vecOrigin;
		CollisionProp()->RandomPointInBounds( Vector(0.25, 0.25, 0.25), Vector( 0.75, 0.75, 0.75 ), &vecOrigin );
		pSpawn->SetAbsOrigin( vecOrigin );

		QAngle vecAngles;
		vecAngles.x = random->RandomFloat( -20.0f, 20.0f );
		vecAngles.y = random->RandomFloat( 0.0f, 360.0f );
		vecAngles.z = random->RandomFloat( -20.0f, 20.0f );
		pSpawn->SetAbsAngles( vecAngles );

		Vector vecActualVelocity;
		vecActualVelocity.Random( -10.0f, 10.0f );
//		vecActualVelocity += vecVelocity;
		pSpawn->SetAbsVelocity( vecActualVelocity );

		QAngle angVel;
		AngularImpulseToQAngle( angImpulse, angVel );
		pSpawn->SetLocalAngularVelocity( angVel );

		// If we're creating an item, it can't be picked up until it comes to rest
		// But only if it wasn't broken by a vehicle
		CItem *pItem = dynamic_cast<CItem*>(pSpawn);
		if ( pItem && !pBreaker->GetServerVehicle())
		{
			pItem->ActivateWhenAtRest();
		}

		pSpawn->Spawn();

		// Avoid missing items drops by a dynamic resupply because they don't think immediately
		if ( FClassnameIs( pSpawn, "item_dynamic_resupply" ) )
		{
			if ( i == 0 )
			{
				pSpawn->AddSpawnFlags( SF_DYNAMICRESUPPLY_ALWAYS_SPAWN );
			}
			pSpawn->SetNextThink( gpGlobals->curtime );
		}
	}
}
