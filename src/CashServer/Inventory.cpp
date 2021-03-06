/*
Copyright (C) 2008-2011 Vana Development Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#include "Inventory.h"
#include "Buffs.h"
#include "GameConstants.h"
#include "GameLogicUtilities.h"
#include "ItemDataProvider.h"
#include "Pet.h"
#include "PetsPacket.h"
#include "Player.h"
#include "PlayerPacket.h"
#include "PlayerMonsterBook.h"
#include "Randomizer.h"
#include "SkillDataProvider.h"

int16_t Inventory::addItem(Player *player, Item *item, bool is) {
	int8_t inv = GameLogicUtilities::getInventory(item->getId());
	int16_t freeslot = 0;
	for (int16_t s = 1; s <= player->getInventory()->getMaxSlots(inv); s++) {
		Item *olditem = player->getInventory()->getItem(inv, s);
		if (olditem == nullptr && !freeslot) {
			freeslot = s;
			if (GameLogicUtilities::isRechargeable(item->getId()) || GameLogicUtilities::isEquip(item->getId()) || GameLogicUtilities::isPet(item->getId()))
				break;
		}
	}
	if (freeslot != 0) {
		player->getInventory()->addItem(inv, freeslot, item);
		if (is) {
			Pet *pet = player->getPets()->getPet(item->getPetId());
			if (pet == nullptr) {
				pet = new Pet(player, item);
				player->getPets()->addPet(pet);
			}
			pet->setInventorySlot((int8_t) freeslot);
		}
		if (GameLogicUtilities::getInventory(item->getId()) == Inventories::CashInventory || GameLogicUtilities::getInventory(item->getId()) == Inventories::EquipInventory) {
			PlayerPacket::sendItemMovedToInventory(player, item, freeslot);
		}
		else {
			PlayerPacket::sendBoughtNonCashItem(player, item->getAmount(), freeslot, item->getId());
		}
	}
	return freeslot;
}

void Inventory::addNewItem(Player *player, int32_t itemid, int16_t amount) {
	if (!ItemDataProvider::Instance()->itemExists(itemid))
		return;

	int16_t max = ItemDataProvider::Instance()->getMaxSlot(itemid);
	int16_t thisamount = 0;
	if (GameLogicUtilities::isRechargeable(itemid)) {
		thisamount = max + player->getSkills()->getRechargeableBonus();
		amount -= 1;
	}
	else if (GameLogicUtilities::isEquip(itemid) || GameLogicUtilities::isPet(itemid)) {
		thisamount = 1;
		amount -= 1;
	}
	else if (amount > max) {
		thisamount = max;
		amount -= max;
	}
	else {
		thisamount = amount;
		amount = 0;
	}

	Item *item = nullptr;
	if (GameLogicUtilities::isEquip(itemid)) {
		item = new Item(itemid, false);
		if (GameLogicUtilities::isMount(itemid))
			player->getMounts()->addMount(itemid);
	}
	else {
		item = new Item(itemid, thisamount);
	}
	if (addItem(player, item, GameLogicUtilities::isPet(itemid)) == 0 && amount > 0) {
		addNewItem(player, itemid, amount);
	}
}

void Inventory::takeItem(Player *player, int32_t itemid, uint16_t howmany) {
	player->getInventory()->changeItemAmount(itemid, -howmany);
	int8_t inv = GameLogicUtilities::getInventory(itemid);
	for (int16_t i = 1; i <= player->getInventory()->getMaxSlots(inv); i++) {
		Item *item = player->getInventory()->getItem(inv, i);
		if (item == nullptr)
			continue;
		if (item->getId() == itemid) {
			if (item->getAmount() >= howmany) {
				item->setAmount(howmany);
				if (item->getAmount() == 0 && !GameLogicUtilities::isRechargeable(item->getId())) {
					player->getInventory()->deleteItem(inv, i);
				}
				else {
				}
				break;
			}
			else if (!GameLogicUtilities::isRechargeable(item->getId())) {
				howmany -= item->getAmount();
				item->setAmount(0);
				player->getInventory()->deleteItem(inv, i);
			}
		}
	}
}

void Inventory::takeItemSlot(Player *player, int8_t inv, int16_t slot, int16_t amount, bool takeStar) {
	Item *item = player->getInventory()->getItem(inv, slot);
	if (item == nullptr || item->getAmount() - amount < 0)
		return;

	item->setAmount(item->getAmount() - amount);
	if ((item->getAmount() == 0 && !GameLogicUtilities::isRechargeable(item->getId())) || (takeStar && GameLogicUtilities::isRechargeable(item->getId()))) {
		player->getInventory()->deleteItem(inv, slot);
	}
	else {
		player->getInventory()->changeItemAmount(item->getId(), -amount);
	}
}

void Inventory::useItem(Player *player, int32_t itemid) {
	ConsumeInfo *item = ItemDataProvider::Instance()->getConsumeInfo(itemid);

	if (item == nullptr) {
		// No reason not to check
		return;
	}

	int16_t potency = 100;
	int32_t skillid = player->getSkills()->getAlchemist();

	if (player->getSkills()->getSkillLevel(skillid) > 0)
		potency = player->getSkills()->getSkillInfo(skillid)->x;

	bool zombie = player->getActiveBuffs()->isZombified();

	if (item->hp > 0)
		player->getStats()->modifyHp(item->hp * (zombie ? (potency / 2) : potency) / 100);
	if (item->mp > 0)
		player->getStats()->modifyMp(item->mp * potency / 100);
	else
		player->getStats()->setMp(player->getStats()->getMp(), true);
	if (item->hpr != 0)
		player->getStats()->modifyHp(item->hpr * (zombie ? (player->getStats()->getMaxHp() / 2) : player->getStats()->getMaxHp()) / 100);
	if (item->mpr != 0)
		player->getStats()->modifyMp(item->mpr * player->getStats()->getMaxMp() / 100);
	if (item->ailment > 0)
		player->getActiveBuffs()->useDebuffHealingItem(item->ailment);

	if (item->time > 0 && item->mcprob == 0) {
		int32_t time = item->time * potency / 100;
		Buffs::addBuff(player, itemid, time);
	}
	if (GameLogicUtilities::isMonsterCard(itemid)) {
		bool isFull = player->getMonsterBook()->addCard(itemid); // Has a special buff for being full?
		if (item->mcprob != 0 && Randomizer::Instance()->randShort(99) < item->mcprob) {
			Buffs::addBuff(player, itemid, item->time);
		}
	}
}
