#include "hat.h"
#include "paths.h"
#include "log.h"
#include "../inc/SimpleIni.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <string>

namespace {
	constexpr int WATCH_INTERVAL_MS = 400;
	constexpr int EQUIP_CONFIRM_TIMEOUT_MS = 1500;
	constexpr int CATCH_TIMEOUT_MS = 250;
	constexpr int REMAKE_RETRY_AFTER_MS = 1500;

	constexpr float REMADE_HAT_HOLD_DISTANCE = 100.0f; // Hold and freeze the hat when beyond this distance so it can't get cleaned up. 
	constexpr float REMADE_HAT_RELEASE_DISTANCE = 50.0f; // Unfreeze when we get close so it can be kicked and tumble again.
	bool remadeHatBeingHeld = true;

	int nextHatPeek = 0;
	int remakeNextAttemptAfter = 0;

	Blip hatBlip = 0;
	Vector3 hatBlipCoords{};

	//Rebuild lost hats by equipping a new copy, bumping it off and moving it to the saved lost location.
	enum class RemakeStep { Equip, KnockOff, Catch };
	bool remakingHat = false;
	RemakeStep remakeStep = RemakeStep::Equip;
	int remakeStepStartTime = 0;
	Object remadeHat = 0;
	Object prevDroppedHat = 0;

	struct HatData {
		bool onHead = false;
		Hash hatHash = 0;
		Vector3 LostLocation{};
		Vector3 LostRotation{};
	};

	HatData theHat;

	std::filesystem::path SaveFile() {
		return std::filesystem::path(ModDataDir()) / "HatSave.ini";
	}

	Hash GetCurrentHat(Ped ped) {
		if (!PED::_IS_META_PED_USING_COMPONENT(ped, MISC::GET_HASH_KEY("hats"))) {
			return 0;
		}

		int componentCount = PED::_GET_NUM_COMPONENTS_IN_PED(ped);
		for (int i = 0; i < componentCount; i++) {
			if (PED::_GET_CATEGORY_OF_COMPONENT_AT_INDEX(ped, i, 0) != MISC::GET_HASH_KEY("hats")) {
				continue;
			}

			// I guess these have to be passed in via reference or else the native returns a different hash.
			BOOL statusFlag = FALSE;
			Hash wearableState = 0;
			return PED::_GET_SHOP_ITEM_COMPONENT_AT_INDEX(ped, i, FALSE, &statusFlag, &wearableState);
		}
		return 0;
	}

	void SetHatPosition(Entity hat) {
		theHat.LostLocation = ENTITY::GET_ENTITY_COORDS(hat, TRUE, FALSE);
		theHat.LostRotation = ENTITY::GET_ENTITY_ROTATION(hat, 2);
	}

	bool RemadeHatObjectExists() {
		return remadeHat != 0 && ENTITY::DOES_ENTITY_EXIST(remadeHat);
	}

	bool EquipConfirmedOrTimedOut(Ped player, Hash wantedHat, int stepStartTime, int now) {
		Hash currentHat = GetCurrentHat(player);

		if (currentHat != wantedHat) {
			return false;
		}

		if (PED::IS_PED_READY_TO_RENDER(player)) {
			return true;
		}

		if ((now - stepStartTime) > EQUIP_CONFIRM_TIMEOUT_MS) {
			return true;
		}

		return false; // Still waiting
	}

	void StartRemakingHat(int now) {
		remakingHat = true;
		remakeStep = RemakeStep::Equip;
		remakeStepStartTime = now;
		Log::Write("StartRemakingHat() Invoked");
	}

	void PlaceRemadeHat(Object hat) {
		ENTITY::SET_ENTITY_AS_MISSION_ENTITY(hat, TRUE, FALSE);
		ENTITY::SET_ENTITY_COORDS(hat, theHat.LostLocation.x, theHat.LostLocation.y, theHat.LostLocation.z, FALSE, FALSE, FALSE, FALSE);
		ENTITY::SET_ENTITY_ROTATION(hat, theHat.LostRotation.x, theHat.LostRotation.y, theHat.LostRotation.z, 2, TRUE);
		//Unfreeze later when player is close.
		ENTITY::FREEZE_ENTITY_POSITION(hat, TRUE);
	}

