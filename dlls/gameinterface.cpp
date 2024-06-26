//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: encapsulates and implements all the accessing of the game dll from external
//			sources (only the engine at the time of writing)
//			This files ONLY contains functions and data necessary to build an interface
//			to external modules
//=============================================================================//

#include "cbase.h"
#include "gamestringpool.h"
#include "mapentities_shared.h"
#include "game.h"
#include "entityapi.h"
#include "client.h"
#include "saverestore.h"
#include "entitylist.h"
#include "gamerules.h"
#include "soundent.h"
#include "player.h"
#include "server_class.h"
#include "ai_node.h"
#include "ai_link.h"
#include "ai_saverestore.h"
#include "ai_networkmanager.h"
#include "ndebugoverlay.h"
#include "ivoiceserver.h"
#include <stdarg.h>
#include "movehelper_server.h"
#include "networkstringtable_gamedll.h"
#include "filesystem.h"
#include "terrainmodmgr.h"
#include "func_areaportalwindow.h"
#include "igamesystem.h"
#include "init_factory.h"
#include "vstdlib/random.h"
#include "env_wind_shared.h"
#include "engine/IEngineSound.h"
#include "ispatialpartition.h"
#include "textstatsmgr.h"
#include "bitbuf.h"
#include "saverestoretypes.h"
#include "physics_saverestore.h"
#include "tier0/vprof.h"
#include "effect_dispatch_data.h"
#include "engine/IStaticPropMgr.h"
#include "TemplateEntities.h"
#include "ai_speech.h"
#include "soundenvelope.h"
#include "usermessages.h"
#include "physics.h"
#include "mapentities.h"
#include "igameevents.h"
#include "EventLog.h"
#include "engine/IVEngineCache.h"
#include "engine/ivdebugoverlay.h"
#include "shareddefs.h"
#include "props.h"
#include "timedeventmgr.h"
#include "gameinterface.h"
#include "eventqueue.h"
#include "hltvdirector.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "nav_mesh.h"
#include "AI_ResponseSystem.h"
#include "saverestore_stringtable.h"
#include "util.h"
#include "vstdlib/ICommandLine.h"

#ifdef CSTRIKE_DLL // BOTPORT: TODO: move these ifdefs out
#include "bot/bot.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void CServerGameClients::GetPlayerLimits( int& minplayers, int& maxplayers, int &defaultMaxPlayers ) const
{
	minplayers = defaultMaxPlayers = 1; 
	maxplayers = MAX_PLAYERS;
}

// -------------------------------------------------------------------------------------------- //
// Mod-specific CServerGameDLL implementation.
// -------------------------------------------------------------------------------------------- //

void CServerGameDLL::LevelInit_ParseAllEntities( const char *pMapEntities )
{
	MapEntity_ParseAllEntities( pMapEntities, NULL );
}

CTimedEventMgr g_NetworkPropertyEventMgr;


// Engine interfaces.
IVEngineServer	*engine = NULL;
IVoiceServer	*g_pVoiceServer = NULL;
ICvar			*cvar = NULL;
IFileSystem		*filesystem = NULL;
INetworkStringTableContainer *networkstringtable = NULL;
IStaticPropMgrServer *staticpropmgr = NULL;
IUniformRandomStream *random = NULL;
IEngineSound *enginesound = NULL;
ISpatialPartition *partition = NULL;
IVModelInfo *modelinfo = NULL;
IEngineTrace *enginetrace = NULL;
IGameEventManager *gameeventmanager = NULL;
IVEngineCache *engineCache = NULL;
IVDebugOverlay * debugoverlay = NULL;
ISoundEmitterSystemBase *soundemitterbase = NULL;

IGameSystem *SoundEmitterSystem();
bool SceneCacheInit();
void SceneCacheShutdown();
bool ModelSoundsCacheInit();
void ModelSoundsCacheShutdown();

void SceneManager_ClientActive( CBasePlayer *player );

#ifdef _DEBUG
static ConVar s_UseNetworkVars( "UseNetworkVars", "1", FCVAR_CHEAT, "For profiling, toggle network vars." );
#endif

extern ConVar sv_noclipduringpause;
ConVar sv_massreport( "sv_massreport", "0" );

ConVar sv_autosave( "sv_autosave", "1", 0, "Set to 1 to save game on level transition. Does not affect autosave triggers." );

// String tables
INetworkStringTable *g_pStringTableEffectDispatch = NULL;
INetworkStringTable *g_pStringTableVguiScreen = NULL;
INetworkStringTable *g_pStringTableMaterials = NULL;
INetworkStringTable *g_pStringTableInfoPanel = NULL;
CStringTableSaveRestoreOps g_VguiScreenStringOps;

// Holds global variables shared between engine and game.
CGlobalVars *gpGlobals;
edict_t *g_pDebugEdictBase = 0;
static int		g_nCommandClientIndex = 0;

static ConVar sv_showhitboxes( "sv_showhitboxes", "-1", FCVAR_CHEAT, "Send server-side hitboxes for specified entity to client (NOTE:  this uses lots of bandwidth, use on listen server only)." );

void PrecachePointTemplates();

static ClientPutInServerOverrideFn g_pClientPutInServerOverride = NULL;
static void UpdateChapterRestrictions( const char *mapname );

void ClientPutInServerOverride( ClientPutInServerOverrideFn fn )
{
	g_pClientPutInServerOverride = fn;
}



