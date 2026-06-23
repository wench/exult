/*
 *  Copyright (C) 2026  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef CHEAT_STRINGS_INCLUDED
#define CHEAT_STRINGS_INCLUDED
#include "items.h"
#include "schedule.h"

struct Strings : public StringsBase {
	static inline const String<0x900> SELECT = {};

	static inline const String<0x901> CURSOR = {};
	
	static inline const String<0x902> Exit= {};

	static inline const String<0X908> ESC = {};

	static inline const String<0X909> UNKNOWNKEYNAME = {};

	static inline const String<0X90A> TAB = {};

	static inline const String<0X90B> RET = {};

	static inline const stringString<0X910> ENTER_COMMAND = {};

	static inline const stringString<0X911> HIT_A_KEY = {};

	static inline const stringString<0X912> NOT_YET_AVAILABLE = {};

	static inline const stringString<0X913> INVALID_NPC = {};

	static inline const stringString<0X914> INVALID_OBJECT = {};

	static inline const stringString<0X915> INVALID_COMMAND = {};

	static inline const stringString<0X916> CANCELLED = {};

	static inline const stringString<0X917> INPUT_REQUIRED = {};

	static inline const stringString<0X918> INVALID_VALUE = {};

	static inline const stringString<0X919> WHICH_NPC = {};

	static inline const stringString<0X91A> ENTERNPCNUMBER = {};

	static inline const stringString<0X91B> TO_CANCEL = {};

	static inline const stringString<0X91C> CLOCK_SET = {};

	static inline const stringString<0X91D> INVALID_TIME = {};

	static inline const stringString<0X91E> INVALID_SHAPE = {};

	static inline const stringString<0X91F> ITEM_CREATED = {};

	static inline const stringString<0X920> SHAPE_SET = {};

	static inline const stringString<0X922> NAME_CHANGED = {};

	static inline const stringString<0X923> WRONG_SHAPE_FILE_MUST_BE_SHAPES_VGA = {};

	static inline const stringString<0X924> ENTER_VALUE = {};

	static inline const stringString<0X925> ENTER_MINUTE = {};

	static inline const stringString<0X926> ENTER_HOUR = {};

	static inline const stringString<0X927> ENTER_DAY = {};

	static inline const stringString<0X928> ENTER_SHAPE = {};

	static inline const stringString<0X929> ENTER_ACTIVITY = {};

	static inline const stringString<0X92A> ENTER_X_COORD = {};

	static inline const stringString<0X92B> ENTER_Y_COORD = {};

	static inline const stringString<0X92C> ENTER_Z_COORD = {};

	static inline const stringString<0X92D> ENTER_LIFT = {};

	static inline const stringString<0X92E> ENTER_GLOBAL_FLAG = {};

	static inline const stringString<0X92F> ENTER_NPC_FLAG = {};

	static inline const stringString<0X930> ENTER_TEMPERATURE = {};

	static inline const stringString<0X931> ENTER_LATITUDE = {};

	static inline const stringString<0X932> ENTER_LONGITUDE = {};

	static inline const stringString<0X934> ENTER_A_NEW_NAME = {};

	static inline const stringString<0X935> LATITUDE_N_ORTH_OR_S_OUTH = {};

	static inline const stringString<0X936> LONGITUDE_W_EST_OR_E_AST = {};

	static inline const String<0x937> Pick_Object_from_World = {};

	static inline const String<0x938> Pick_NPC_from_World = {};

	static inline const struct : public String<0x940, 35> {
		const char* operator[](unsigned offset) const override {
			// Special handling for returning the name of scripted schedules
			if (offset >= Schedule::first_scripted_schedule) {
				const char* scriptname = Schedule_change::get_script_name(offset);
				return scriptname ? scriptname : "";
			}

			return String::operator[](offset);
		}
	} ScheduleActivity = {};

	static inline const String<0x970, 64> NPCFlag = {};

	static inline const String<0x9B0, 4> Alignment = {};

	struct BusinessMenu {
		static inline const String<0x9b8> Set = {};

		static inline const String<0x9B9> Location = {};

		static inline const stringString<0x9BA> Clear = {};

		static inline const String<0x9BB> SetCurrentActivity = {};

		static inline const String<0x9BC> Revert = {};
	};

	// am and pm are taken from notebook gump
	static inline const String<0x659> am = {};

	static inline const String<0x65A> pm = {};

	struct RootMenu {
		static inline const String<0x9E0> Paperdolls   = {};
		static inline const String<0x9E1> GodMode      = {};
		static inline const String<0X9E2> WizardMode   = {};
		static inline const String<0X9E3> Infravision  = {};
		static inline const String<0X9E4> HackMover    = {};
		static inline const String<0X9E5> EggsVisible  = {};
		static inline const String<0X9E6> SetTime      = {};
		static inline const String<0X9E7> NPCTool      = {};
		static inline const String<0X9E8> FlagModifier = {};
		static inline const String<0X9E9> Teleport     = {};
		static inline const String<0X9EA> TimeRate     = {};
	};

	struct NPCMenu {
		static inline const String<0x9F0> BusinessActivity = {};
		static inline const String<0x9F1> ChangeShape      = {};
		static inline const String<0x9F2> Exprience        = {};
		static inline const String<0x9F3> Npcflags         = {};
		static inline const String<0x9F4> Name             = {};
		static inline const String<0x9F5> Stats            = {};
		static inline const String<0x9F6> TrainingPoints   = {};
		static inline const String<0x9F7> TeleporttoNPC    = {};
		static inline const String<0x9F8> PaletteEffect    = {};
		static inline const String<0x9F9> WalktoAvatar     = {};
		static inline const String<0x9FA> ChangeNPC        = {};
		static inline const String<0x9FB> ScrollNPCs       = {};
	};

	struct NPCStatsMenu {
		static inline const String<0xA00> Dexterity    = {};
		static inline const String<0xA01> FoodLevel    = {};
		static inline const String<0xA02> Intellicence = {};
		static inline const String<0xA03> Strength     = {};
		static inline const String<0xA04> CombatSkill  = {};
		static inline const String<0xA05> HitPoints    = {};
		static inline const String<0xA06> MagicPoints  = {};
		static inline const String<0xA07> ManaLevel    = {};
	};
};

#endif    // CHEAT_STRINGS_INCLUDED