	void AttemptRemake(Ped player, int now) {
		switch (remakeStep) {
		case RemakeStep::Equip:
			PED::_APPLY_SHOP_ITEM_TO_PED(player, theHat.hatHash, TRUE, FALSE, FALSE);
			PED::_UPDATE_PED_VARIATION(player, FALSE, TRUE, TRUE, TRUE, FALSE);
			remakeStep = RemakeStep::KnockOff;
			remakeStepStartTime = now;
			return;

		case RemakeStep::KnockOff:
			if (!EquipConfirmedOrTimedOut(player, theHat.hatHash, remakeStepStartTime, now)) {
				return;
			}
			Log::Write("hat.cpp: Attempting knock off");
			prevDroppedHat = PED::_GET_PED_LAST_DROPPED_HAT(player);
			PED::KNOCK_OFF_PED_PROP(player, FALSE, TRUE, FALSE, TRUE);
			remakeStep = RemakeStep::Catch;
			remakeStepStartTime = now;
			return;

		case RemakeStep::Catch:
			Object dropped = PED::_GET_PED_LAST_DROPPED_HAT(player);
			if (!dropped || dropped == prevDroppedHat || !ENTITY::DOES_ENTITY_EXIST(dropped)) {
				if (now - remakeStepStartTime > CATCH_TIMEOUT_MS) {
					remakingHat = false;
					remakeNextAttemptAfter = now + REMAKE_RETRY_AFTER_MS;
					Log::Write("Hat.cpp: Remake attempt failed. Trying again later");
				}
				return;
			}
			remadeHat = dropped;
			PlaceRemadeHat(remadeHat);
			remakingHat = false;
			Log::Write("hat.cpp: Remade hat caught and placed");
			return;
		}
	}

	int nextDistanceReportTime = 0;

	void KeepRemadeHat(Ped player) {
		Vector3 hatLoc = ENTITY::GET_ENTITY_COORDS(remadeHat, TRUE, FALSE);
		Vector3 playerLoc = ENTITY::GET_ENTITY_COORDS(player, TRUE, FALSE);
		float distance = MISC::GET_DISTANCE_BETWEEN_COORDS(playerLoc.x, playerLoc.y, playerLoc.z, hatLoc.x, hatLoc.y, hatLoc.z, TRUE);

		if (MISC::GET_GAME_TIMER() > nextDistanceReportTime) {
			Log::Write(std::format("Distance from hat: {:.2f}", distance));
			nextDistanceReportTime = MISC::GET_GAME_TIMER() + 2000;
		}

		if (!remadeHatBeingHeld && distance > REMADE_HAT_HOLD_DISTANCE) {
			ENTITY::SET_ENTITY_AS_MISSION_ENTITY(remadeHat, TRUE, FALSE);
			ENTITY::FREEZE_ENTITY_POSITION(remadeHat, TRUE);
			remadeHatBeingHeld = true;
		}
		else if (remadeHatBeingHeld && distance < REMADE_HAT_RELEASE_DISTANCE) {
			ENTITY::FREEZE_ENTITY_POSITION(remadeHat, FALSE);
			Object releasedHat = remadeHat;
			ENTITY::SET_ENTITY_AS_NO_LONGER_NEEDED(&releasedHat);
			remadeHatBeingHeld = false;
		}
	}

	void HandleHatBlip() {
		if (theHat.onHead || theHat.hatHash == 0) {
			if (hatBlip && MAP::DOES_BLIP_EXIST(hatBlip)) {
				MAP::REMOVE_BLIP(&hatBlip);
			}
			hatBlip = 0;
			return;
		}

		if (!hatBlip || !MAP::DOES_BLIP_EXIST(hatBlip)) {
			hatBlip = MAP::BLIP_ADD_FOR_COORDS(MISC::GET_HASH_KEY("BLIP_STYLE_PLAYER_HAT_BLIP"), theHat.LostLocation.x, theHat.LostLocation.y, theHat.LostLocation.z);
			MAP::BLIP_ADD_MODIFIER(hatBlip, MISC::GET_HASH_KEY("BLIP_MODIFIER_RADAR_EDGE_ALWAYS"));
			return;
		}

		MAP::SET_BLIP_COORDS(hatBlip, theHat.LostLocation.x, theHat.LostLocation.y, theHat.LostLocation.z);
	}

}