//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int UTIL_GetCommandClientIndex( void )
{
	// -1 == unknown,dedicated server console
	// 0  == player 1

	// Convert to 1 based offset
	return (g_nCommandClientIndex+1);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBasePlayer
//-----------------------------------------------------------------------------
CBasePlayer *UTIL_GetCommandClient( void )
{
	int idx = UTIL_GetCommandClientIndex();
	if ( idx > 0 )
	{
		return UTIL_PlayerByIndex( idx );
	}

	// HLDS console issued command
	return NULL;
}

extern void InitializeCvars( void );

CBaseEntity*	FindPickerEntity( CBasePlayer* pPlayer );
CAI_Node*		FindPickerAINode( CBasePlayer* pPlayer, NodeType_e nNodeType );
CAI_Link*		FindPickerAILink( CBasePlayer* pPlayer );
float			GetFloorZ(const Vector &origin);
void			UpdateAllClientData( void );
void			DrawMessageEntities();

#include "ai_network.h"

// For now just using one big AI network
extern ConVar think_limit;


#if 0
//-----------------------------------------------------------------------------
// Purpose: Draw output overlays for any measure sections
// Input  : 
//-----------------------------------------------------------------------------
void DrawMeasuredSections(void)
{
	int		row = 1;
	float	rowheight = 0.025;

	CMeasureSection *p = CMeasureSection::GetList();
	while ( p )
	{
		char str[256];
		Q_snprintf(str,sizeof(str),"%s",p->GetName());
		NDebugOverlay::ScreenText( 0.01,0.51+(row*rowheight),str, 255,255,255,255, 0.0 );
		
		Q_snprintf(str,sizeof(str),"%5.2f\n",p->GetTime().GetMillisecondsF());
		//Q_snprintf(str,sizeof(str),"%3.3f\n",p->GetTime().GetSeconds() * 100.0 / engine->Time());
		NDebugOverlay::ScreenText( 0.28,0.51+(row*rowheight),str, 255,255,255,255, 0.0 );

		Q_snprintf(str,sizeof(str),"%5.2f\n",p->GetMaxTime().GetMillisecondsF());
		//Q_snprintf(str,sizeof(str),"%3.3f\n",p->GetTime().GetSeconds() * 100.0 / engine->Time());
		NDebugOverlay::ScreenText( 0.34,0.51+(row*rowheight),str, 255,255,255,255, 0.0 );


		row++;

		p = p->GetNext();
	}

	bool sort_reset = false;

	// Time to redo sort?
	if ( measure_resort.GetFloat() > 0.0 &&
		engine->Time() >= CMeasureSection::m_dNextResort )
	{
		// Redo it
		CMeasureSection::SortSections();
		// Set next time
		CMeasureSection::m_dNextResort = engine->Time() + measure_resort.GetFloat();
		// Flag to reset sort accumulator, too
		sort_reset = true;
	}

	// Iterate through the sections now
	p = CMeasureSection::GetList();
	while ( p )
	{
		// Update max 
		p->UpdateMax();

		// Reset regular accum.
		p->Reset();
		// Reset sort accum less often
		if ( sort_reset )
		{
			p->SortReset();
		}
		p = p->GetNext();
	}

}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void DrawAllDebugOverlays( void ) 
{
	// If in debug select mode print the selection entities name or classname
	if (CBaseEntity::m_bInDebugSelect)
	{
		CBasePlayer* pPlayer =  UTIL_PlayerByIndex( CBaseEntity::m_nDebugPlayer );

		if (pPlayer)
		{
			// First try to trace a hull to an entity
			CBaseEntity *pEntity = FindPickerEntity( pPlayer );

			if ( pEntity ) 
			{
				pEntity->DrawDebugTextOverlays();
				pEntity->DrawBBoxOverlay();
				pEntity->SendDebugPivotOverlay();
			}
		}
	}

	// --------------------------------------------------------
	//  Draw debug overlay lines 
	// --------------------------------------------------------
	UTIL_DrawOverlayLines();

	// ------------------------------------------------------------------------
	// If in wc_edit mode draw a box to highlight which node I'm looking at
	// ------------------------------------------------------------------------
	if (engine->IsInEditMode())
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex( CBaseEntity::m_nDebugPlayer );
		if (pPlayer) 
		{
			if (g_pAINetworkManager->GetEditOps()->m_bLinkEditMode)
			{
				CAI_Link* pAILink = FindPickerAILink(pPlayer);
				if (pAILink)
				{
					// For now just using one big AI network
					Vector startPos = g_pBigAINet->GetNode(pAILink->m_iSrcID)->GetPosition(g_pAINetworkManager->GetEditOps()->m_iHullDrawNum);
					Vector endPos	= g_pBigAINet->GetNode(pAILink->m_iDestID)->GetPosition(g_pAINetworkManager->GetEditOps()->m_iHullDrawNum);
					Vector linkDir	= startPos-endPos;
					float linkLen = VectorNormalize( linkDir );
					
					// Draw in green if link that's been turned off
					if (pAILink->m_LinkInfo & bits_LINK_OFF)
					{
						NDebugOverlay::BoxDirection(startPos, Vector(-4,-4,-4), Vector(-linkLen,4,4), linkDir, 0,255,0,40,0);
					}
					else
					{
						NDebugOverlay::BoxDirection(startPos, Vector(-4,-4,-4), Vector(-linkLen,4,4), linkDir, 255,0,0,40,0);
					}
				}
			}
			else
			{
				CAI_Node* pAINode;
				if (g_pAINetworkManager->GetEditOps()->m_bAirEditMode)
				{
					pAINode = FindPickerAINode(pPlayer,NODE_AIR);
				}
				else
				{
					pAINode = FindPickerAINode(pPlayer,NODE_GROUND);
				}

				if (pAINode)
				{
					NDebugOverlay::Box(pAINode->GetPosition(g_pAINetworkManager->GetEditOps()->m_iHullDrawNum), Vector(-8,-8,-8), Vector(8,8,8), 255,0,0,40,0);
				}
			}
			// ------------------------------------
			// If in air edit mode draw guide line
			// ------------------------------------
			if (g_pAINetworkManager->GetEditOps()->m_bAirEditMode)
			{
				UTIL_DrawPositioningOverlay(g_pAINetworkManager->GetEditOps()->m_flAirEditDistance);
			}
			else
			{
				NDebugOverlay::DrawGroundCrossHairOverlay();
			}
		}
	}

	// For not just using one big AI Network
	if ( g_pAINetworkManager )
	{
		g_pAINetworkManager->GetEditOps()->DrawAINetworkOverlay();
	}

	// PERFORMANCE: only do this in developer mode
	if ( g_pDeveloper->GetInt() )
	{
		// iterate through all objects for debug overlays
		const CEntInfo *pInfo = gEntList.FirstEntInfo();

		for ( ;pInfo; pInfo = pInfo->m_pNext )
		{
			CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
			// HACKHACK: to flag off these calls
			if ( ent->m_debugOverlays || ent->m_pTimedOverlay )
			{
				ent->DrawDebugGeometryOverlays();
			}
		}
	}

	if ( sv_massreport.GetInt() )
	{
		// iterate through all objects for debug overlays
		const CEntInfo *pInfo = gEntList.FirstEntInfo();

		for ( ;pInfo; pInfo = pInfo->m_pNext )
		{
			CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
			if (!ent->VPhysicsGetObject())
				continue;

			char tempstr[512];
			Q_snprintf(tempstr, sizeof(tempstr),"%s: Mass: %.2f kg / %.2f lb (%s)", 
				ent->GetModelName(), ent->VPhysicsGetObject()->GetMass(), 
				kg2lbs(ent->VPhysicsGetObject()->GetMass()), 
				GetMassEquivalent(ent->VPhysicsGetObject()->GetMass()));
			NDebugOverlay::EntityText(ent->entindex(), 0, tempstr, 0);
		}
	}

	// A hack to draw point_message entities w/o developer required
	DrawMessageEntities();
}

CServerGameDLL g_ServerGameDLL;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CServerGameDLL, IServerGameDLL, INTERFACEVERSION_SERVERGAMEDLL, g_ServerGameDLL);

bool CServerGameDLL::DLLInit(CreateInterfaceFn engineFactory, 
		CreateInterfaceFn physicsFactory, CreateInterfaceFn fileSystemFactory, 
		CGlobalVars *pGlobals)
{
	if((engine = (IVEngineServer*)engineFactory(INTERFACEVERSION_VENGINESERVER, NULL)) == NULL ||
		(g_pVoiceServer = (IVoiceServer*)engineFactory(INTERFACEVERSION_VOICESERVER, NULL)) == NULL ||
		(cvar = (ICvar*)engineFactory(VENGINE_CVAR_INTERFACE_VERSION, NULL)) == NULL ||
		(networkstringtable = (INetworkStringTableContainer *)engineFactory(INTERFACENAME_NETWORKSTRINGTABLESERVER,NULL)) == NULL ||
		(staticpropmgr = (IStaticPropMgrServer *)engineFactory(INTERFACEVERSION_STATICPROPMGR_SERVER,NULL)) == NULL ||
		(random = (IUniformRandomStream *)engineFactory(VENGINE_SERVER_RANDOM_INTERFACE_VERSION, NULL)) == NULL ||
		(enginesound = (IEngineSound *)engineFactory(IENGINESOUND_SERVER_INTERFACE_VERSION, NULL)) == NULL ||
		(partition = (ISpatialPartition *)engineFactory(INTERFACEVERSION_SPATIALPARTITION, NULL)) == NULL ||
		(modelinfo = (IVModelInfo *)engineFactory(VMODELINFO_SERVER_INTERFACE_VERSION, NULL)) == NULL ||
		(enginetrace = (IEngineTrace *)engineFactory(INTERFACEVERSION_ENGINETRACE_SERVER,NULL)) == NULL ||
		(filesystem = (IFileSystem *)fileSystemFactory(FILESYSTEM_INTERFACE_VERSION,NULL)) == NULL ||
		(gameeventmanager = (IGameEventManager *)engineFactory(INTERFACEVERSION_GAMEEVENTSMANAGER,NULL)) == NULL ||
		(engineCache = (IVEngineCache*)engineFactory(VENGINE_CACHE_INTERFACE_VERSION, NULL )) == NULL ||
		(soundemitterbase = (ISoundEmitterSystemBase *)engineFactory(SOUNDEMITTERSYSTEM_INTERFACE_VERSION, NULL)) == NULL
	)
	{
		return false;
	}

	// Yes, both the client and game .dlls will try to Connect, the soundemittersystem.dll will handle this gracefully
	if ( !soundemitterbase->Connect( engineFactory ) )
	{
		return false;
	}

	// cache the globals
	gpGlobals = pGlobals;

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

	// save these in case other system inits need them
	factorylist_t factories;
	factories.engineFactory = engineFactory;
	factories.fileSystemFactory = fileSystemFactory;
	factories.physicsFactory = physicsFactory;
	FactoryList_Store( factories );

	// load used game events  
	gameeventmanager->LoadEventsFromFile("resource/gameevents.res");

	// init the cvar list first in case inits want to reference them
	InitializeCvars();
	
	sv_cheats = (ConVar*) ConCommandBase::FindCommand( "sv_cheats" );
	if ( !sv_cheats )
		return false;

	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetEntitySaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetPhysSaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetAISaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetTemplateSaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetDefaultResponseSystemSaveRestoreBlockHandler() );

	// The string system must init first + shutdown last
	IGameSystem::Add( GameStringSystem() );

	// Physics must occur before the sound envelope manager
	IGameSystem::Add( PhysicsGameSystem() );
	
	// Add game log system
	IGameSystem::Add( GameLogSystem() );

	// Add HLTV director 
	IGameSystem::Add( HLTVDirectorSystem() );

	// Add sound emitter
	IGameSystem::Add( SoundEmitterSystem() );

#ifdef CSTRIKE_DLL // BOTPORT: TODO: move these ifdefs out
	InstallBotControl();
