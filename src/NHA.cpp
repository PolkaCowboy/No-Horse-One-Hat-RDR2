#include "NHA.h"
#include "log.h"

#include <format>

namespace {
	constexpr int HORSECHECK_INETRVAL_MS = 250;
	int nextHorseCheck = 0;
}

void NHA::HideThatHorse(int now) {
	
	if (now < nextHorseCheck) {
		return;
	}
	nextHorseCheck = now + HORSECHECK_INETRVAL_MS;


	Ped horse = PLAYER::_GET_SADDLE_HORSE_FOR_PLAYER(PLAYER::PLAYER_ID());

	if (!horse || !ENTITY::DOES_ENTITY_EXIST(horse) || ENTITY::IS_ENTITY_DEAD(horse)) {
		Log::Write("No horse detected or horse is dead");
		return;
	}
	Ped player = PLAYER::PLAYER_PED_ID();
	if (PED::IS_PED_ON_MOUNT(player) && PED::GET_MOUNT(player) == horse){
		Log::Write("Player on horse");
		return;
	}

	TASK::CLEAR_PED_TASKS_IMMEDIATELY(horse, FALSE, TRUE);
	//Center of map. Undergroung south of Valentine
	ENTITY::SET_ENTITY_COORDS(horse, 0.0f, 0.0f, 0.0f, FALSE, TRUE, TRUE, TRUE);
	ENTITY::FREEZE_ENTITY_POSITION(horse, TRUE);
}

//Own function else the blip flickers at the interval rate.
void NHA::HideThatBlip() {
	MISC::SET_THIS_SCRIPT_CAN_REMOVE_BLIPS_CREATED_BY_ANY_SCRIPT(TRUE);
	Ped horse = PLAYER::_GET_SADDLE_HORSE_FOR_PLAYER(PLAYER::PLAYER_ID());
	Blip blip = MAP::GET_BLIP_FROM_ENTITY(horse);
	Log::Write(std::format("blip handle: {}", blip));
	MAP::_CLEAR_BLIP(blip);
	MAP::REMOVE_BLIP(&blip);
}