void Hat::Load() {
	CSimpleIni ini;
	if (ini.LoadFile(SaveFile().string().c_str()) < 0) {
		return;
	}

	theHat.onHead = ini.GetBoolValue("Hat", "OnHead", false);
	theHat.hatHash = static_cast<Hash>(std::strtoul(ini.GetValue("Hat", "HatHash", "0"), nullptr, 16));
	theHat.LostLocation.x = static_cast<float>(ini.GetDoubleValue("Hat", "X", 0L));
	theHat.LostLocation.y = static_cast<float>(ini.GetDoubleValue("Hat", "Y", 0L));
	theHat.LostLocation.z = static_cast<float>(ini.GetDoubleValue("Hat", "Z", 0L));
	theHat.LostRotation.x = static_cast<float>(ini.GetDoubleValue("Hat", "Rot_X", 0L));
	theHat.LostRotation.y = static_cast<float>(ini.GetDoubleValue("Hat", "Rot_Y", 0L));
	theHat.LostRotation.z = static_cast<float>(ini.GetDoubleValue("Hat", "Rot_Z", 0L));

};

void Hat::Save() {
	CSimpleIni ini;
	ini.SetBoolValue("Hat", "OnHead", theHat.onHead);
	ini.SetValue("Hat", "HatHash", std::format("0x{:08X}", theHat.hatHash).c_str());

	ini.SetDoubleValue("Hat", "X", theHat.LostLocation.x);
	ini.SetDoubleValue("Hat", "Y", theHat.LostLocation.y);
	ini.SetDoubleValue("Hat", "Z", theHat.LostLocation.z);

	//Sure. Keep the rotation as well. Why not.
	ini.SetDoubleValue("Hat", "Rot_X", theHat.LostRotation.x);
	ini.SetDoubleValue("Hat", "Rot_Y", theHat.LostRotation.y);
	ini.SetDoubleValue("Hat", "Rot_Z", theHat.LostRotation.z);

	ini.SaveFile(SaveFile().string().c_str(), FALSE);
}

void Hat::WatchThatHat(int now) {

	Ped player = PLAYER::PLAYER_PED_ID();
	if (remakingHat) {
		if (ENTITY::IS_ENTITY_DEAD(player)) {
			remakingHat = false;
			remakeNextAttemptAfter = now + REMAKE_RETRY_AFTER_MS;
			Log::Write("hat.cpp: Dead player mid remake: Aborting.");
			return;
		}
		AttemptRemake(player, now);
		return;
	}

	if (now < nextHatPeek) {
		return;
	}
	nextHatPeek = now + WATCH_INTERVAL_MS;

	Hash hatOnHeadHash = GetCurrentHat(player);

	if (hatOnHeadHash != 0) {
		//A new hat touches the beacon. (Or they same old hat)
		if (!theHat.onHead) {
			Log::Write(std::format("Hat: detected on head (hash 0x{:08X}), not via remake", hatOnHeadHash));
		}
		theHat.hatHash = hatOnHeadHash;
		theHat.onHead = TRUE;
	}
	else if (theHat.hatHash != 0) {
		if (RemadeHatObjectExists()) {
			KeepRemadeHat(player);
		}
		else {
			//Hat knocked off. Start tracking position.
			Object droppedHat = PED::_GET_PED_LAST_DROPPED_HAT(player);
			if (droppedHat && ENTITY::DOES_ENTITY_EXIST(droppedHat)) {
				theHat.onHead = FALSE;
				theHat.hatHash = PED::_GET_SHOP_ITEM_HAT_COMPONENT(droppedHat, 0, FALSE);
				SetHatPosition(droppedHat);
				Log::Write(std::format("Hat: natural drop still tracked, handle {}", droppedHat));
			}
			else if (!RemadeHatObjectExists() && now >= remakeNextAttemptAfter) {
				StartRemakingHat(now);
			}
		}
	}

	HandleHatBlip();
	Hat::Save();
};