#endif

	// load Mod specific game events ( MUST be before InitAllSystems() so it can pickup the mod specific events)
	gameeventmanager->LoadEventsFromFile("resource/ModEvents.res");

	if ( !IGameSystem::InitAllSystems() )
		return false;

	// Due to dependencies, these are not autogamesystems
	if ( !SceneCacheInit() )
	{
		return false;
	}

	// Due to dependencies, these are not autogamesystems
	if ( !ModelSoundsCacheInit() )
	{
		return false;
	}

	// try to get debug overlay, may be NULL if on HLDS
	debugoverlay = (IVDebugOverlay *)engineFactory( VDEBUG_OVERLAY_INTERFACE_VERSION, NULL );

	// create the Navigation Mesh interface
	TheNavMesh = new CNavMesh;

	return true;
}

void CServerGameDLL::DLLShutdown( void )
{
	// Due to dependencies, these are not autogamesystems
	ModelSoundsCacheShutdown();
	SceneCacheShutdown();

	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetDefaultResponseSystemSaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetTemplateSaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetAISaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetPhysSaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetEntitySaveRestoreBlockHandler() );

	char *pFilename = g_TextStatsMgr.GetStatsFilename();
	if ( !pFilename || !pFilename[0] )
	{
		g_TextStatsMgr.SetStatsFilename( "stats.txt" );
	}
	g_TextStatsMgr.WriteFile( filesystem );

	IGameSystem::ShutdownAllSystems();

	// destroy the Navigation Mesh interface
	if (TheNavMesh)
	{
		delete TheNavMesh;
		TheNavMesh = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: See shareddefs.h for redefining this.  Don't even think about it, though, for HL2.  Or you will pay.  ywb 9/22/03
// Output : float
//-----------------------------------------------------------------------------
float CServerGameDLL::GetTickInterval( void ) const
{
	COMPILE_TIME_ASSERT( TICK_INTERVAL >= MINIMUM_TICK_INTERVAL && TICK_INTERVAL <= MAXIMUM_TICK_INTERVAL );
	return TICK_INTERVAL;
}

// This is called when a new game is started. (restart, map)
bool CServerGameDLL::GameInit( void )
{
	ResetGlobalState();
	engine->ServerCommand( "exec game.cfg\n" );
	engine->ServerExecute( );
	// clear out any old game's temporary save data
//	engine->ClearSaveDir();
	return true;
}

// This is called when a game ends (server disconnect, death, restart, load)
// NOT on level transitions within a game
void CServerGameDLL::GameShutdown( void )
{
	ResetGlobalState();
}

static bool g_OneWayTransition = false;
void Game_SetOneWayTransition( void )
{
	g_OneWayTransition = true;
}

static CUtlVector<EHANDLE> g_RestoredEntities;
// just for debugging, assert that this is the only time this function is called
static bool g_InRestore = false;

void AddRestoredEntity( CBaseEntity *pEntity )
{
	Assert(g_InRestore);
	if ( !pEntity )
		return;

	g_RestoredEntities.AddToTail( EHANDLE(pEntity) );
}

void EndRestoreEntities()
{
	if ( !g_InRestore )
		return;

	// Call all entities' OnRestore handlers
	for ( int i = g_RestoredEntities.Count()-1; i >=0; --i )
	{
		CBaseEntity *pEntity = g_RestoredEntities[i].Get();
		if ( pEntity && !pEntity->IsDormant() )
		{
			engineCache->EnterCriticalSection();
			pEntity->OnRestore();
			engineCache->ExitCriticalSection();
		}
	}

	g_RestoredEntities.Purge();

	IGameSystem::OnRestoreAllSystems();

	g_InRestore = false;
	gEntList.CleanupDeleteList();

	// HACKHACK: UNDONE: We need to redesign the main loop with respect to save/load/server activate
	g_ServerGameDLL.ServerActivate( NULL, 0, 0 );
}

void BeginRestoreEntities()
{
	if ( g_InRestore )
	{
		DevMsg( "BeginRestoreEntities without previous EndRestoreEntities.\n" );
		gEntList.CleanupDeleteList();
	}
	g_RestoredEntities.Purge();
	g_InRestore = true;
}

//-----------------------------------------------------------------------------
// Purpose: This prevents sv.tickcount/gpGlobals->tickcount from advancing during restore which
//  would cause a lot of the NPCs to fast forward their think times to the same
//  tick due to some ticks being elapsed during restore where there was no simulation going on
//-----------------------------------------------------------------------------
bool CServerGameDLL::IsRestoring()
{
	return g_InRestore;
}

// Called any time a new level is started (after GameInit() also on level transitions within a game)
bool CServerGameDLL::LevelInit( const char *pMapName, char const *pMapEntities, char const *pOldLevel, char const *pLandmarkName, bool loadGame, bool background )
{
	VPROF("CServerGameDLL::LevelInit");
	ResetWindspeed();
	UpdateChapterRestrictions( pMapName );

	// IGameSystem::LevelInitPreEntityAllSystems() is called when the world is precached
	// That happens either in LoadGameState() or in MapEntity_ParseAllEntities()
	if ( loadGame )
	{
		BeginRestoreEntities();
		if ( !engine->LoadGameState( pMapName, 1 ) )
		{
			MapEntity_ParseAllEntities( pMapEntities );
		}

		if ( pOldLevel )
		{
			gpGlobals->eLoadType = MapLoad_Transition;
			engine->LoadAdjacentEnts( pOldLevel, pLandmarkName );
		}
		else
		{
			gpGlobals->eLoadType = MapLoad_LoadGame;
		}

		if ( g_OneWayTransition )
		{
			engine->ClearSaveDir();
		}

		if ( pOldLevel && sv_autosave.GetBool() == true )
		{
			// This is a single-player style level transition.
			// Queue up an autosave one second into the level
			CBaseEntity *pAutosave = CBaseEntity::Create( "logic_autosave", vec3_origin, vec3_angle, NULL );
			if ( pAutosave )
			{
				g_EventQueue.AddEvent( pAutosave, "Save", 1.0, NULL, NULL );
				g_EventQueue.AddEvent( pAutosave, "Kill", 1.1, NULL, NULL );
			}

		}
	}
	else
	{
		if ( background )
		{
			gpGlobals->eLoadType = MapLoad_Background;
		}
		else
		{
			gpGlobals->eLoadType = MapLoad_NewGame;
		}

		LevelInit_ParseAllEntities( pMapEntities );

	}

	// Now that all of the active entities have been loaded in, precache any entities who need point_template parameters
	//  to be parsed (the above code has loaded all point_template entities)
	PrecachePointTemplates();

	// load MOTD from file into stringtable
	LoadMessageOfTheDay();

	// Sometimes an ent will Remove() itself during its precache, so RemoveImmediate won't happen.
	// This makes sure those ents get cleaned up.
	gEntList.CleanupDeleteList();

	g_AIFriendliesTalkSemaphore.Release();
	g_AIFoesTalkSemaphore.Release();
	g_OneWayTransition = false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: called after every level change and load game, iterates through all the
//			active entities and gives them a chance to fix up their state
//-----------------------------------------------------------------------------
#ifdef DEBUG
bool g_bReceivedChainedActivate;
bool g_bCheckForChainedActivate;
#define BeginCheckChainedActivate() if (0) ; else { g_bCheckForChainedActivate = true; g_bReceivedChainedActivate = false; }
#define EndCheckChainedActivate( bCheck )	\
	if (0) ; else \
	{ \
		if ( bCheck ) \
		{ \
			char msg[ 1024 ];	\
			Q_snprintf( msg, sizeof( msg ),  "Entity (%i/%s/%s) failed to call base class Activate()\n", pClass->entindex(), pClass->GetClassname(), STRING( pClass->GetEntityName() ) );	\
			AssertMsg( g_bReceivedChainedActivate == true, msg ); \
		} \
		g_bCheckForChainedActivate = false; \
	}
#else
#define BeginCheckChainedActivate()			((void)0)
#define EndCheckChainedActivate( bCheck )	((void)0)
#endif

void CServerGameDLL::ServerActivate( edict_t *pEdictList, int edictCount, int clientMax )
{
	// HACKHACK: UNDONE: We need to redesign the main loop with respect to save/load/server activate
	if ( g_InRestore )
		return;

	if ( gEntList.ResetDeleteList() != 0 )
	{
		Msg( "ERROR: Entity delete queue not empty on level start!\n" );
	}

	for ( CBaseEntity *pClass = gEntList.FirstEnt(); pClass != NULL; pClass = gEntList.NextEnt(pClass) )
	{
		if ( pClass && !pClass->IsDormant() )
		{
			CEngineCacheCriticalSection engineCacheCriticalSection( engineCache );

			BeginCheckChainedActivate();
			pClass->Activate();
			
			// We don't care if it finished activating if it decided to remove itself.
			EndCheckChainedActivate( !( pClass->GetEFlags() & EFL_KILLME ) ); 
		}
	}

	IGameSystem::LevelInitPostEntityAllSystems();
	// No more precaching after PostEntityAllSystems!!!
	CBaseEntity::SetAllowPrecache( false );

	// only display the think limit when the game is run with "developer" mode set
	if ( !g_pDeveloper->GetInt() )
	{
		think_limit.SetValue( 0 );
	}

	// load the Navigation Mesh for this map
	TheNavMesh->Load();

#ifdef CSTRIKE_DLL // BOTPORT: TODO: move these ifdefs out
	TheBots->ServerActivate();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Called at the start of every game frame
//-----------------------------------------------------------------------------
ConVar  trace_report( "trace_report", "0" );

void CServerGameDLL::GameFrame( bool simulating )
{
	VPROF( "CServerGameDLL::GameFrame" );

	// Don't run frames until fully restored
	if ( g_InRestore )
		return;

	static bool skipframe = false;

	// If server is skipping frames, don't run simulation this time through
	if ( skipframe )
	{
		skipframe = false;
		return;
	}

	float oldframetime = gpGlobals->frametime;
	if ( CBaseEntity::IsSimulatingOnAlternateTicks() )
	{
		skipframe = true;
		// If we're skipping frames, then the frametime is 2x the normal tick
		gpGlobals->frametime *= 2.0f;
	}

#ifdef _DEBUG
	// For profiling.. let them enable/disable the networkvar manual mode stuff.
	g_bUseNetworkVars = s_UseNetworkVars.GetBool();
#endif

	extern void GameStartFrame( void );
	extern void ServiceEventQueue( void );
	extern void Physics_RunThinkFunctions( bool simulating );

	// Delete anything that was marked for deletion
	//  outside of server frameloop (e.g., in response to concommand)
	gEntList.CleanupDeleteList();

	IGameSystem::FrameUpdatePreEntityThinkAllSystems();
	GameStartFrame();

	TheNavMesh->Update();

#ifdef CSTRIKE_DLL // BOTPORT: TODO: move these ifdefs out
	TheBots->StartFrame();
#endif

	Physics_RunThinkFunctions( simulating );
	
	IGameSystem::FrameUpdatePostEntityThinkAllSystems();

	// UNDONE: Make these systems IGameSystems and move these calls into FrameUpdatePostEntityThink()
	// service event queue, firing off any actions whos time has come
	ServiceEventQueue();

	// free all ents marked in think functions
	gEntList.CleanupDeleteList();

	// FIXME:  Should this only occur on the final tick?
	UpdateAllClientData();

	if ( g_pGameRules )
	{
		g_pGameRules->EndGameFrame();
	}

	if ( trace_report.GetBool() )
	{
		int total = 0, totals[3];
		for ( int i = 0; i < 3; i++ )
		{
			totals[i] = enginetrace->GetStatByIndex( i, true );
			if ( totals[i] > 0 )
			{
				total += totals[i];
			}
		}

		if ( total )
		{
			Msg("Trace: %d, contents %d, enumerate %d\n", totals[0], totals[1], totals[2] );
		}
	}

	// Any entities that detect network state changes on a timer do it here.
	g_NetworkPropertyEventMgr.FireEvents();

	gpGlobals->frametime = oldframetime;
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame even if not ticking
// Input  : simulating - 
//-----------------------------------------------------------------------------
void CServerGameDLL::PreClientUpdate( bool simulating )
{
	if ( !simulating )
		return;

	/*
	if (game_speeds.GetInt())
	{
		DrawMeasuredSections();
	}
	*/

//#ifdef _DEBUG  - allow this in release for now
	DrawAllDebugOverlays();
//#endif
	
	IGameSystem::PreClientUpdateAllSystems();

	if ( sv_showhitboxes.GetInt() == -1 )
		return;

	if ( sv_showhitboxes.GetInt() == 0 )
	{
		// assume it's text
		CBaseEntity *pEntity = NULL;

		while (1)
		{
			pEntity = gEntList.FindEntityByName( pEntity, sv_showhitboxes.GetString(), NULL );
			if ( !pEntity )
				break;

			CBaseAnimating *anim = dynamic_cast< CBaseAnimating * >( pEntity );

			if (anim)
			{
				anim->DrawServerHitboxes();
			}
		}
		return;
	}

	CBaseAnimating *anim = dynamic_cast< CBaseAnimating * >( CBaseEntity::Instance( engine->PEntityOfEntIndex( sv_showhitboxes.GetInt() ) ) );
	if ( !anim )
		return;

	anim->DrawServerHitboxes();
}

// Called when a level is shutdown (including changing levels)
void CServerGameDLL::LevelShutdown( void )
{
	IGameSystem::LevelShutdownPreEntityAllSystems();

	// YWB:
	// This entity pointer is going away now and is corrupting memory on level transitions/restarts
	CSoundEnt::ShutdownSoundEnt();

	gEntList.Clear();

	IGameSystem::LevelShutdownPostEntityAllSystems();

	// In case we quit out during initial load
	CBaseEntity::SetAllowPrecache( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : ServerClass*
//-----------------------------------------------------------------------------
ServerClass* CServerGameDLL::GetAllServerClasses()
{
	return g_pServerClassHead;
}


const char *CServerGameDLL::GetGameDescription( void )
{
	return ::GetGameDescription();
}

void CServerGameDLL::CreateNetworkStringTables( void )
{
	// Create any shared string tables here (and only here!)
	// E.g.:  xxx = networkstringtable->CreateStringTable( "SceneStrings", 512 );
	g_pStringTableEffectDispatch = networkstringtable->CreateStringTable( "EffectDispatch", MAX_EFFECT_DISPATCH_STRINGS );
	g_pStringTableVguiScreen = networkstringtable->CreateStringTable( "VguiScreen", MAX_VGUI_SCREEN_STRINGS );
	g_pStringTableMaterials = networkstringtable->CreateStringTable( "Materials", MAX_MATERIAL_STRINGS );
	g_pStringTableInfoPanel = networkstringtable->CreateStringTable( "InfoPanel", MAX_INFOPANEL_STRINGS );

	Assert( g_pStringTableEffectDispatch &&
			g_pStringTableVguiScreen &&
			g_pStringTableMaterials &&
			g_pStringTableInfoPanel	);

	// Need this so we have the error material always handy
	PrecacheMaterial( "debug/debugempty" );
	Assert( GetMaterialIndex( "debug/debugempty" ) == 0 );

	CreateNetworkStringTables_GameRules();

	// Set up save/load utilities for string tables
	g_VguiScreenStringOps.Init( g_pStringTableVguiScreen );
}

CSaveRestoreData *CServerGameDLL::SaveInit( int size )
{
	return ::SaveInit(size);
}

//-----------------------------------------------------------------------------
// Purpose: Saves data from a struct into a saverestore object, to be saved to disk
// Input  : *pSaveData - the saverestore object
//			char *pname - the name of the data to write
//			*pBaseData - the struct into which the data is to be read
//			*pFields - pointer to an array of data field descriptions
//			fieldCount - the size of the array (number of field descriptions)
//-----------------------------------------------------------------------------
void CServerGameDLL::SaveWriteFields( CSaveRestoreData *pSaveData, const char *pname, void *pBaseData, datamap_t *pMap, typedescription_t *pFields, int fieldCount )
{
	CSave saveHelper( pSaveData );
	saveHelper.WriteFields( pname, pBaseData, pMap, pFields, fieldCount );
}


//-----------------------------------------------------------------------------
// Purpose: Reads data from a save/restore block into a structure
// Input  : *pSaveData - the saverestore object
//			char *pname - the name of the data to extract from
//			*pBaseData - the struct into which the data is to be restored
//			*pFields - pointer to an array of data field descriptions
//			fieldCount - the size of the array (number of field descriptions)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

void CServerGameDLL::SaveReadFields( CSaveRestoreData *pSaveData, const char *pname, void *pBaseData, datamap_t *pMap, typedescription_t *pFields, int fieldCount )
{
	CRestore restoreHelper( pSaveData );
	restoreHelper.ReadFields( pname, pBaseData, pMap, pFields, fieldCount );
}

//-----------------------------------------------------------------------------

void CServerGameDLL::SaveGlobalState( CSaveRestoreData *s )
{
	::SaveGlobalState(s);
}

void CServerGameDLL::RestoreGlobalState(CSaveRestoreData *s)
{
	::RestoreGlobalState(s);
}

void CServerGameDLL::Save( CSaveRestoreData *s )
{
	CSave saveHelper( s );
	g_pGameSaveRestoreBlockSet->Save( &saveHelper );
}

void CServerGameDLL::Restore( CSaveRestoreData *s, bool b)
{
	CRestore restore(s);
	g_pGameSaveRestoreBlockSet->Restore( &restore, b );
	g_pGameSaveRestoreBlockSet->PostRestore();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_type - 
//			*name - 
//			size - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------

bool CServerGameDLL::GetUserMessageInfo( int msg_type, char *name, int maxnamelength, int& size )
{
	if ( !usermessages->IsValidIndex( msg_type ) )
		return false;

	Q_strncpy( name, usermessages->GetUserMessageName( msg_type ), maxnamelength );
	size = usermessages->GetUserMessageSize( msg_type );
	return true;
}

CStandardSendProxies* CServerGameDLL::GetStandardSendProxies()
{
	return &g_StandardSendProxies;
}

int	CServerGameDLL::CreateEntityTransitionList( CSaveRestoreData *s, int a)
{
	CRestore restoreHelper( s );
	// save off file base
	int base = restoreHelper.GetReadPos();

	int movedCount = ::CreateEntityTransitionList(s, a);
	if ( movedCount )
	{
		g_pGameSaveRestoreBlockSet->CallBlockHandlerRestore( GetPhysSaveRestoreBlockHandler(), base, &restoreHelper, false );
		g_pGameSaveRestoreBlockSet->CallBlockHandlerRestore( GetAISaveRestoreBlockHandler(), base, &restoreHelper, false );
	}

	GetPhysSaveRestoreBlockHandler()->PostRestore();
	GetAISaveRestoreBlockHandler()->PostRestore();

	return movedCount;
}

void CServerGameDLL::PreSave( CSaveRestoreData *s )
{
	g_pGameSaveRestoreBlockSet->PreSave( s );
}

#include "client_textmessage.h"

// This little hack lets me marry BSP names to messages in titles.txt
typedef struct
{
	char *pBSPName;
	char *pTitleName;
} TITLECOMMENT;

// this list gets searched for the first partial match, so some are out of order
static TITLECOMMENT gTitleComments[] =
{
#ifdef HL1_DLL
	{ "t0a0", "#T0A0TITLE" },
	{ "c0a0", "#HL1_Chapter1_Title" },
	{ "c1a0", "#HL1_Chapter2_Title" },
	{ "c1a1", "#HL1_Chapter3_Title" },
	{ "c1a2", "#HL1_Chapter4_Title" },
	{ "c1a3", "#HL1_Chapter5_Title" },
	{ "c1a4", "#HL1_Chapter6_Title" },
	{ "c2a1", "#HL1_Chapter7_Title" },
	{ "c2a2", "#HL1_Chapter8_Title" },
	{ "c2a3", "#HL1_Chapter9_Title" },
	{ "c2a4d", "#HL1_Chapter11_Title" },	// These must appear before "C2A4" so all other map names starting with C2A4 get that title
	{ "c2a4e", "#HL1_Chapter11_Title" },
	{ "c2a4f", "#HL1_Chapter11_Title" },
	{ "c2a4g", "#HL1_Chapter11_Title" },
	{ "c2a4", "#HL1_Chapter10_Title" },
	{ "c2a5", "#HL1_Chapter12_Title" },
	{ "c3a1", "#HL1_Chapter13_Title" },
	{ "c3a2", "#HL1_Chapter14_Title" },
	{ "c4a1a", "#HL1_Chapter17_Title"  },	// Order is important, see above
	{ "c4a1b", "#HL1_Chapter17_Title"  },
	{ "c4a1c", "#HL1_Chapter17_Title"  },
	{ "c4a1d", "#HL1_Chapter17_Title"  },
	{ "c4a1e", "#HL1_Chapter17_Title"  },
	{ "c4a1", "#HL1_Chapter15_Title" },
	{ "c4a2", "#HL1_Chapter16_Title"  },
	{ "c4a3", "#HL1_Chapter18_Title"  },
	{ "c5a1", "#HL1_Chapter19_Title"  },
#else
	{ "intro", "#HL2_Chapter1_Title" },

	{ "d1_trainstation_05", "#HL2_Chapter2_Title" },
	{ "d1_trainstation_06", "#HL2_Chapter2_Title" },
	
	{ "d1_trainstation_", "#HL2_Chapter1_Title" },

	{ "d1_canals_05a", "#HL2_Chapter4_Title" },
	{ "d1_canals_05b", "#HL2_Chapter4_Title" },
	{ "d1_canals_06", "#HL2_Chapter4_Title" },
	{ "d1_canals_07", "#HL2_Chapter4_Title" },
	{ "d1_canals_08", "#HL2_Chapter4_Title" },
	{ "d1_canals_09", "#HL2_Chapter4_Title" },
	{ "d1_canals_1", "#HL2_Chapter4_Title" },
	
	{ "d1_canals_0", "#HL2_Chapter3_Title" },

	{ "d1_eli_", "#HL2_Chapter5_Title" },

	{ "d1_town_", "#HL2_Chapter6_Title" },

	{ "d2_coast_09", "#HL2_Chapter8_Title" },
	{ "d2_coast_1", "#HL2_Chapter8_Title" },
	{ "d2_prison_01", "#HL2_Chapter8_Title" },

	{ "d2_coast_", "#HL2_Chapter7_Title" },

	{ "d2_prison_06", "#HL2_Chapter9a_Title" },
	{ "d2_prison_07", "#HL2_Chapter9a_Title" },
	{ "d2_prison_08", "#HL2_Chapter9a_Title" },
	{ "d3_c17_01", "#HL2_Chapter9a_Title" },

	{ "d2_prison_", "#HL2_Chapter9_Title" },

	{ "d3_c17_09", "#HL2_Chapter11_Title" },
	{ "d3_c17_1", "#HL2_Chapter11_Title" },

	{ "d3_c17_", "#HL2_Chapter10_Title" },

	{ "d3_citadel_", "#HL2_Chapter12_Title" },

	{ "d3_breen_", "#HL2_Chapter13_Title" },
#endif
};

void CServerGameDLL::GetSaveComment( char *text, int maxlength )
{
	char comment[64];
	const char	*pName;
	int		i;

	char const *mapname = STRING( gpGlobals->mapname );

	pName = NULL;

	// Try to find a matching title comment for this mapname
	for ( i = 0; i < ARRAYSIZE(gTitleComments) && !pName; i++ )
	{
		if ( !Q_strnicmp( mapname, gTitleComments[i].pBSPName, strlen(gTitleComments[i].pBSPName) ) )
		{
			// found one
			int j;

			// Got a message, post-process it to be save name friendly
			Q_strncpy( comment, gTitleComments[i].pTitleName, sizeof( comment ) );
			pName = comment;
			j = 0;
			// Strip out CRs
			while ( j < 64 && comment[j] )
			{
				if ( comment[j] == '\n' || comment[j] == '\r' )
					comment[j] = 0;
				else
					j++;
			}

			break;
		}
	}
	
	// If we didn't get one, use the designer's map name, or the BSP name itself
	if ( !pName )
	{
		pName = mapname;
	}

	int minutes = (int)( gpGlobals->curtime / 60.0 );
	int seconds = (int)fmod( gpGlobals->curtime, 60.0f );

	// Wow, this guy/gal must suck...!
	if ( minutes >= 1000 )
	{
		minutes = 999;
		seconds = 59;
	}

	// add the elapsed time at the end of the comment, for the ui to parse out
	Q_snprintf( text, maxlength, "%-64.64s %03d:%02d", pName, minutes, seconds );
}

void CServerGameDLL::WriteSaveHeaders( CSaveRestoreData *s )
{
	CSave saveHelper( s );
	g_pGameSaveRestoreBlockSet->WriteSaveHeaders( &saveHelper );
	g_pGameSaveRestoreBlockSet->PostSave();
}

void CServerGameDLL::ReadRestoreHeaders( CSaveRestoreData *s )
{
	CRestore restoreHelper( s );
	g_pGameSaveRestoreBlockSet->PreRestore();
	g_pGameSaveRestoreBlockSet->ReadRestoreHeaders( &restoreHelper );
}


//-----------------------------------------------------------------------------
// Purpose: Called during a transition, to build a map adjacency list
//-----------------------------------------------------------------------------
void CServerGameDLL::BuildAdjacentMapList( void )
{
	// retrieve the pointer to the save data
	CSaveRestoreData *pSaveData = gpGlobals->pSaveData;

	if ( pSaveData )
		pSaveData->levelInfo.connectionCount = BuildChangeList( pSaveData->levelInfo.levelList, MAX_LEVEL_CONNECTIONS );
}

void CServerGameDLL::LoadMessageOfTheDay()
{
	char data[2048];

	int length = filesystem->Size( "motd.txt", "GAME" );

	if ( length <= 0 || length >= (sizeof(data)-1) )
	{
		DevMsg("Invalid file size for motd.txt\n" );
		return;
	}

	FileHandle_t hFile = filesystem->Open( "motd.txt", "rb", "GAME" );

	if ( hFile == FILESYSTEM_INVALID_HANDLE )
		return;

	filesystem->Read( data, length, hFile );
	filesystem->Close( hFile );

	data[length] = 0;

	g_pStringTableInfoPanel->AddString( "motd", length+1, data );
}

// keeps track of which chapters the user has unlocked
ConVar sv_unlockedchapters( "sv_unlockedchapters", "1", FCVAR_ARCHIVE );

//-----------------------------------------------------------------------------
// Purpose: Updates which chapters are unlocked
//-----------------------------------------------------------------------------
void UpdateChapterRestrictions( const char *mapname )
{
	// look at the chapter for this map
	char chapterTitle[64];
	chapterTitle[0] = 0;
	for ( int i = 0; i < ARRAYSIZE(gTitleComments); i++ )
	{
		if ( !Q_strnicmp( mapname, gTitleComments[i].pBSPName, strlen(gTitleComments[i].pBSPName) ) )
		{
			// found
			Q_strncpy( chapterTitle, gTitleComments[i].pTitleName, sizeof( chapterTitle ) );
			int j = 0;
			while ( j < 64 && chapterTitle[j] )
			{
				if ( chapterTitle[j] == '\n' || chapterTitle[j] == '\r' )
					chapterTitle[j] = 0;
				else
					j++;
			}

			break;
		}
	}

	if ( !chapterTitle[0] )
		return;

	// make sure the specified chapter title is unlocked
	strlwr( chapterTitle );

	const char *pGameDir = CommandLine()->ParmValue( "-game", "hl2" );

	char chapterNumberPrefix[64];
	Q_snprintf(chapterNumberPrefix, sizeof(chapterNumberPrefix), "#%s_chapter", pGameDir);

	const char *newChapterNumber = strstr( chapterTitle, chapterNumberPrefix );
	if ( newChapterNumber )
	{
		// cut off the front
		newChapterNumber += strlen( chapterNumberPrefix );
		char newChapter[32];
		Q_strncpy( newChapter, newChapterNumber, sizeof(newChapter) );

		// cut off the end
		char *end = strstr( newChapter, "_title" );
		if ( end )
		{
			*end = 0;
		}

		// ok we have the string, see if it's newer
		const char *unlockedChapter = sv_unlockedchapters.GetString();
		if ( atoi(unlockedChapter) < atoi(newChapter)
			|| (atoi(unlockedChapter) == atoi(newChapter) && stricmp(unlockedChapter, newChapter) < 0) )
		{
			// ok we're at a higher chapter, unlock
			sv_unlockedchapters.SetValue( newChapter );
		}
	}
}

//-----------------------------------------------------------------------------
// Precaches a vgui screen overlay material
//-----------------------------------------------------------------------------
void PrecacheMaterial( const char *pMaterialName )
{
	Assert( pMaterialName && pMaterialName[0] );
	g_pStringTableMaterials->AddString( pMaterialName );
}


//-----------------------------------------------------------------------------
// Converts a previously precached material into an index
//-----------------------------------------------------------------------------
int GetMaterialIndex( const char *pMaterialName )
{
	if (pMaterialName)
	{
		int nIndex = g_pStringTableMaterials->FindStringIndex( pMaterialName );
		
		if (nIndex != INVALID_STRING_INDEX )
		{
			return nIndex;
		}
		else
		{
			DevMsg("Warning! GetMaterialIndex: couldn't find material %s\n ", pMaterialName );
			return 0;
		}
	}

	// This is the invalid string index
	return 0;
}

//-----------------------------------------------------------------------------
// Converts a previously precached material index into a string
//-----------------------------------------------------------------------------
const char *GetMaterialNameFromIndex( int nMaterialIndex )
{
	return g_pStringTableMaterials->GetString( nMaterialIndex );
}


class CServerGameEnts : public IServerGameEnts
{
public:
	virtual void			SetDebugEdictBase(edict_t *base);
	virtual void			MarkEntitiesAsTouching( edict_t *e1, edict_t *e2 );
	virtual void			FreeContainingEntity( edict_t * ); 
	virtual edict_t*		BaseEntityToEdict( CBaseEntity *pEnt );
	virtual CBaseEntity*	EdictToBaseEntity( edict_t *pEdict );
	virtual void			CheckTransmit( CCheckTransmitInfo *pInfo, const unsigned short *pEdictIndices, int nEdicts );
};
EXPOSE_SINGLE_INTERFACE(CServerGameEnts, IServerGameEnts, INTERFACEVERSION_SERVERGAMEENTS);

void CServerGameEnts::SetDebugEdictBase(edict_t *base)
{
	g_pDebugEdictBase = base;
}

//-----------------------------------------------------------------------------
// Purpose: Marks entities as touching
// Input  : *e1 - 
//			*e2 - 
//-----------------------------------------------------------------------------
void CServerGameEnts::MarkEntitiesAsTouching( edict_t *e1, edict_t *e2 )
{
	CBaseEntity *entity = GetContainingEntity( e1 );
	CBaseEntity *entityTouched = GetContainingEntity( e2 );
	if ( entity && entityTouched )
	{
		// HACKHACK: UNDONE: Pass in the trace here??!?!?
		trace_t tr;
		UTIL_ClearTrace( tr );
		tr.endpos = (entity->GetAbsOrigin() + entityTouched->GetAbsOrigin()) * 0.5;
		entity->PhysicsMarkEntitiesAsTouching( entityTouched, tr );
	}
}

void CServerGameEnts::FreeContainingEntity( edict_t *e )
{
	::FreeContainingEntity(e);
}

edict_t* CServerGameEnts::BaseEntityToEdict( CBaseEntity *pEnt )
{
	if ( pEnt )
		return pEnt->edict();
	else
		return NULL;
}

CBaseEntity* CServerGameEnts::EdictToBaseEntity( edict_t *pEdict )
{
	if ( pEdict )
		return CBaseEntity::Instance( pEdict );
	else
		return NULL;
}


/* Yuck.. ideally this would be in CServerNetworkProperty's header, but it requires CBaseEntity and
// inlining it gives a nice speedup.
inline void CServerNetworkProperty::CheckTransmit( CCheckTransmitInfo *pInfo )
{
	// If we have a transmit proxy, let it hook our ShouldTransmit return value.
	if ( m_pTransmitProxy )
	{
		nShouldTransmit = m_pTransmitProxy->ShouldTransmit( pInfo, nShouldTransmit );
	}

	if ( m_pOuter->ShouldTransmit( pInfo ) )
	{
		m_pOuter->SetTransmit( pInfo );
	}
} */


void CServerGameEnts::CheckTransmit( CCheckTransmitInfo *pInfo, const unsigned short *pEdictIndices, int nEdicts )
{
	// NOTE: for speed's sake, this assumes that all networkables are CBaseEntities and that the edict list
	// is consecutive in memory. If either of these things change, then this routine needs to change, but
	// ideally we won't be calling any virtual from this routine. This speedy routine was added as an
	// optimization which would be nice to keep.
	edict_t *pBaseEdict = engine->PEntityOfEntIndex( 0 );

	// get recipient player's skybox:
	CBaseEntity *pRecipientEntity = CBaseEntity::Instance( pInfo->m_pClientEnt );

	Assert( pRecipientEntity && pRecipientEntity->IsPlayer() );

	if ( !pRecipientEntity )
		return;
	
	CBasePlayer *pRecipientPlayer = static_cast<CBasePlayer*>( pRecipientEntity );

	const int skyBoxArea = pRecipientPlayer->m_Local.m_skybox3d.area;
	const bool bIsHLTV = pRecipientPlayer->IsHLTV();

	// m_pTransmitAlways must be set if HLTV client
	Assert( bIsHLTV == ( pInfo->m_pTransmitAlways != NULL) );

	// int dontSend = 0; int always = 0; int fullCheck = 0; int PVS = 0;

	for ( int i=0; i < nEdicts; i++ )
	{
		int iEdict = pEdictIndices[i];

		edict_t *pEdict = &pBaseEdict[iEdict];
		Assert( pEdict == engine->PEntityOfEntIndex( iEdict ) );
		int nFlags = pEdict->m_fStateFlags & (FL_EDICT_DONTSEND|FL_EDICT_ALWAYS|FL_EDICT_PVSCHECK);

		if ( nFlags & FL_EDICT_DONTSEND )
		{
			// entity needs no transmit
			continue;
		}
		
		if ( pInfo->m_pTransmitEdict->Get( iEdict ) )
		{
			// entity is already marked for sending
			continue;
		}

		if ( nFlags & FL_EDICT_ALWAYS )
		{
			// mark entity for sending
			pInfo->m_pTransmitEdict->Set( iEdict );

			if ( bIsHLTV )
			{
				pInfo->m_pTransmitAlways->Set( iEdict );
			}
			
			continue;
		}

		// get the Baseentity

		CBaseEntity *pEnt = (CBaseEntity*)pEdict->GetUnknown();
		Assert( dynamic_cast< CBaseEntity* >( pEdict->GetUnknown() ) == pEnt );

		if ( nFlags == FL_EDICT_FULLCHECK )
		{
			// do a full ShouldTransmit() check, may return FL_EDICT_CHECKPVS
			nFlags = pEnt->ShouldTransmit( pInfo );

			Assert( !(nFlags & FL_EDICT_FULLCHECK) );

			if ( nFlags == FL_EDICT_ALWAYS )
			{
				pEnt->SetTransmit( pInfo, true );
				continue;
			}	
		}

		if ( nFlags != FL_EDICT_PVSCHECK )
		{
			// dont send this entity
			continue;
		}

		CServerNetworkProperty *netProp = pEnt->NetworkProp();

		if ( bIsHLTV )
		{
			// for the HLTV we don't cull against PVS
			if ( netProp->AreaNum() == skyBoxArea )
			{
				pEnt->SetTransmit( pInfo, true );
			}
			else
			{
				pEnt->SetTransmit( pInfo, false );
			}
			continue;
		}

		// Always send entities in the player's 3d skybox.
		// Sidenote: call of AreaNum() ensures that PVS data is up to date for this entity
		if ( netProp->AreaNum() == skyBoxArea )
		{
			pEnt->SetTransmit( pInfo, true );
		}
		else if ( netProp->IsInPVS( pInfo ) )
		{
			// only send if entity is in PVS
			pEnt->SetTransmit( pInfo, false );
		}
	}

//	Msg("A:%i, N:%i, F: %i, P: %i\n", always, dontSend, fullCheck, PVS );
}


CServerGameClients g_ServerGameClients;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CServerGameClients, IServerGameClients, INTERFACEVERSION_SERVERGAMECLIENTS, g_ServerGameClients );


//-----------------------------------------------------------------------------
// Purpose: called when a player tries to connect to the server
// Input  : *pEdict - the new player
//			char *pszName - the players name
//			char *pszAddress - the IP address of the player
//			reject - output - fill in with the reason why
//			maxrejectlen -- sizeof output buffer
//			the player was not allowed to connect.
// Output : Returns TRUE if player is allowed to join, FALSE if connection is denied.
//-----------------------------------------------------------------------------
bool CServerGameClients::ClientConnect( edict_t *pEdict, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen )
{	
	return g_pGameRules->ClientConnected( pEdict, pszName, pszAddress, reject, maxrejectlen );
}

//-----------------------------------------------------------------------------
// Purpose: Called when a player is fully active (i.e. ready to receive messages)
// Input  : *pEntity - the player
//-----------------------------------------------------------------------------
void CServerGameClients::ClientActive( edict_t *pEdict, bool bLoadGame )
{
	::ClientActive( pEdict, bLoadGame );

	// If we just loaded from a save file, call OnRestore on valid entities
	EndRestoreEntities();
	SimThink_SortThinkList();
	// Tell the sound controller to check looping sounds
	CBasePlayer *pPlayer = ( CBasePlayer * )CBaseEntity::Instance( pEdict );
	CSoundEnvelopeController::GetController().CheckLoopingSoundsForPlayer( pPlayer );
	SceneManager_ClientActive( pPlayer );
}

//-----------------------------------------------------------------------------
// Purpose: called when a player disconnects from a server
// Input  : *pEdict - the player
//-----------------------------------------------------------------------------
void CServerGameClients::ClientDisconnect( edict_t *pEdict )
{
	extern bool	g_fGameOver;

	CBasePlayer *player = ( CBasePlayer * )CBaseEntity::Instance( pEdict );
	if ( player )
	{
		if ( !g_fGameOver )
		{
			player->SetMaxSpeed( 0.0f );

			CSound *pSound;
			pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( pEdict ) );
			{
				// since this client isn't around to think anymore, reset their sound. 
				if ( pSound )
				{
					pSound->Reset();
				}
			}

		// since the edict doesn't get deleted, fix it so it doesn't interfere.
			player->RemoveFlag( FL_AIMTARGET ); // don't attract autoaim
			player->AddFlag( FL_DONTTOUCH );	// stop it touching anything
			player->AddFlag( FL_NOTARGET );	// stop NPCs noticing it
			player->AddSolidFlags( FSOLID_NOT_SOLID );		// nonsolid

			if ( g_pGameRules )
			{
				g_pGameRules->ClientDisconnected( pEdict );
			}
		}

		// Make sure all Untouch()'s are called for this client leaving
		CBaseEntity::PhysicsRemoveTouchedList( player );
		CBaseEntity::PhysicsRemoveGroundList( player );

		// Make sure anything we "own" is simulated by the server from now on
		player->ClearPlayerSimulationList();
	}
}


void CServerGameClients::ClientPutInServer( edict_t *pEntity, const char *playername )
{
	if ( g_pClientPutInServerOverride )
		g_pClientPutInServerOverride( pEntity, playername );
	else
		::ClientPutInServer( pEntity, playername );
}

void CServerGameClients::ClientCommand( edict_t *pEntity )
{
	CBasePlayer *player = ToBasePlayer( GetContainingEntity( pEntity ) );
	::ClientCommand(player);
}

//-----------------------------------------------------------------------------
// Purpose: called after the player changes userinfo - gives dll a chance to modify 
//			it before it gets sent into the rest of the engine->
// Input  : *pEdict - the player
//			*infobuffer - their infobuffer
//-----------------------------------------------------------------------------
void CServerGameClients::ClientSettingsChanged( edict_t *pEdict )
{
	// Is the client spawned yet?
	if ( !pEdict->GetUnknown() )
		return;

	CBasePlayer *player = ( CBasePlayer * )CBaseEntity::Instance( pEdict );
	
	if ( !player )
		return;

#define QUICKGETCVARVALUE(v) (engine->GetClientConVarValue( player->entindex(), v ))

	// get network setting for prediction & lag compensation
	player->m_nUpdateRate = Q_atoi( QUICKGETCVARVALUE("cl_updaterate") );
	player->m_fLerpTime   = Q_atof( QUICKGETCVARVALUE("cl_interp") );

	bool usePrediction = Q_atoi( QUICKGETCVARVALUE("cl_predict")) != 0;

	player->m_bPredictWeapons  = usePrediction && (Q_atoi( QUICKGETCVARVALUE("cl_predictweapons")) != 0);
	player->m_bLagCompensation = usePrediction && (Q_atoi( QUICKGETCVARVALUE("cl_lagcompensation")) != 0);

#undef QUICKGETCVARVALUE

	g_pGameRules->ClientSettingsChanged( player );
}


//-----------------------------------------------------------------------------
// Purpose: A client can have a separate "view entity" indicating that his/her view should depend on the origin of that
//  view entity.  If that's the case, then pViewEntity will be non-NULL and will be used.  Otherwise, the current
//  entity's origin is used.  Either is offset by the m_vecViewOffset to get the eye position.
// From the eye position, we set up the PAS and PVS to use for filtering network messages to the client.  At this point, we could
//  override the actual PAS or PVS values, or use a different origin.
// NOTE:  Do not cache the values of pas and pvs, as they depend on reusable memory in the engine, they are only good for this one frame
// Input  : *pViewEntity - 
//			*pClient - 
//			**pvs - 
//			**pas - 
//-----------------------------------------------------------------------------
void CServerGameClients::ClientSetupVisibility( edict_t *pViewEntity, edict_t *pClient, unsigned char *pvs, int pvssize )
{
	Vector org;

	// Reset the PVS!!!
	engine->ResetPVS( pvs, pvssize );

	// Find the client's PVS
	CBaseEntity *pVE = NULL;
	if ( pViewEntity )
	{
		pVE = GetContainingEntity( pViewEntity );
		// If we have a viewentity, it overrides the player's origin
		if ( pVE )
		{
			org = pVE->EyePosition();
			engine->AddOriginToPVS( org );
		}
	}

	float fovDistanceAdjustFactor = 1;

	CBasePlayer *pPlayer = ( CBasePlayer * )GetContainingEntity( pClient );
	if ( pPlayer )
	{
		org = pPlayer->EyePosition();
		pPlayer->SetupVisibility( pVE, pvs, pvssize );
		UTIL_SetClientVisibilityPVS( pClient, pvs, pvssize );
		fovDistanceAdjustFactor = pPlayer->GetFOVDistanceAdjustFactor();
	}

	for( unsigned short i = g_AreaPortals.Head(); i != g_AreaPortals.InvalidIndex(); i = g_AreaPortals.Next(i) )
	{
		CFuncAreaPortalBase *pCur = g_AreaPortals[i];
		pCur->UpdateVisibility( org, fovDistanceAdjustFactor );
	}

	// Update the area bits that get sent to the client.
	pPlayer->m_Local.UpdateAreaBits( pPlayer );
}




//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *player - 
//			*buf - 
//			numcmds - 
//			totalcmds - 
//			dropped_packets - 
//			ignore - 
//			paused - 
// Output : float
//-----------------------------------------------------------------------------
float CServerGameClients::ProcessUsercmds( edict_t *player, bf_read *buf, int numcmds, int totalcmds,
	int dropped_packets, bool ignore, bool paused )
{
	int				i;
	CUserCmd		*from, *to;

	// We track last three command in case we drop some 
	//  packets but get them back.
	CUserCmd		cmds[ CMD_MAXBACKUP ];  

	CUserCmd		cmdNull;  // For delta compression
	
	Assert( numcmds >= 0 );
	Assert( ( totalcmds - numcmds ) >= 0 );

	CBasePlayer *pPlayer = NULL;
	CBaseEntity *pEnt = CBaseEntity::Instance(player);
	if ( pEnt && pEnt->IsPlayer() )
	{
		pPlayer = static_cast< CBasePlayer * >( pEnt );
	}
	// Too many commands?
	if ( totalcmds < 0 || totalcmds >= ( CMD_MAXBACKUP - 1 ) )
	{
		const char *name = "unknown";
		if ( pPlayer )
		{
			name = pPlayer->GetPlayerName();
		}

		Msg("CBasePlayer::ProcessUsercmds: too many cmds %i sent for player %s\n", totalcmds, name );
		// FIXME:  Need a way to drop the client from here
		//SV_DropClient ( host_client, false, "CMD_MAXBACKUP hit" );
		buf->SetOverflowFlag();
		return 0.0f;
	}

	// Initialize for reading delta compressed usercmds
	cmdNull.Reset();
	from = &cmdNull;
	for ( i = totalcmds - 1; i >= 0; i-- )
	{
		to = &cmds[ i ];
		ReadUsercmd( buf, to, from );
		from = to;
	}

	// Client not fully connected or server has gone inactive  or is paused, just ignore
	if ( ignore || !pPlayer )
	{
		return 0.0f;
	}

	pPlayer->ProcessUsercmds( cmds, numcmds, totalcmds, dropped_packets, paused );

	return TICK_INTERVAL;
}


void CServerGameClients::PostClientMessagesSent( void )
{
	VPROF("CServerGameClients::PostClient");
	gEntList.PostClientMessagesSent();
}

// Sets the client index for the client who typed the command into his/her console
void CServerGameClients::SetCommandClient( int index )
{
	g_nCommandClientIndex = index;
}

static ConVar replaydelay( "replaydelay", "0", 0 );

int	CServerGameClients::GetReplayDelay( edict_t *player )
{
	return replaydelay.GetInt();
}


//-----------------------------------------------------------------------------
// The client's userinfo data lump has changed
//-----------------------------------------------------------------------------
void CServerGameClients::ClientEarPosition( edict_t *pEdict, Vector *pEarOrigin )
{
	CBasePlayer *pPlayer = ( CBasePlayer * )CBaseEntity::Instance( pEdict );
	if (pPlayer)
	{
		*pEarOrigin = pPlayer->EarPosition();
	}
	else
	{
		// Shouldn't happen
		Assert(0);
		*pEarOrigin = vec3_origin;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *player - 
// Output : CPlayerState
//-----------------------------------------------------------------------------
CPlayerState *CServerGameClients::GetPlayerState( edict_t *player )
{
	// Is the client spawned yet?
	if ( !player || !player->GetUnknown() )
		return NULL;

	CBasePlayer *pBasePlayer = ( CBasePlayer * )CBaseEntity::Instance( player );
	if ( !pBasePlayer )
		return NULL;

	return &pBasePlayer->pl;
}

//-----------------------------------------------------------------------------
// Purpose: Anything this game .dll wants to add to the bug reporter text (e.g., the entity/model under the picker crosshair)
//  can be added here
// Input  : *buf - 
//			buflen - 
//-----------------------------------------------------------------------------
void CServerGameClients::GetBugReportInfo( char *buf, int buflen )
{
	buf[ 0 ] = 0;

	if ( gpGlobals->maxClients == 1 )
	{
		CBaseEntity *ent = FindPickerEntity( UTIL_PlayerByIndex(1) );
		if ( ent )
		{
			Q_snprintf( buf, buflen, "Picker %i/%s - ent %s model %s\n",
				ent->entindex(),
				ent->GetClassname(),
				STRING( ent->GetEntityName() ),
				ent->GetModelName() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static bf_write *g_pMsgBuffer = NULL;

void EntityMessageBegin( CBaseEntity * entity, bool reliable /*= false*/ ) 
{
	Assert( !g_pMsgBuffer );

	Assert ( entity );

	g_pMsgBuffer = engine->EntityMessageBegin( entity->entindex(), entity->GetServerClass(), reliable );
}

void UserMessageBegin( IRecipientFilter& filter, const char *messagename )
{
	Assert( !g_pMsgBuffer );

	Assert( messagename );

	int msg_type = usermessages->LookupUserMessage( messagename );
	
	if ( msg_type == -1 )
	{
		Error( "UserMessageBegin:  Unregistered message '%s'\n", messagename );
	}

	g_pMsgBuffer = engine->UserMessageBegin( &filter, msg_type );
}

void MessageEnd( void )
{
	Assert( g_pMsgBuffer );

	engine->MessageEnd();

	g_pMsgBuffer = NULL;
}

void MessageWriteByte( int iValue)
{
	if (!g_pMsgBuffer)
		Error( "WRITE_BYTE called with no active message\n" );

	g_pMsgBuffer->WriteByte( iValue );
}

void MessageWriteChar( int iValue)
{
	if (!g_pMsgBuffer)
		Error( "WRITE_CHAR called with no active message\n" );

	g_pMsgBuffer->WriteChar( iValue );
}

void MessageWriteShort( int iValue)
{
	if (!g_pMsgBuffer)
		Error( "WRITE_SHORT called with no active message\n" );

	g_pMsgBuffer->WriteShort( iValue );
}

void MessageWriteWord( int iValue )
{
	if (!g_pMsgBuffer)
		Error( "WRITE_WORD called with no active message\n" );

	g_pMsgBuffer->WriteWord( iValue );
}

void MessageWriteLong( int iValue)
{
	if (!g_pMsgBuffer)
		Error( "WriteLong called with no active message\n" );

	g_pMsgBuffer->WriteLong( iValue );
}

void MessageWriteFloat( float flValue)
{
	if (!g_pMsgBuffer)
		Error( "WriteFloat called with no active message\n" );

	g_pMsgBuffer->WriteFloat( flValue );
}

void MessageWriteAngle( float flValue)
{
	if (!g_pMsgBuffer)
		Error( "WriteAngle called with no active message\n" );

	g_pMsgBuffer->WriteBitAngle( flValue, 8 );
}

void MessageWriteCoord( float flValue)
{
	if (!g_pMsgBuffer)
		Error( "WriteCoord called with no active message\n" );

	g_pMsgBuffer->WriteBitCoord( flValue );
}

void MessageWriteVec3Coord( const Vector& rgflValue)
{
	if (!g_pMsgBuffer)
		Error( "WriteVec3Coord called with no active message\n" );

	g_pMsgBuffer->WriteBitVec3Coord( rgflValue );
}

void MessageWriteVec3Normal( const Vector& rgflValue)
{
	if (!g_pMsgBuffer)
		Error( "WriteVec3Normal called with no active message\n" );

	g_pMsgBuffer->WriteBitVec3Normal( rgflValue );
}

void MessageWriteAngles( const QAngle& rgflValue)
{
	if (!g_pMsgBuffer)
		Error( "WriteVec3Normal called with no active message\n" );

	g_pMsgBuffer->WriteBitAngles( rgflValue );
}

void MessageWriteString( const char *sz )
{
	if (!g_pMsgBuffer)
		Error( "WriteString called with no active message\n" );

	g_pMsgBuffer->WriteString( sz );
}

void MessageWriteEntity( int iValue)
{
	if (!g_pMsgBuffer)
		Error( "WriteEntity called with no active message\n" );

	g_pMsgBuffer->WriteShort( iValue );
}

// bitwise
void MessageWriteBool( bool bValue )
{
	if (!g_pMsgBuffer)
		Error( "WriteBool called with no active message\n" );

	g_pMsgBuffer->WriteOneBit( bValue ? 1 : 0 );
}

void MessageWriteUBitLong( unsigned int data, int numbits )
{
	if (!g_pMsgBuffer)
		Error( "WriteUBitLong called with no active message\n" );

	g_pMsgBuffer->WriteUBitLong( data, numbits );
}

void MessageWriteSBitLong( int data, int numbits )
{
	if (!g_pMsgBuffer)
		Error( "WriteSBitLong called with no active message\n" );

	g_pMsgBuffer->WriteSBitLong( data, numbits );
}

void MessageWriteBits( const void *pIn, int nBits )
{
	if (!g_pMsgBuffer)
		Error( "WriteBits called with no active message\n" );

	g_pMsgBuffer->WriteBits( pIn, nBits );
}


