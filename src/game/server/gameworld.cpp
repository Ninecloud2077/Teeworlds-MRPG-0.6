/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "gameworld.h"
#include "entity.h"
#include "gamecontext.h"

#include <engine/shared/config.h>

//////////////////////////////////////////////////
// game world
//////////////////////////////////////////////////
CGameWorld::CGameWorld(): m_pNextTraverseEntity(nullptr), m_Paused(false)
{
	m_pGS = nullptr;
	m_pServer = nullptr;

	m_ResetRequested = false;
	for (int i = 0; i < NUM_ENTTYPES; i++)
		m_apFirstEntityTypes[i] = nullptr;
}

CGameWorld::~CGameWorld()
{
	// delete all entities
	for(int i = 0; i < NUM_ENTTYPES; i++)
		while(m_apFirstEntityTypes[i])
			delete m_apFirstEntityTypes[i];
}

void CGameWorld::SetGameServer(CGS *pGS)
{
	m_pGS = pGS;
	m_pServer = m_pGS->Server();
}

CEntity *CGameWorld::FindFirst(int Type)
{
	return Type < 0 || Type >= NUM_ENTTYPES ? nullptr : m_apFirstEntityTypes[Type];
}

int CGameWorld::FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Type)
{
	if(Type < 0 || Type >= NUM_ENTTYPES)
		return 0;

	int Num = 0;
	for(CEntity *pEnt = m_apFirstEntityTypes[Type];	pEnt; pEnt = pEnt->m_pNextTypeEntity)
	{
		if(distance(pEnt->m_Pos, Pos) < Radius+pEnt->m_ProximityRadius)
		{
			if(ppEnts)
				ppEnts[Num] = pEnt;
			Num++;
			if(Num == Max)
				break;
		}
	}
	return Num;
}

void CGameWorld::InsertEntity(CEntity *pEnt)
{
#ifdef CONF_DEBUG
	for(CEntity *pCur = m_apFirstEntityTypes[pEnt->m_ObjType]; pCur; pCur = pCur->m_pNextTypeEntity)
		dbg_assert(pCur != pEnt, "err");
#endif

	// insert it
	if(m_apFirstEntityTypes[pEnt->m_ObjType])
		m_apFirstEntityTypes[pEnt->m_ObjType]->m_pPrevTypeEntity = pEnt;
	pEnt->m_pNextTypeEntity = m_apFirstEntityTypes[pEnt->m_ObjType];
	pEnt->m_pPrevTypeEntity = nullptr;
	m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt;
}

void CGameWorld::DestroyEntity(CEntity *pEnt)
{
	pEnt->MarkForDestroy();
}

void CGameWorld::RemoveEntity(CEntity *pEnt)
{
	// not in the list
	if(!pEnt->m_pNextTypeEntity && !pEnt->m_pPrevTypeEntity && m_apFirstEntityTypes[pEnt->m_ObjType] != pEnt)
		return;

	// remove
	if(pEnt->m_pPrevTypeEntity)
		pEnt->m_pPrevTypeEntity->m_pNextTypeEntity = pEnt->m_pNextTypeEntity;
	else
		m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt->m_pNextTypeEntity;
	if(pEnt->m_pNextTypeEntity)
		pEnt->m_pNextTypeEntity->m_pPrevTypeEntity = pEnt->m_pPrevTypeEntity;

	// keep list traversing valid
	if(m_pNextTraverseEntity == pEnt)
		m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;

	pEnt->m_pNextTypeEntity = nullptr;
	pEnt->m_pPrevTypeEntity = nullptr;
}

//
void CGameWorld::Snap(int SnappingClient)
{
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->Snap(SnappingClient);
			pEnt = m_pNextTraverseEntity;
		}
}

//
void CGameWorld::PostSnap()
{
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity* pEnt = m_apFirstEntityTypes[i]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->PostSnap();
			pEnt = m_pNextTraverseEntity;
		}
}

void CGameWorld::Reset()
{
	// reset all entities
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->Reset();
			pEnt = m_pNextTraverseEntity;
		}
	RemoveEntities();

	GS()->m_pController->OnReset();
	RemoveEntities();

	m_ResetRequested = false;
}

void CGameWorld::RemoveEntities()
{
	// destroy objects marked for destruction
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			if(pEnt->IsMarkedForDestroy())
			{
				RemoveEntity(pEnt);
				pEnt->Destroy();
				pEnt = nullptr;
			}
			pEnt = m_pNextTraverseEntity;
		}
}

void CGameWorld::Tick()
{
	if(m_ResetRequested)
		Reset();

	// update all objects
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->Tick();
			pEnt = m_pNextTraverseEntity;
		}

	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->TickDeferred();
			pEnt = m_pNextTraverseEntity;
		}

	RemoveEntities();

	UpdatePlayerMaps();
}


// TODO: should be more general
CCharacter *CGameWorld::IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, vec2& NewPos, CEntity *pNotThis)
{
	// Find other players
	float ClosestLen = distance(Pos0, Pos1) * 100.0f;
	CCharacter *pClosest = nullptr;

	CCharacter *p = (CCharacter *)FindFirst(ENTTYPE_CHARACTER);
	for(; p; p = (CCharacter *)p->TypeNext())
 	{
		if(p == pNotThis)
			continue;

		vec2 IntersectPos = closest_point_on_line(Pos0, Pos1, p->m_Pos);
		float Len = distance(p->m_Pos, IntersectPos);
		if(Len < p->m_ProximityRadius+Radius)
		{
			Len = distance(Pos0, IntersectPos);
			if(Len < ClosestLen)
			{
				NewPos = IntersectPos;
				ClosestLen = Len;
				pClosest = p;
			}
		}
	}

	return pClosest;
}

