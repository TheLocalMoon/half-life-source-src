//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// nav_area.cpp
// AI Navigation areas
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

#include "cbase.h"

#include "nav_mesh.h"
#include "nav_node.h"
#include "nav_pathfind.h"

#ifdef _WIN32
#pragma warning (disable:4701)				// disable warning that variable *may* not be initialized 
#endif

#define DrawLine( from, to, duration, red, green, blue )		NDebugOverlay::Line( from, to, red, green, blue, true, 0.1f )


extern void HintMessageToAllPlayers( const char *message );

unsigned int CNavArea::m_nextID = 1;
NavAreaList TheNavAreaList;

unsigned int CNavArea::m_masterMarker = 1;
CNavArea *CNavArea::m_openList = NULL;

bool CNavArea::m_isReset = false;

//--------------------------------------------------------------------------------------------------------------
/**
 * To keep constructors consistent
 */
void CNavArea::Initialize( void )
{
	m_marker = 0;
	m_parent = NULL;
	m_parentHow = GO_NORTH;
	m_attributeFlags = 0;
	m_place = 0;

	for ( int i=0; i<MAX_AREA_TEAMS; ++i )
	{
		m_danger[i] = 0.0f;
		m_dangerTimestamp[i] = 0.0f;

		m_clearedTimestamp[i] = 0.0f;
	}

	m_approachCount = 0;

	// set an ID for splitting and other interactive editing - loads will overwrite this
	m_id = m_nextID++;

	m_prevHash = NULL;
	m_nextHash = NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Constructor used during normal runtime.
 */
CNavArea::CNavArea( void )
{
	Initialize();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Assumes Z is flat
 */
CNavArea::CNavArea( const Vector *corner, const Vector *otherCorner )
{
	Initialize();

	if (corner->x < otherCorner->x)
	{
		m_extent.lo.x = corner->x;
		m_extent.hi.x = otherCorner->x;
	}
	else
	{
		m_extent.hi.x = corner->x;
		m_extent.lo.x = otherCorner->x;
	}

	if (corner->y < otherCorner->y)
	{
		m_extent.lo.y = corner->y;
		m_extent.hi.y = otherCorner->y;
	}
	else
	{
		m_extent.hi.y = corner->y;
		m_extent.lo.y = otherCorner->y;
	}

	m_extent.lo.z = corner->z;
	m_extent.hi.z = corner->z;

	m_center.x = (m_extent.lo.x + m_extent.hi.x)/2.0f;
	m_center.y = (m_extent.lo.y + m_extent.hi.y)/2.0f;
	m_center.z = (m_extent.lo.z + m_extent.hi.z)/2.0f;

	m_neZ = corner->z;
	m_swZ = otherCorner->z;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * 
 */
CNavArea::CNavArea( const Vector *nwCorner, const Vector *neCorner, const Vector *seCorner, const Vector *swCorner )
{
	Initialize();

	m_extent.lo = *nwCorner;
	m_extent.hi = *seCorner;

	m_center.x = (m_extent.lo.x + m_extent.hi.x)/2.0f;
	m_center.y = (m_extent.lo.y + m_extent.hi.y)/2.0f;
	m_center.z = (m_extent.lo.z + m_extent.hi.z)/2.0f;

	m_neZ = neCorner->z;
	m_swZ = swCorner->z;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Constructor used during generation phase.
 */
CNavArea::CNavArea( CNavNode *nwNode, CNavNode *neNode, CNavNode *seNode, CNavNode *swNode )
{
	Initialize();

	m_extent.lo = *nwNode->GetPosition();
	m_extent.hi = *seNode->GetPosition();

	m_center.x = (m_extent.lo.x + m_extent.hi.x)/2.0f;
	m_center.y = (m_extent.lo.y + m_extent.hi.y)/2.0f;
	m_center.z = (m_extent.lo.z + m_extent.hi.z)/2.0f;

	m_neZ = neNode->GetPosition()->z;
	m_swZ = swNode->GetPosition()->z;

	m_node[ NORTH_WEST ] = nwNode;
	m_node[ NORTH_EAST ] = neNode;
	m_node[ SOUTH_EAST ] = seNode;
	m_node[ SOUTH_WEST ] = swNode;

	// mark internal nodes as part of this area
	AssignNodes( this );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Destructor
 */
CNavArea::~CNavArea()
{
	// if we are resetting the system, don't bother cleaning up - all areas are being destroyed
	if (m_isReset)
		return;

	// tell the other areas we are going away
	FOR_EACH_LL( TheNavAreaList, it )
	{
		CNavArea *area = TheNavAreaList[ it ];

		if (area == this)
			continue;

		area->OnDestroyNotify( this );
	}

	// unhook from ladders
	for( int i=0; i<CNavLadder::NUM_LADDER_DIRECTIONS; ++i )
	{
		FOR_EACH_LL( m_ladder[i], it )
		{
			CNavLadder *ladder = m_ladder[ i ][ it ];

			ladder->OnDestroyNotify( this );
		}
	}

	// remove the area from the grid
	TheNavMesh->RemoveNavArea( this );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * This is invoked when an area is going away.
 * Remove any references we have to it.
 */
void CNavArea::OnDestroyNotify( CNavArea *dead )
{
	NavConnect con;
	con.area = dead;
	for( int d=0; d<NUM_DIRECTIONS; ++d )
		m_connect[ d ].FindAndRemove( con );

	m_overlapList.FindAndRemove( dead );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Connect this area to given area in given direction
 */
void CNavArea::ConnectTo( CNavArea *area, NavDirType dir )
{
	// check if already connected
	FOR_EACH_LL( m_connect[ dir ], it )
	{
		if (m_connect[ dir ][ it ].area == area)
			return;
	}

	NavConnect con;
	con.area = area;
	m_connect[ dir ].AddToTail( con );

	//static char *dirName[] = { "NORTH", "EAST", "SOUTH", "WEST" };
	//CONSOLE_ECHO( "  Connected area #%d to #%d, %s\n", m_id, area->m_id, dirName[ dir ] );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Disconnect this area from given area
 */
void CNavArea::Disconnect( CNavArea *area )
{
	NavConnect connect;
	connect.area = area;

	for( int dir = 0; dir<NUM_DIRECTIONS; dir++ )
		m_connect[ dir ].FindAndRemove( connect );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Recompute internal data once nodes have been adjusted during merge
 * Destroy adjArea.
 */
void CNavArea::FinishMerge( CNavArea *adjArea )
{
	// update extent
	m_extent.lo = *m_node[ NORTH_WEST ]->GetPosition();
	m_extent.hi = *m_node[ SOUTH_EAST ]->GetPosition();

	m_center.x = (m_extent.lo.x + m_extent.hi.x)/2.0f;
	m_center.y = (m_extent.lo.y + m_extent.hi.y)/2.0f;
	m_center.z = (m_extent.lo.z + m_extent.hi.z)/2.0f;

	m_neZ = m_node[ NORTH_EAST ]->GetPosition()->z;
	m_swZ = m_node[ SOUTH_WEST ]->GetPosition()->z;

	// reassign the adjacent area's internal nodes to the final area
	adjArea->AssignNodes( this );

	// merge adjacency links - we gain all the connections that adjArea had
	MergeAdjacentConnections( adjArea );

	// remove subsumed adjacent area
	TheNavAreaList.FindAndRemove( adjArea );
	delete adjArea;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * For merging with "adjArea" - pick up all of "adjArea"s connections
 */
void CNavArea::MergeAdjacentConnections( CNavArea *adjArea )
{
	// merge adjacency links - we gain all the connections that adjArea had
	int dir;
	for( dir = 0; dir<NUM_DIRECTIONS; dir++ )
	{
		FOR_EACH_LL( adjArea->m_connect[ dir ], it )
		{
			NavConnect connect = adjArea->m_connect[ dir ][ it ];

			if (connect.area != adjArea && connect.area != this)
				ConnectTo( connect.area, (NavDirType)dir );
		}
	}

	// remove any references from this area to the adjacent area, since it is now part of us
	for( dir = 0; dir<NUM_DIRECTIONS; dir++ )
	{
		NavConnect connect;
		connect.area = adjArea;

		m_connect[dir].FindAndRemove( connect );
	}

	// Change other references to adjArea to refer instead to us
	// We can't just replace existing connections, as several adjacent areas may have been merged into one,
	// resulting in a large area adjacent to all of them ending up with multiple redunandant connections
	// into the merged area, one for each of the adjacent subsumed smaller ones.
	// If an area has a connection to the merged area, we must remove all references to adjArea, and add
	// a single connection to us.
	FOR_EACH_LL( TheNavAreaList, it )
	{
		CNavArea *area = TheNavAreaList[ it ];

		if (area == this || area == adjArea)
			continue;

		for( dir = 0; dir<NUM_DIRECTIONS; dir++ )
		{
			// check if there are any references to adjArea in this direction
			bool connected = false;
			FOR_EACH_LL( area->m_connect[ dir ], cit )
			{
				NavConnect connect = area->m_connect[ dir ][ cit ];

				if (connect.area == adjArea)
				{
					connected = true;
					break;
				}
			}

			if (connected)
			{
				// remove all references to adjArea
				NavConnect connect;
				connect.area = adjArea;
				area->m_connect[dir].FindAndRemove( connect );

				// remove all references to the new area
				connect.area = this;
				area->m_connect[dir].FindAndRemove( connect );

				// add a single connection to the new area
				connect.area = this;
				area->m_connect[dir].AddToTail( connect );
			}
		}
	}

}

//--------------------------------------------------------------------------------------------------------------
/**
 * Assign internal nodes to the given area
 * NOTE: "internal" nodes do not include the east or south border nodes
 */
void CNavArea::AssignNodes( CNavArea *area )
{
	CNavNode *horizLast = m_node[ NORTH_EAST ];

	for( CNavNode *vertNode = m_node[ NORTH_WEST ]; vertNode != m_node[ SOUTH_WEST ]; vertNode = vertNode->GetConnectedNode( SOUTH ) )
	{
		for( CNavNode *horizNode = vertNode; horizNode != horizLast; horizNode = horizNode->GetConnectedNode( EAST ) )
		{
			horizNode->AssignArea( area );
		}

		horizLast = horizLast->GetConnectedNode( SOUTH );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Split this area into two areas at the given edge.
 * Preserve all adjacency connections.
 * NOTE: This does not update node connections, only areas.
 */
bool CNavArea::SplitEdit( bool splitAlongX, float splitEdge, CNavArea **outAlpha, CNavArea **outBeta )
{
	CNavArea *alpha = NULL;
	CNavArea *beta = NULL;

	if (splitAlongX)
	{
		// +-----+->X
		// |  A  |
		// +-----+
		// |  B  |
		// +-----+
		// |
		// Y

		// don't do split if at edge of area
		if (splitEdge <= m_extent.lo.y + 1.0f)
			return false;

		if (splitEdge >= m_extent.hi.y - 1.0f)
			return false;

		alpha = new CNavArea;
		alpha->m_extent.lo = m_extent.lo;

		alpha->m_extent.hi.x = m_extent.hi.x;
		alpha->m_extent.hi.y = splitEdge;
		alpha->m_extent.hi.z = GetZ( &alpha->m_extent.hi );

		beta = new CNavArea;
		beta->m_extent.lo.x = m_extent.lo.x;
		beta->m_extent.lo.y = splitEdge;
		beta->m_extent.lo.z = GetZ( &beta->m_extent.lo );

		beta->m_extent.hi = m_extent.hi;

		alpha->ConnectTo( beta, SOUTH );
		beta->ConnectTo( alpha, NORTH );

		FinishSplitEdit( alpha, SOUTH );
		FinishSplitEdit( beta, NORTH );
	}
	else
	{
		// +--+--+->X
		// |  |  |
		// | A|B |
		// |  |  |
		// +--+--+
		// |
		// Y

		// don't do split if at edge of area
		if (splitEdge <= m_extent.lo.x + 1.0f)
			return false;

		if (splitEdge >= m_extent.hi.x - 1.0f)
			return false;

		alpha = new CNavArea;
		alpha->m_extent.lo = m_extent.lo;

		alpha->m_extent.hi.x = splitEdge;
		alpha->m_extent.hi.y = m_extent.hi.y;
		alpha->m_extent.hi.z = GetZ( &alpha->m_extent.hi );

		beta = new CNavArea;
		beta->m_extent.lo.x = splitEdge;
		beta->m_extent.lo.y = m_extent.lo.y;
		beta->m_extent.lo.z = GetZ( &beta->m_extent.lo );

		beta->m_extent.hi = m_extent.hi;

		alpha->ConnectTo( beta, EAST );
		beta->ConnectTo( alpha, WEST );

		FinishSplitEdit( alpha, EAST );
		FinishSplitEdit( beta, WEST );
	}

	// new areas inherit attributes from original area
	alpha->SetAttributes( GetAttributes() );
	beta->SetAttributes( GetAttributes() );

	// new areas inherit place from original area
	alpha->SetPlace( GetPlace() );
	beta->SetPlace( GetPlace() );

	// return new areas
	if (outAlpha)
		*outAlpha = alpha;

	if (outBeta)
		*outBeta = beta;

	// remove original area
	TheNavAreaList.FindAndRemove( this );
	delete this;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if given area is connected in given direction
 * if dir == NUM_DIRECTIONS, check all directions (direction is unknown)
 * @todo Formalize "asymmetric" flag on connections
 */
bool CNavArea::IsConnected( const CNavArea *area, NavDirType dir ) const
{
	// we are connected to ourself
	if (area == this)
		return true;

	if (dir == NUM_DIRECTIONS)
	{
		// search all directions
		for( int d=0; d<NUM_DIRECTIONS; ++d )
		{
			FOR_EACH_LL( m_connect[ d ], it )
			{
				if (area == m_connect[ d ][ it ].area)
					return true;
			}
		}

		// check ladder connections
		FOR_EACH_LL( m_ladder[ CNavLadder::LADDER_UP ], it )
		{
			CNavLadder *ladder = m_ladder[ CNavLadder::LADDER_UP ][ it ];

			if (ladder->m_topBehindArea == area ||
				ladder->m_topForwardArea == area ||
				ladder->m_topLeftArea == area ||
				ladder->m_topRightArea == area)
				return true;
		}

		FOR_EACH_LL( m_ladder[ CNavLadder::LADDER_DOWN ], dit )
		{
			CNavLadder *ladder = m_ladder[ CNavLadder::LADDER_DOWN ][ dit ];

			if (ladder->m_bottomArea == area)
				return true;
		}
	}
	else
	{
		// check specific direction
		FOR_EACH_LL( m_connect[ dir ], it )
		{
			if (area == m_connect[ dir ][ it ].area)
				return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute change in height from this area to given area
 * @todo This is approximate for now
 */
float CNavArea::ComputeHeightChange( const CNavArea *area )
{
	float ourZ = GetZ( GetCenter() );
	float areaZ = area->GetZ( area->GetCenter() );

	return areaZ - ourZ;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Given the portion of the original area, update its internal data
 * The "ignoreEdge" direction defines the side of the original area that the new area does not include
 */
void CNavArea::FinishSplitEdit( CNavArea *newArea, NavDirType ignoreEdge )
{
	newArea->m_center.x = (newArea->m_extent.lo.x + newArea->m_extent.hi.x)/2.0f;
	newArea->m_center.y = (newArea->m_extent.lo.y + newArea->m_extent.hi.y)/2.0f;
	newArea->m_center.z = (newArea->m_extent.lo.z + newArea->m_extent.hi.z)/2.0f;

	newArea->m_neZ = GetZ( newArea->m_extent.hi.x, newArea->m_extent.lo.y );
	newArea->m_swZ = GetZ( newArea->m_extent.lo.x, newArea->m_extent.hi.y );

	// connect to adjacent areas
	for( int d=0; d<NUM_DIRECTIONS; ++d )
	{
		if (d == ignoreEdge)
			continue;

		int count = GetAdjacentCount( (NavDirType)d );

		for( int a=0; a<count; ++a )
		{
			CNavArea *adj = GetAdjacentArea( (NavDirType)d, a );

			switch( d )
			{
				case NORTH:
				case SOUTH:
					if (newArea->IsOverlappingX( adj ))
					{
						newArea->ConnectTo( adj, (NavDirType)d );			

						// add reciprocal connection if needed
						if (adj->IsConnected( this, OppositeDirection( (NavDirType)d )))
							adj->ConnectTo( newArea, OppositeDirection( (NavDirType)d ) );
					}
					break;

				case EAST:
				case WEST:
					if (newArea->IsOverlappingY( adj ))
					{
						newArea->ConnectTo( adj, (NavDirType)d );			

						// add reciprocal connection if needed
						if (adj->IsConnected( this, OppositeDirection( (NavDirType)d )))
							adj->ConnectTo( newArea, OppositeDirection( (NavDirType)d ) );
					}
					break;
			}
		}
	}

	TheNavAreaList.AddToTail( newArea );
	TheNavMesh->AddNavArea( newArea );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Create a new area between this area and given area 
 */
bool CNavArea::SpliceEdit( CNavArea *other )
{
	CNavArea *newArea = NULL;
	Vector nw, ne, se, sw;

	if (m_extent.lo.x > other->m_extent.hi.x)
	{
		// 'this' is east of 'other'
		float top = max( m_extent.lo.y, other->m_extent.lo.y );
		float bottom = min( m_extent.hi.y, other->m_extent.hi.y );

		nw.x = other->m_extent.hi.x;
		nw.y = top;
		nw.z = other->GetZ( &nw );

		se.x = m_extent.lo.x;
		se.y = bottom;
		se.z = GetZ( &se );

		ne.x = se.x;
		ne.y = nw.y;
		ne.z = GetZ( &ne );

		sw.x = nw.x;
		sw.y = se.y;
		sw.z = other->GetZ( &sw );

		newArea = new CNavArea( &nw, &ne, &se, &sw );

		this->ConnectTo( newArea, WEST );
		newArea->ConnectTo( this, EAST );

		other->ConnectTo( newArea, EAST );
		newArea->ConnectTo( other, WEST );
	}
	else if (m_extent.hi.x < other->m_extent.lo.x)
	{
		// 'this' is west of 'other'
		float top = max( m_extent.lo.y, other->m_extent.lo.y );
		float bottom = min( m_extent.hi.y, other->m_extent.hi.y );

		nw.x = m_extent.hi.x;
		nw.y = top;
		nw.z = GetZ( &nw );

		se.x = other->m_extent.lo.x;
		se.y = bottom;
		se.z = other->GetZ( &se );

		ne.x = se.x;
		ne.y = nw.y;
		ne.z = other->GetZ( &ne );

		sw.x = nw.x;
		sw.y = se.y;
		sw.z = GetZ( &sw );

		newArea = new CNavArea( &nw, &ne, &se, &sw );

		this->ConnectTo( newArea, EAST );
		newArea->ConnectTo( this, WEST );

		other->ConnectTo( newArea, WEST );
		newArea->ConnectTo( other, EAST );
	}
	else	// 'this' overlaps in X
	{
		if (m_extent.lo.y > other->m_extent.hi.y)
		{
			// 'this' is south of 'other'
			float left = max( m_extent.lo.x, other->m_extent.lo.x );
			float right = min( m_extent.hi.x, other->m_extent.hi.x );

			nw.x = left;
			nw.y = other->m_extent.hi.y;
			nw.z = other->GetZ( &nw );

			se.x = right;
			se.y = m_extent.lo.y;
			se.z = GetZ( &se );

			ne.x = se.x;
			ne.y = nw.y;
			ne.z = other->GetZ( &ne );

			sw.x = nw.x;
			sw.y = se.y;
			sw.z = GetZ( &sw );

			newArea = new CNavArea( &nw, &ne, &se, &sw );

			this->ConnectTo( newArea, NORTH );
			newArea->ConnectTo( this, SOUTH );

			other->ConnectTo( newArea, SOUTH );
			newArea->ConnectTo( other, NORTH );
		}
		else if (m_extent.hi.y < other->m_extent.lo.y)
		{
			// 'this' is north of 'other'
			float left = max( m_extent.lo.x, other->m_extent.lo.x );
			float right = min( m_extent.hi.x, other->m_extent.hi.x );

			nw.x = left;
			nw.y = m_extent.hi.y;
			nw.z = GetZ( &nw );

			se.x = right;
			se.y = other->m_extent.lo.y;
			se.z = other->GetZ( &se );

			ne.x = se.x;
			ne.y = nw.y;
			ne.z = GetZ( &ne );

			sw.x = nw.x;
			sw.y = se.y;
			sw.z = other->GetZ( &sw );

			newArea = new CNavArea( &nw, &ne, &se, &sw );

			this->ConnectTo( newArea, SOUTH );
			newArea->ConnectTo( this, NORTH );

			other->ConnectTo( newArea, NORTH );
			newArea->ConnectTo( other, SOUTH );
		}
		else
		{
			// areas overlap
			return false;
		}
	}

	// if both areas have the same place, the new area inherits it
	if (GetPlace() == other->GetPlace())
	{
		newArea->SetPlace( GetPlace() );
	}
	else if (GetPlace() == UNDEFINED_PLACE)
	{
		newArea->SetPlace( other->GetPlace() );
	}
	else if (other->GetPlace() == UNDEFINED_PLACE)
	{
		newArea->SetPlace( GetPlace() );
	}
	else
	{
		// both have valid, but different places - pick on at random
		if (RandomInt( 0, 100 ) < 50)
			newArea->SetPlace( GetPlace() );
		else
			newArea->SetPlace( other->GetPlace() );
	}

	TheNavAreaList.AddToTail( newArea );
	TheNavMesh->AddNavArea( newArea );

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Merge this area and given adjacent area 
 */
bool CNavArea::MergeEdit( CNavArea *adj )
{
	// can only merge if attributes of both areas match


	// check that these areas can be merged
	const float tolerance = 1.0f;
	bool merge = false;
	if (fabs( m_extent.lo.x - adj->m_extent.lo.x ) < tolerance && 
		fabs( m_extent.hi.x - adj->m_extent.hi.x ) < tolerance)
		merge = true;

	if (fabs( m_extent.lo.y - adj->m_extent.lo.y ) < tolerance && 
		fabs( m_extent.hi.y - adj->m_extent.hi.y ) < tolerance)
		merge = true;

	if (merge == false)
		return false;

	Extent origExtent = m_extent;
	
	// update extent
	if (m_extent.lo.x > adj->m_extent.lo.x || m_extent.lo.y > adj->m_extent.lo.y)
		m_extent.lo = adj->m_extent.lo;

	if (m_extent.hi.x < adj->m_extent.hi.x || m_extent.hi.y < adj->m_extent.hi.y)
		m_extent.hi = adj->m_extent.hi;

	m_center.x = (m_extent.lo.x + m_extent.hi.x)/2.0f;
	m_center.y = (m_extent.lo.y + m_extent.hi.y)/2.0f;
	m_center.z = (m_extent.lo.z + m_extent.hi.z)/2.0f;

	if (m_extent.hi.x > origExtent.hi.x || m_extent.lo.y < origExtent.lo.y)
		m_neZ = adj->GetZ( m_extent.hi.x, m_extent.lo.y );
	else
		m_neZ = GetZ( m_extent.hi.x, m_extent.lo.y );

	if (m_extent.lo.x < origExtent.lo.x || m_extent.hi.y > origExtent.hi.y)
		m_swZ = adj->GetZ( m_extent.lo.x, m_extent.hi.y );
	else
		m_swZ = GetZ( m_extent.lo.x, m_extent.hi.y );

	// merge adjacency links - we gain all the connections that adjArea had
	MergeAdjacentConnections( adj );

	// remove subsumed adjacent area
	TheNavAreaList.FindAndRemove( adj );
	delete adj;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
void ApproachAreaAnalysisPrep( void )
{
}

//--------------------------------------------------------------------------------------------------------------
void CleanupApproachAreaAnalysisPrep( void )
{
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Remove "analyzed" data from nav area
 */
void CNavArea::Strip( void )
{
	m_approachCount = 0;
	m_spotEncounterList.RemoveAll();		// memory leak	
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if area is more or less square.
 * This is used when merging to prevent long, thin, areas being created.
 */
bool CNavArea::IsRoughlySquare( void ) const
{
	float aspect = GetSizeX() / GetSizeY();

	const float maxAspect = 3.01;
	const float minAspect = 1.0f / maxAspect;
	if (aspect < minAspect || aspect > maxAspect)
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if 'pos' is within 2D extents of area.
 */
bool CNavArea::IsOverlapping( const Vector *pos ) const
{
	if (pos->x >= m_extent.lo.x && pos->x <= m_extent.hi.x &&
			pos->y >= m_extent.lo.y && pos->y <= m_extent.hi.y)
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if 'area' overlaps our 2D extents
 */
bool CNavArea::IsOverlapping( const CNavArea *area ) const
{
	if (area->m_extent.lo.x < m_extent.hi.x && area->m_extent.hi.x > m_extent.lo.x && 
			area->m_extent.lo.y < m_extent.hi.y && area->m_extent.hi.y > m_extent.lo.y)
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if 'area' overlaps our X extent
 */
bool CNavArea::IsOverlappingX( const CNavArea *area ) const
{
	if (area->m_extent.lo.x < m_extent.hi.x && area->m_extent.hi.x > m_extent.lo.x)
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if 'area' overlaps our Y extent
 */
bool CNavArea::IsOverlappingY( const CNavArea *area ) const
{
	if (area->m_extent.lo.y < m_extent.hi.y && area->m_extent.hi.y > m_extent.lo.y)
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if given point is on or above this area, but no others
 */
bool CNavArea::Contains( const Vector *pos ) const
{
	// check 2D overlap
	if (!IsOverlapping( pos ))
		return false;

	// the point overlaps us, check that it is above us, but not above any areas that overlap us
	float ourZ = GetZ( pos );

	// if we are above this point, fail
	if (ourZ > pos->z)
		return false;

	FOR_EACH_LL( m_overlapList, it )
	{
		const CNavArea *area = m_overlapList[ it ];

		// skip self
		if (area == this)
			continue;

		// check 2D overlap
		if (!area->IsOverlapping( pos ))
			continue;

		float theirZ = area->GetZ( pos );
		if (theirZ > pos->z)
		{
			// they are above the point
			continue;
		}

		if (theirZ > ourZ)
		{
			// we are below an area that is closer underneath the point
			return false;
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this area and given area are approximately co-planar
 */
bool CNavArea::IsCoplanar( const CNavArea *area ) const
{
	Vector u, v;

	// compute our unit surface normal
	u.x = m_extent.hi.x - m_extent.lo.x;
	u.y = 0.0f;
	u.z = m_neZ - m_extent.lo.z;

	v.x = 0.0f;
	v.y = m_extent.hi.y - m_extent.lo.y;
	v.z = m_swZ - m_extent.lo.z;

	Vector normal = CrossProduct( u, v );
	normal.NormalizeInPlace();


	// compute their unit surface normal
	u.x = area->m_extent.hi.x - area->m_extent.lo.x;
	u.y = 0.0f;
	u.z = area->m_neZ - area->m_extent.lo.z;

	v.x = 0.0f;
	v.y = area->m_extent.hi.y - area->m_extent.lo.y;
	v.z = area->m_swZ - area->m_extent.lo.z;

	Vector otherNormal = CrossProduct( u, v );
	otherNormal.NormalizeInPlace();

	// can only merge areas that are nearly planar, to ensure areas do not differ from underlying geometry much
	const float tolerance = 0.99f; // 0.7071f;		// 0.9
	if (DotProduct( normal, otherNormal ) > tolerance)
		return true;

	return false;	
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return Z of area at (x,y) of 'pos'
 * Trilinear interpolation of Z values at quad edges.
 * NOTE: pos->z is not used.
 */
float CNavArea::GetZ( const Vector *pos ) const
{
	float dx = m_extent.hi.x - m_extent.lo.x;
	float dy = m_extent.hi.y - m_extent.lo.y;

	// guard against division by zero due to degenerate areas
	if (dx == 0.0f || dy == 0.0f)
		return m_neZ;

	float u = (pos->x - m_extent.lo.x) / dx;
	float v = (pos->y - m_extent.lo.y) / dy;

	// clamp Z values to (x,y) volume
	if (u < 0.0f)
		u = 0.0f;
	else if (u > 1.0f)
		u = 1.0f;
		
	if (v < 0.0f)
		v = 0.0f;
	else if (v > 1.0f)
		v = 1.0f;

	float northZ = m_extent.lo.z + u * (m_neZ - m_extent.lo.z);
	float southZ = m_swZ + u * (m_extent.hi.z - m_swZ);
	
	return northZ + v * (southZ - northZ);
}

float CNavArea::GetZ( float x, float y ) const
{
	Vector pos( x, y, 0.0f );
	return GetZ( &pos );
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return closest point to 'pos' on 'area'.
 * Returned point is in 'close'.
 */
void CNavArea::GetClosestPointOnArea( const Vector *pos, Vector *close ) const
{
	const Extent *extent = GetExtent();

	if (pos->x < extent->lo.x)
	{
		if (pos->y < extent->lo.y)
		{
			// position is north-west of area
			*close = extent->lo;
		}
		else if (pos->y > extent->hi.y)
		{
			// position is south-west of area
			close->x = extent->lo.x;
			close->y = extent->hi.y;
		}
		else
		{
			// position is west of area
			close->x = extent->lo.x;
			close->y = pos->y;
		}
	}
	else if (pos->x > extent->hi.x)
	{
		if (pos->y < extent->lo.y)
		{
			// position is north-east of area
			close->x = extent->hi.x;
			close->y = extent->lo.y;
		}
		else if (pos->y > extent->hi.y)
		{
			// position is south-east of area
			*close = extent->hi;
		}
		else
		{
			// position is east of area
			close->x = extent->hi.x;
			close->y = pos->y;
		}
	}
	else if (pos->y < extent->lo.y)
	{
		// position is north of area
		close->x = pos->x;
		close->y = extent->lo.y;
	}
	else if (pos->y > extent->hi.y)
	{
		// position is south of area
		close->x = pos->x;
		close->y = extent->hi.y;
	}
	else
	{
		// position is inside of area - it is the 'closest point' to itself
		*close = *pos;
	}

	close->z = GetZ( close );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return shortest distance squared between point and this area
 */
float CNavArea::GetDistanceSquaredToPoint( const Vector *pos ) const
{
	const Extent *extent = GetExtent();

	if (pos->x < extent->lo.x)
	{
		if (pos->y < extent->lo.y)
		{
			// position is north-west of area
			return (extent->lo - *pos).LengthSqr();
		}
		else if (pos->y > extent->hi.y)
		{
			// position is south-west of area
			Vector d;
			d.x = extent->lo.x - pos->x;
			d.y = extent->hi.y - pos->y;
			d.z = m_swZ - pos->z;
			return d.LengthSqr();
		}
		else
		{
			// position is west of area
			float d = extent->lo.x - pos->x;
			return d * d;
		}
	}
	else if (pos->x > extent->hi.x)
	{
		if (pos->y < extent->lo.y)
		{
			// position is north-east of area
			Vector d;
			d.x = extent->hi.x - pos->x;
			d.y = extent->lo.y - pos->y;
			d.z = m_neZ - pos->z;
			return d.LengthSqr();
		}
		else if (pos->y > extent->hi.y)
		{
			// position is south-east of area
			return (extent->hi - *pos).LengthSqr();
		}
		else
		{
			// position is east of area
			float d = pos->z - extent->hi.x;
			return d * d;
		}
	}
	else if (pos->y < extent->lo.y)
	{
		// position is north of area
		float d = extent->lo.y - pos->y;
		return d * d;
	}
	else if (pos->y > extent->hi.y)
	{
		// position is south of area
		float d = pos->y - extent->hi.y;
		return d * d;
	}
	else
	{
		// position is inside of 2D extent of area - find delta Z
		float z = GetZ( pos );
		float d = z - pos->z;
		return d * d;
	}
}



//--------------------------------------------------------------------------------------------------------------
CNavArea *CNavArea::GetRandomAdjacentArea( NavDirType dir ) const
{
	int count = m_connect[ dir ].Count();
	int which = RandomInt( 0, count-1 );

	int i = 0;
	FOR_EACH_LL( m_connect[ dir ], it )
	{
		if (i == which)
			return m_connect[ dir ][ it ].area;

		++i;
	}

	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute "portal" between to adjacent areas. 
 * Return center of portal opening, and half-width defining sides of portal from center.
 * NOTE: center->z is unset.
 */
void CNavArea::ComputePortal( const CNavArea *to, NavDirType dir, Vector *center, float *halfWidth ) const
{
	if (dir == NORTH || dir == SOUTH)
	{
		if (dir == NORTH)
			center->y = m_extent.lo.y;
		else
			center->y = m_extent.hi.y;

		float left = max( m_extent.lo.x, to->m_extent.lo.x );
		float right = min( m_extent.hi.x, to->m_extent.hi.x );

		// clamp to our extent in case areas are disjoint
		if (left < m_extent.lo.x)
			left = m_extent.lo.x;
		else if (left > m_extent.hi.x)
			left = m_extent.hi.x;

		if (right < m_extent.lo.x)
			right = m_extent.lo.x;
		else if (right > m_extent.hi.x)
			right = m_extent.hi.x;

		center->x = (left + right)/2.0f;
		*halfWidth = (right - left)/2.0f;
	}
	else	// EAST or WEST
	{
		if (dir == WEST)
			center->x = m_extent.lo.x;
		else
			center->x = m_extent.hi.x;

		float top = max( m_extent.lo.y, to->m_extent.lo.y );
		float bottom = min( m_extent.hi.y, to->m_extent.hi.y );

		// clamp to our extent in case areas are disjoint
		if (top < m_extent.lo.y)
			top = m_extent.lo.y;
		else if (top > m_extent.hi.y)
			top = m_extent.hi.y;

		if (bottom < m_extent.lo.y)
			bottom = m_extent.lo.y;
		else if (bottom > m_extent.hi.y)
			bottom = m_extent.hi.y;

		center->y = (top + bottom)/2.0f;
		*halfWidth = (bottom - top)/2.0f;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute closest point within the "portal" between to adjacent areas. 
 */
void CNavArea::ComputeClosestPointInPortal( const CNavArea *to, NavDirType dir, const Vector *fromPos, Vector *closePos ) const
{
	const float margin = GenerationStepSize/2.0f;

	if (dir == NORTH || dir == SOUTH)
	{
		if (dir == NORTH)
			closePos->y = m_extent.lo.y;
		else
			closePos->y = m_extent.hi.y;

		float left = max( m_extent.lo.x, to->m_extent.lo.x );
		float right = min( m_extent.hi.x, to->m_extent.hi.x );

		// clamp to our extent in case areas are disjoint
		if (left < m_extent.lo.x)
			left = m_extent.lo.x;
		else if (left > m_extent.hi.x)
			left = m_extent.hi.x;

		if (right < m_extent.lo.x)
			right = m_extent.lo.x;
		else if (right > m_extent.hi.x)
			right = m_extent.hi.x;

		// keep margin if against edge
		const float leftMargin = (to->IsEdge( WEST )) ? (left + margin) : left;
		const float rightMargin = (to->IsEdge( EAST )) ? (right - margin) : right;

		// limit x to within portal
		if (fromPos->x < leftMargin)
			closePos->x = leftMargin;
		else if (fromPos->x > rightMargin)
			closePos->x = rightMargin;
		else
			closePos->x = fromPos->x;

	}
	else	// EAST or WEST
	{
		if (dir == WEST)
			closePos->x = m_extent.lo.x;
		else
			closePos->x = m_extent.hi.x;

		float top = max( m_extent.lo.y, to->m_extent.lo.y );
		float bottom = min( m_extent.hi.y, to->m_extent.hi.y );

		// clamp to our extent in case areas are disjoint
		if (top < m_extent.lo.y)
			top = m_extent.lo.y;
		else if (top > m_extent.hi.y)
			top = m_extent.hi.y;

		if (bottom < m_extent.lo.y)
			bottom = m_extent.lo.y;
		else if (bottom > m_extent.hi.y)
			bottom = m_extent.hi.y;

		// keep margin if against edge
		const float topMargin = (to->IsEdge( NORTH )) ? (top + margin) : top;
		const float bottomMargin = (to->IsEdge( SOUTH )) ? (bottom - margin) : bottom;

		// limit y to within portal
		if (fromPos->y < topMargin)
			closePos->y = topMargin;
		else if (fromPos->y > bottomMargin)
			closePos->y = bottomMargin;
		else
			closePos->y = fromPos->y;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if there are no bi-directional links on the given side
 */
bool CNavArea::IsEdge( NavDirType dir ) const
{
	FOR_EACH_LL( m_connect[ dir ], it )
	{
		const NavConnect connect = m_connect[ dir ][ it ];

		if (connect.area->IsConnected( this, OppositeDirection( dir ) ))
			return false;
	}

	return true;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return direction from this area to the given point
 */
NavDirType CNavArea::ComputeDirection( Vector *point ) const
{
	if (point->x >= m_extent.lo.x && point->x <= m_extent.hi.x)
	{
		if (point->y < m_extent.lo.y)
			return NORTH;
		else if (point->y > m_extent.hi.y)
			return SOUTH;
	}
	else if (point->y >= m_extent.lo.y && point->y <= m_extent.hi.y)
	{
		if (point->x < m_extent.lo.x)
			return WEST;
		else if (point->x > m_extent.hi.x)
			return EAST;
	}

	// find closest direction
	Vector to = *point - m_center;

	if (fabs(to.x) > fabs(to.y))
	{
		if (to.x > 0.0f)
			return EAST;
		return WEST;
	}
	else
	{
		if (to.y > 0.0f)
			return SOUTH;
		return NORTH;
	}

	return NUM_DIRECTIONS;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Draw area for debugging
 */
void CNavArea::Draw( byte red, byte green, byte blue, int duration )
{
	Vector nw, ne, sw, se;

	nw = m_extent.lo;
	se = m_extent.hi;
	ne.x = se.x;
	ne.y = nw.y;
	ne.z = m_neZ;
	sw.x = nw.x;
	sw.y = se.y;
	sw.z = m_swZ;

	float border = 2.0f;
	nw.x += border;
	nw.y += border;
	ne.x -= border;
	ne.y += border;
	sw.x += border;
	sw.y -= border;
	se.x -= border;
	se.y -= border;

	DrawLine( nw, ne, duration, red, green, blue );
	DrawLine( ne, se, duration, red, green, blue );
	DrawLine( se, sw, duration, red, green, blue );
	DrawLine( sw, nw, duration, red, green, blue );

	if (GetAttributes() & NAV_MESH_CROUCH)
		DrawLine( nw, se, duration, red, green, blue );

	if (GetAttributes() & NAV_MESH_JUMP)
	{
		DrawLine( nw, se, duration, red, green, blue );
		DrawLine( ne, sw, duration, red, green, blue );
	}

	if (GetAttributes() & NAV_MESH_PRECISE)
	{
		float size = 8.0f;
		Vector up( m_center.x, m_center.y - size, m_center.z );
		Vector down( m_center.x, m_center.y + size, m_center.z );
		DrawLine( up, down, duration, red, green, blue );

		Vector left( m_center.x - size, m_center.y, m_center.z );
		Vector right( m_center.x + size, m_center.y, m_center.z );
		DrawLine( left, right, duration, red, green, blue );
	}

	if (GetAttributes() & NAV_MESH_NO_JUMP)
	{
		float size = 8.0f;
		Vector up( m_center.x, m_center.y - size, m_center.z );
		Vector down( m_center.x, m_center.y + size, m_center.z );
		Vector left( m_center.x - size, m_center.y, m_center.z );
		Vector right( m_center.x + size, m_center.y, m_center.z );
		DrawLine( up, right, duration, red, green, blue );
		DrawLine( right, down, duration, red, green, blue );
		DrawLine( down, left, duration, red, green, blue );
		DrawLine( left, up, duration, red, green, blue );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Draw selected corner for debugging
 */
void CNavArea::DrawMarkedCorner( NavCornerType corner, byte red, byte green, byte blue, int duration )
{
	Vector nw, ne, sw, se;

	nw = m_extent.lo;
	se = m_extent.hi;
	ne.x = se.x;
	ne.y = nw.y;
	ne.z = m_neZ;
	sw.x = nw.x;
	sw.y = se.y;
	sw.z = m_swZ;

	float border = 2.0f;
	nw.x += border;
	nw.y += border;
	ne.x -= border;
	ne.y += border;
	sw.x += border;
	sw.y -= border;
	se.x -= border;
	se.y -= border;

	switch( corner )
	{
	case NORTH_WEST:
		DrawLine( nw + Vector( 0, 0, 10 ), nw, duration, red, green, blue );
		break;
	case NORTH_EAST:
		DrawLine( ne + Vector( 0, 0, 10 ), ne, duration, red, green, blue );
		break;
	case SOUTH_EAST:
		DrawLine( se + Vector( 0, 0, 10 ), se, duration, red, green, blue );
		break;
	case SOUTH_WEST:
		DrawLine( sw + Vector( 0, 0, 10 ), sw, duration, red, green, blue );
		break;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Add to open list in decreasing value order
 */
void CNavArea::AddToOpenList( void )
{
	// mark as being on open list for quick check
	m_openMarker = m_masterMarker;

	// if list is empty, add and return
	if (m_openList == NULL)
	{
		m_openList = this;
		this->m_prevOpen = NULL;
		this->m_nextOpen = NULL;
		return;
	}

	// insert self in ascending cost order
	CNavArea *area, *last = NULL;
	for( area = m_openList; area; area = area->m_nextOpen )
	{
		if (this->GetTotalCost() < area->GetTotalCost())
			break;

		last = area;
	}

	if (area)
	{
		// insert before this area
		this->m_prevOpen = area->m_prevOpen;
		if (this->m_prevOpen)
			this->m_prevOpen->m_nextOpen = this;
		else
			m_openList = this;

		this->m_nextOpen = area;
		area->m_prevOpen = this;
	}
	else
	{
		// append to end of list
		last->m_nextOpen = this;

		this->m_prevOpen = last;
		this->m_nextOpen = NULL;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * A smaller value has been found, update this area on the open list
 * @todo "bubbling" does unnecessary work, since the order of all other nodes will be unchanged - only this node is altered
 */
void CNavArea::UpdateOnOpenList( void )
{
	// since value can only decrease, bubble this area up from current spot
	while( m_prevOpen && 
				 this->GetTotalCost() < m_prevOpen->GetTotalCost() )
	{
		// swap position with predecessor
		CNavArea *other = m_prevOpen;
		CNavArea *before = other->m_prevOpen;
		CNavArea *after  = this->m_nextOpen;

		this->m_nextOpen = other;
		this->m_prevOpen = before;

		other->m_prevOpen = this;
		other->m_nextOpen = after;

		if (before)
			before->m_nextOpen = this;
		else
			m_openList = this;

		if (after)
			after->m_prevOpen = other;
	}
}

//--------------------------------------------------------------------------------------------------------------
void CNavArea::RemoveFromOpenList( void )
{
	if (m_prevOpen)
		m_prevOpen->m_nextOpen = m_nextOpen;
	else
		m_openList = m_nextOpen;

	if (m_nextOpen)
		m_nextOpen->m_prevOpen = m_prevOpen;

	// zero is an invalid marker
	m_openMarker = 0;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Clears the open and closed lists for a new search
 */
void CNavArea::ClearSearchLists( void )
{
	// effectively clears all open list pointers and closed flags
	CNavArea::MakeNewMarker();

	m_openList = NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the coordinates of the area's corner.
 * NOTE: Do not retain the returned pointer - it is temporary.
 */
const Vector *CNavArea::GetCorner( NavCornerType corner ) const
{
	static Vector pos;

	switch( corner )
	{
		case NORTH_WEST:
			return &m_extent.lo;

		case NORTH_EAST:
			pos.x = m_extent.hi.x;
			pos.y = m_extent.lo.y;
			pos.z = m_neZ;
			return &pos;

		case SOUTH_WEST:
			pos.x = m_extent.lo.x;
			pos.y = m_extent.hi.y;
			pos.z = m_swZ;
			return &pos;

		case SOUTH_EAST:
			return &m_extent.hi;
	}

	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if an existing hiding spot is too close to given position
 */
bool CNavArea::IsHidingSpotCollision( const Vector *pos ) const
{
	const float collisionRange = 30.0f;

	FOR_EACH_LL( m_hidingSpotList, it )
	{
		const HidingSpot *spot = m_hidingSpotList[ it ];

		if ((*spot->GetPosition() - *pos).IsLengthLessThan( collisionRange ))
			return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
bool IsHidingSpotInCover( const Vector *spot )
{
	int coverCount = 0;
	trace_t result;

	Vector from = *spot;
	from.z += HalfHumanHeight;

	Vector to;

	// if we are crouched underneath something, that counts as good cover
	to = from + Vector( 0, 0, 20.0f );
	UTIL_TraceLine( from, to, MASK_PLAYERSOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &result );
	if (result.fraction != 1.0f)
		return true;

	const float coverRange = 100.0f;
	const float inc = M_PI / 8.0f;

	for( float angle = 0.0f; angle < 2.0f * M_PI; angle += inc )
	{
		to = from + Vector( coverRange * (float)cos(angle), coverRange * (float)sin(angle), HalfHumanHeight );

		UTIL_TraceLine( from, to, MASK_PLAYERSOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &result );

		// if traceline hit something, it hit "cover"
		if (result.fraction != 1.0f)
			++coverCount;
	}

	// if more than half of the circle has no cover, the spot is not "in cover"
	const int halfCover = 8;
	if (coverCount < halfCover)
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Analyze local area neighborhood to find "hiding spots" for this area
 */
void CNavArea::ComputeHidingSpots( void )
{
	struct
	{
		float lo, hi;
	}
	extent;

	// "jump areas" cannot have hiding spots
	if (GetAttributes() & NAV_MESH_JUMP)
		return;

	int cornerCount[NUM_CORNERS];
	for( int i=0; i<NUM_CORNERS; ++i )
		cornerCount[i] = 0;

	const float cornerSize = 20.0f;

	// for each direction, find extents of adjacent areas along the wall
	for( int d=0; d<NUM_DIRECTIONS; ++d )
	{
		extent.lo = 999999.9f;
		extent.hi = -999999.9f;

		bool isHoriz = (d == NORTH || d == SOUTH) ? true : false;

		FOR_EACH_LL( m_connect[d], it )
		{
			NavConnect connect = m_connect[ d ][ it ];

			// if connection is only one-way, it's a "jump down" connection (ie: a discontinuity that may mean cover) 
			// ignore it
			if (connect.area->IsConnected( this, OppositeDirection( static_cast<NavDirType>( d ) ) ) == false)
				continue;

			// ignore jump areas
			if (connect.area->GetAttributes() & NAV_MESH_JUMP)
				continue;

			if (isHoriz)
			{
				if (connect.area->m_extent.lo.x < extent.lo)
					extent.lo = connect.area->m_extent.lo.x;

				if (connect.area->m_extent.hi.x > extent.hi)
					extent.hi = connect.area->m_extent.hi.x;			
			}
			else
			{
				if (connect.area->m_extent.lo.y < extent.lo)
					extent.lo = connect.area->m_extent.lo.y;

				if (connect.area->m_extent.hi.y > extent.hi)
					extent.hi = connect.area->m_extent.hi.y;
			}
		}

		switch( d )
		{
			case NORTH:
				if (extent.lo - m_extent.lo.x >= cornerSize)
					++cornerCount[ NORTH_WEST ];

				if (m_extent.hi.x - extent.hi >= cornerSize)
					++cornerCount[ NORTH_EAST ];
				break;

			case SOUTH:
				if (extent.lo - m_extent.lo.x >= cornerSize)
					++cornerCount[ SOUTH_WEST ];

				if (m_extent.hi.x - extent.hi >= cornerSize)
					++cornerCount[ SOUTH_EAST ];
				break;

			case EAST:
				if (extent.lo - m_extent.lo.y >= cornerSize)
					++cornerCount[ NORTH_EAST ];

				if (m_extent.hi.y - extent.hi >= cornerSize)
					++cornerCount[ SOUTH_EAST ];
				break;

			case WEST:
				if (extent.lo - m_extent.lo.y >= cornerSize)
					++cornerCount[ NORTH_WEST ];

				if (m_extent.hi.y - extent.hi >= cornerSize)
					++cornerCount[ SOUTH_WEST ];
				break;
		}
	}

	// if a corner count is 2, then it really is a corner (walls on both sides)
	float offset = 12.5f;

	if (cornerCount[ NORTH_WEST ] == 2)
	{
		Vector pos = *GetCorner( NORTH_WEST ) + Vector(  offset,  offset, 0.0f );

		m_hidingSpotList.AddToTail( new HidingSpot( &pos, (IsHidingSpotInCover( &pos )) ? HidingSpot::IN_COVER : 0 ) );
	}

	if (cornerCount[ NORTH_EAST ] == 2)
	{
		Vector pos = *GetCorner( NORTH_EAST ) + Vector( -offset,  offset, 0.0f );
		if (!IsHidingSpotCollision( &pos ))
			m_hidingSpotList.AddToTail( new HidingSpot( &pos, (IsHidingSpotInCover( &pos )) ? HidingSpot::IN_COVER : 0 ) );
	}

	if (cornerCount[ SOUTH_WEST ] == 2)
	{
		Vector pos = *GetCorner( SOUTH_WEST ) + Vector(  offset, -offset, 0.0f );
		if (!IsHidingSpotCollision( &pos ))
			m_hidingSpotList.AddToTail( new HidingSpot( &pos, (IsHidingSpotInCover( &pos )) ? HidingSpot::IN_COVER : 0 ) );
	}

	if (cornerCount[ SOUTH_EAST ] == 2)
	{
		Vector pos = *GetCorner( SOUTH_EAST ) + Vector( -offset, -offset, 0.0f );
		if (!IsHidingSpotCollision( &pos ))
			m_hidingSpotList.AddToTail( new HidingSpot( &pos, (IsHidingSpotInCover( &pos )) ? HidingSpot::IN_COVER : 0 ) );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Determine how much walkable area we can see from the spot, and how far away we can see.
 */
void ClassifySniperSpot( HidingSpot *spot )
{
	Vector eye = *spot->GetPosition() + Vector( 0, 0, HalfHumanHeight );		// assume we are crouching
	Vector walkable;
	trace_t result;

	Extent sniperExtent;
	float farthestRangeSq = 0.0f;
	const float minSniperRangeSq = 1000.0f * 1000.0f;
	bool found = false;

	// to make compiler stop warning me
	sniperExtent.lo = Vector( 0.0f, 0.0f, 0.0f );
	sniperExtent.hi = Vector( 0.0f, 0.0f, 0.0f );

	FOR_EACH_LL( TheNavAreaList, it )
	{
		CNavArea *area = TheNavAreaList[ it ];

		const Extent *extent = area->GetExtent();

		// scan this area
		for( walkable.y = extent->lo.y + GenerationStepSize/2.0f; walkable.y < extent->hi.y; walkable.y += GenerationStepSize )
		{
			for( walkable.x = extent->lo.x + GenerationStepSize/2.0f; walkable.x < extent->hi.x; walkable.x += GenerationStepSize )
			{
				walkable.z = area->GetZ( &walkable ) + HalfHumanHeight;
				
				// check line of sight
				// BOTPORT: ignore glass here
				UTIL_TraceLine( eye, walkable, MASK_PLAYERSOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &result );

				if (result.fraction == 1.0f && !result.startsolid)
				{
					// can see this spot

					// keep track of how far we can see
					float rangeSq = (eye - walkable).LengthSqr();
					if (rangeSq > farthestRangeSq)
					{
						farthestRangeSq = rangeSq;

						if (rangeSq >= minSniperRangeSq)
						{
							// this is a sniper spot
							// determine how good of a sniper spot it is by keeping track of the snipable area
							if (found)
							{
								if (walkable.x < sniperExtent.lo.x)
									sniperExtent.lo.x = walkable.x;
								if (walkable.x > sniperExtent.hi.x)
									sniperExtent.hi.x = walkable.x;

								if (walkable.y < sniperExtent.lo.y)
									sniperExtent.lo.y = walkable.y;
								if (walkable.y > sniperExtent.hi.y)
									sniperExtent.hi.y = walkable.y;
							}
							else
							{
								sniperExtent.lo = walkable;
								sniperExtent.hi = walkable;
								found = true;
							}
						}
					}
				}	
			}
		}
	}

	if (found)
	{
		// if we can see a large snipable area, it is an "ideal" spot
		float snipableArea = sniperExtent.Area();

		const float minIdealSniperArea = 200.0f * 200.0f;
		const float longSniperRangeSq = 1500.0f * 1500.0f;

		if (snipableArea >= minIdealSniperArea || farthestRangeSq >= longSniperRangeSq)
			spot->SetFlags( HidingSpot::IDEAL_SNIPER_SPOT );
		else
			spot->SetFlags( HidingSpot::GOOD_SNIPER_SPOT );
	}
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Analyze local area neighborhood to find "sniper spots" for this area
 */
void CNavArea::ComputeSniperSpots( void )
{
	if (nav_quicksave.GetBool())
		return;

	FOR_EACH_LL( m_hidingSpotList, it )
	{
		HidingSpot *spot = m_hidingSpotList[ it ];

		ClassifySniperSpot( spot );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Given the areas we are moving between, return the spots we will encounter
 */
SpotEncounter *CNavArea::GetSpotEncounter( const CNavArea *from, const CNavArea *to )
{
	if (from && to)
	{
		SpotEncounter *e;

		FOR_EACH_LL( m_spotEncounterList, it )
		{
			e = m_spotEncounterList[ it ];

			if (e->from.area == from && e->to.area == to)
				return e;
		}
	}

	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Add spot encounter data when moving from area to area
 */
void CNavArea::AddSpotEncounters( const CNavArea *from, NavDirType fromDir, const CNavArea *to, NavDirType toDir )
{
	SpotEncounter *e = new SpotEncounter;

	e->from.area = const_cast<CNavArea *>( from );
	e->fromDir = fromDir;

	e->to.area = const_cast<CNavArea *>( to );
	e->toDir = toDir;

	float halfWidth;
	ComputePortal( to, toDir, &e->path.to, &halfWidth );
	ComputePortal( from, fromDir, &e->path.from, &halfWidth );

	const float eyeHeight = HalfHumanHeight;
	e->path.from.z = from->GetZ( &e->path.from ) + eyeHeight;
	e->path.to.z = to->GetZ( &e->path.to ) + eyeHeight;

	// step along ray and track which spots can be seen
	Vector dir = e->path.to - e->path.from;
	float length = dir.NormalizeInPlace();

	// create unique marker to flag used spots
	HidingSpot::ChangeMasterMarker();

	const float stepSize = 25.0f;		// 50
	const float seeSpotRange = 2000.0f;	// 3000
	trace_t result;

	Vector eye, delta;
	HidingSpot *spot;
	SpotOrder spotOrder;

	// step along path thru this area
	bool done = false;
	for( float along = 0.0f; !done; along += stepSize )
	{
		// make sure we check the endpoint of the path segment
		if (along >= length)
		{
			along = length;
			done = true;
		}

		// move the eyepoint along the path segment
		eye = e->path.from + along * dir;

		// check each hiding spot for visibility
		FOR_EACH_LL( TheHidingSpotList, it )
		{
			spot = TheHidingSpotList[ it ];

			// only look at spots with cover (others are out in the open and easily seen)
			if (!spot->HasGoodCover())
				continue;

			if (spot->IsMarked())
				continue;

			const Vector *spotPos = spot->GetPosition();

			delta.x = spotPos->x - eye.x;
			delta.y = spotPos->y - eye.y;
			delta.z = (spotPos->z + eyeHeight) - eye.z;

			// check if in range
			if (delta.IsLengthGreaterThan( seeSpotRange ))
				continue;

			// check if we have LOS
			// BOTPORT: ignore glass here
			UTIL_TraceLine( eye, Vector( spotPos->x, spotPos->y, spotPos->z + HalfHumanHeight ), MASK_PLAYERSOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &result );
			if (result.fraction != 1.0f)
				continue;

			// if spot is in front of us along our path, ignore it
			delta.NormalizeInPlace();
			float dot = DotProduct( dir, delta );
			if (dot < 0.7071f && dot > -0.7071f)
			{
				// we only want to keep spots that BECOME visible as we walk past them
				// therefore, skip ALL visible spots at the start of the path segment
				if (along > 0.0f)
				{
					// add spot to encounter
					spotOrder.spot = spot;
					spotOrder.t = along/length;
					e->spotList.AddToTail( spotOrder );
				}
			}

			// mark spot as encountered
			spot->Mark();
		}
	}

	// add encounter to list
	m_spotEncounterList.AddToTail( e );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute "spot encounter" data. This is an ordered list of spots to look at 
 * for each possible path thru a nav area.
 */
void CNavArea::ComputeSpotEncounters( void )
{
	m_spotEncounterList.RemoveAll();

	if (nav_quicksave.GetBool())
		return;

	// for each adjacent area
	for( int fromDir=0; fromDir<NUM_DIRECTIONS; ++fromDir )
	{
		FOR_EACH_LL( m_connect[ fromDir ], it )
		{
			NavConnect *fromCon = &(m_connect[ fromDir ][ it ]);

			// compute encounter data for path to each adjacent area
			for( int toDir=0; toDir<NUM_DIRECTIONS; ++toDir )
			{
				FOR_EACH_LL( m_connect[ toDir ], ot )
				{
					NavConnect *toCon = &(m_connect[ toDir ][ ot ]);

					if (toCon == fromCon)
						continue;

					// just do our direction, as we'll loop around for other direction
					AddSpotEncounters( fromCon->area, (NavDirType)fromDir, toCon->area, (NavDirType)toDir );
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Decay the danger values
 */
void CNavArea::DecayDanger( void )
{
	// one kill == 1.0, which we will forget about in two minutes
	const float decayRate = 1.0f / 120.0f;

	for( int i=0; i<MAX_AREA_TEAMS; ++i )
	{
		float deltaT = gpGlobals->curtime - m_dangerTimestamp[i];
		float decayAmount = decayRate * deltaT;

		m_danger[i] -= decayAmount;
		if (m_danger[i] < 0.0f)
			m_danger[i] = 0.0f;

		// update timestamp
		m_dangerTimestamp[i] = gpGlobals->curtime;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Increase the danger of this area for the given team
 */
void CNavArea::IncreaseDanger( int teamID, float amount )
{
	// before we add the new value, decay what's there
	DecayDanger();

	int teamIdx = teamID % MAX_AREA_TEAMS;

	m_danger[ teamIdx ] += amount;
	m_dangerTimestamp[ teamIdx ] = gpGlobals->curtime;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the danger of this area (decays over time)
 */
float CNavArea::GetDanger( int teamID )
{
	DecayDanger();

	int teamIdx = teamID % MAX_AREA_TEAMS;
	return m_danger[ teamIdx ];
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return number of players with given teamID in this area (teamID == 0 means any/all)
 * @todo Keep pointers to contained Players to make this a zero-time query
 */
int CNavArea::GetPlayerCount( int teamID ) const
{
	int count = 0;

	for( int i=1; i<=gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == NULL)
			continue;

		if (FNullEnt( player->edict() ))
			continue;

		if (!player->IsPlayer())
			continue;

		Vector playerOrigin = player->GetAbsOrigin();

		if (player->IsAlive() && 
			(teamID == 0 || player->GetTeamNumber() == teamID) &&
			IsOverlapping( &playerOrigin ))
		{
			++count;
		}
	}
	
	return count;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Raise/lower a corner
 */
void CNavArea::RaiseCorner( NavCornerType corner, int amount )
{
	if ( corner == NUM_CORNERS )
	{
		m_extent.lo.z += amount;
		m_extent.hi.z += amount;
		m_neZ += amount;
		m_swZ += amount;
	}
	else
	{
		switch (corner)
		{
		case NORTH_WEST:
			m_extent.lo.z += amount;
			break;
		case NORTH_EAST:
			m_neZ += amount;
			break;
		case SOUTH_WEST:
			m_swZ += amount;
			break;
		case SOUTH_EAST:
			m_extent.hi.z += amount;
			break;
		}
	}

	m_center.x = (m_extent.lo.x + m_extent.hi.x)/2.0f;
	m_center.y = (m_extent.lo.y + m_extent.hi.y)/2.0f;
	m_center.z = (m_extent.lo.z + m_extent.hi.z)/2.0f;
}


