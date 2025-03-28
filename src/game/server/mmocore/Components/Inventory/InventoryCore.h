/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_INVENTORY_CORE_H
#define GAME_SERVER_COMPONENT_INVENTORY_CORE_H
#include <game/server/mmocore/MmoComponent.h>

#include "ItemData.h"

class CInventoryCore : public MmoComponent
{
	~CInventoryCore() override
	{
		CAttributeDescription::Data().clear();
		CItemDescription::Data().clear();
		CPlayerItem::Data().clear();
	}

	void OnInit() override;
	void OnInitAccount(class CPlayer* pPlayer) override;
	void OnResetClient(int ClientID) override;
	bool OnHandleVoteCommands(class CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;
	bool OnHandleMenulist(class CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;

	int SecureCheck(class CPlayer *pPlayer, ItemIdentifier ItemID, int Value, int Settings, int Enchant);
	int DeSecureCheck(class CPlayer *pPlayer, ItemIdentifier ItemID, int Value, int Settings);

public:
	// primary
	void ListInventory(int ClientID, ItemType Type);
	void ListInventory(int ClientID, ItemFunctional Type);
	void ItemSelected(class CPlayer* pPlayer, const CPlayerItem& pItemPlayer, bool Dress = false);
	int GetUnfrozenItemValue(class CPlayer* pPlayer, ItemIdentifier ItemID) const;

	void RepairDurabilityItems(class CPlayer *pPlayer);
	int GetCountItemsType(class CPlayer* pPlayer, ItemType Type) const;

	void AddItemSleep(int AccountID, ItemIdentifier ItemID, int Value, int Milliseconds);
	int GiveItem(class CPlayer* pPlayer, ItemIdentifier ItemID, int Value, int Settings, int Enchant);
	int RemoveItem(class CPlayer* pPlayer, ItemIdentifier ItemID, int Value, int Settings);
};

#endif