bool CGameWorld::IntersectClosestEntity(vec2 Pos, float Radius, int EnttypeID)
{
	for(CEntity *pDoor = FindFirst(EnttypeID); pDoor; pDoor = pDoor->TypeNext())
 	{
		vec2 IntersectPos = pDoor->m_PosTo;
		if(pDoor->m_Pos != pDoor->m_PosTo)
			IntersectPos = closest_point_on_line(pDoor->m_Pos, pDoor->m_PosTo, Pos);
		if (distance(IntersectPos, Pos) <= Radius)
			return true;
	}
	return false;
}

bool CGameWorld::IntersectClosestDoorEntity(vec2 Pos, float Radius)
{
	if(IntersectClosestEntity(Pos, Radius, ENTTYPE_DUNGEON_DOOR))
		return true;
	if(IntersectClosestEntity(Pos, Radius, ENTTYPE_DUNGEON_PROGRESS_DOOR))
		return true;
	if(IntersectClosestEntity(Pos, Radius, ENTTYPE_GUILD_HOUSE_DOOR))
		return true;
	if(IntersectClosestEntity(Pos, Radius, ENTTYPE_PLAYER_HOUSE_DOOR))
		return true;
	return false;
}

CEntity *CGameWorld::ClosestEntity(vec2 Pos, float Radius, int Type, CEntity *pNotThis) const
{
	// Find other players
	float ClosestRange = Radius*2;
	CEntity *pClosest = nullptr;

	CEntity *p = GS()->m_World.FindFirst(Type);
	for(; p; p = p->TypeNext())
 	{
		if(p == pNotThis)
			continue;

		const float Len = distance(Pos, p->m_Pos);
		if(Len < p->m_ProximityRadius+Radius)
		{
			if(Len < ClosestRange)
			{
				ClosestRange = Len;
				pClosest = p;
			}
		}
	}

	return pClosest;
}

bool distCompare(std::pair<float, int> a, std::pair<float, int> b)
{
	return (a.first < b.first);
}

void CGameWorld::UpdatePlayerMaps()
{
	if(Server()->Tick() % g_Config.m_SvMapUpdateRate != 0)
		return;

	std::pair<float, int> Dist[MAX_CLIENTS];
	for(int ClientID = 0; ClientID < MAX_PLAYERS; ClientID++)
	{
		CPlayer* pPlayer = GS()->m_apPlayers[ClientID];
		int ClientWorldID = Server()->GetClientWorldID(ClientID);
		if(!Server()->ClientIngame(ClientID) || ClientWorldID != GS()->GetWorldID() || !pPlayer)
			continue;

		int* pMap = Server()->GetIdMap(ClientID);

		// compute distances
		for(int j = MAX_PLAYERS; j < MAX_CLIENTS; j++)
		{
			Dist[j].second = j;

			CPlayerBot* pBotPlayer = dynamic_cast<CPlayerBot*>(GS()->m_apPlayers[j]);
			if(!Server()->ClientIngame(j) || !pBotPlayer || !pBotPlayer->GetCharacter())
			{
				Dist[j].first = 1e10;
				continue;
			}

			if(!pBotPlayer->IsVisibleForClient(ClientID))
				Dist[j].first = 1e8;
			else
				Dist[j].first = 0;

			Dist[j].first += distance(pPlayer->m_ViewPos, pBotPlayer->GetCharacter()->m_Pos);
		}

		// always send the player themselves
		Dist[ClientID].first = 0;

		// compute reverse map
		int aReverseMap[MAX_CLIENTS];
		memset(aReverseMap, -1, sizeof(int) * MAX_CLIENTS);
		for(int j = MAX_PLAYERS; j < VANILLA_MAX_CLIENTS; j++)
		{
			if(pMap[j] == -1)
				continue;

			if(Dist[pMap[j]].first > 5e9f)
				pMap[j] = -1;
			else
				aReverseMap[pMap[j]] = j;
		}

		std::nth_element(&Dist[MAX_PLAYERS], &Dist[VANILLA_MAX_CLIENTS - 1], &Dist[MAX_CLIENTS], distCompare);

		int Mapc = MAX_PLAYERS;
		int Demand = 0;
		for(int j = MAX_PLAYERS; j < VANILLA_MAX_CLIENTS - 1; j++)
		{
			int k = Dist[j].second;

			if(aReverseMap[k] != -1 || Dist[j].first > 5e9f)
				continue;

			while(Mapc < VANILLA_MAX_CLIENTS && pMap[Mapc] != -1)
				Mapc++;

			if(Mapc < VANILLA_MAX_CLIENTS - 1)
				pMap[Mapc] = k;
			else
				Demand++;
		}

		for(int j = MAX_CLIENTS - 1; j > VANILLA_MAX_CLIENTS - 2; j--)
		{
			int k = Dist[j].second;

			if(aReverseMap[k] != -1 && Demand-- > 0)
				pMap[aReverseMap[k]] = -1;
		}

		pMap[VANILLA_MAX_CLIENTS - 1] = -1; // player with empty name to say chat msgs
	}
}