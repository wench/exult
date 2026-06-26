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
	static inline const String<0x8F0> SELECT = {};

	static inline const String<0x8F1> CURSOR = {};

	static inline const String<0x8F2> Exit = {};

	static inline const String<0x8F3> AdvancedOptionCheatScreen = {};
	static inline const String<0x8F4> Running                   = {};
	static inline const String<0x8F5> CurrentTime               = {};
	static inline const String<0x8F6> Day                       = {};
	static inline const String<0x8F7> Coordinates               = {};
	static inline const String<0x8F8> North                     = {};
	static inline const String<0x8F8> South                     = {};
	static inline const String<0x8FA> Map                       = {};
	static inline const String<0x8FB> West                      = {};
	static inline const String<0x8FC> East                      = {};
	static inline const String<0x8FD> Coords_hex                = {};
	static inline const String<0x8FE> Coords_dec                = {};
	static inline const String<0x8FF> Yes                       = {};
	static inline const String<0x900> No                        = {};
	static inline const String<0x901> On                        = {};
	static inline const String<0x902> Off                       = {};

	static inline const String<0X90C> ESC = {};

	static inline const String<0X90D> UNKNOWNKEYNAME = {};

	static inline const String<0X90E> TAB = {};

	static inline const String<0X90F> RET = {};

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

	static inline const String<0x939> Browse      = {};
	static inline const String<0x93A> ENTER_FRAME = {};

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

	static inline const stringviewString<0x970, 64> NPCFlag = {};

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
		static inline const String<0x9FC> NPC_             = {};
		static inline const String<0x9FD> Loc_             = {};
		static inline const String<0x9FE> Shape_           = {};
		static inline const String<0x9FF> CurrentActivity_ = {};
		static inline const String<0xA00> Level_           = {};
		static inline const String<0xA01> Health_          = {};
		static inline const String<0xA02> Training_        = {};
		static inline const String<0xA03> Expriemce_       = {};
		static inline const String<0xA04> unused           = {};
		static inline const String<0xA05> Met              = {};
		static inline const String<0xA06> NotMet           = {};
		static inline const String<0xA07> Usecodeitem      = {};
		static inline const String<0xA08> function         = {};
		static inline const String<0xA09> Usecodefunction  = {};
		static inline const String<0xA0A> Alignment        = {};
		static inline const String<0xA0B> orig             = {};
		static inline const String<0xA0C> Polymorphedfrom  = {};
		static inline const String<0xA0D> InvalidNPC       = {};
	};

	struct NPCStatsMenu {
		static inline const String<0xA10> Dexterity    = {};
		static inline const String<0xA11> FoodLevel    = {};
		static inline const String<0xA12> Intellicence = {};
		static inline const String<0xA13> Strength     = {};
		static inline const String<0xA14> CombatSkill  = {};
		static inline const String<0xA15> HitPoints    = {};
		static inline const String<0xA16> MagicPoints  = {};
		static inline const String<0xA17> ManaLevel    = {};
	};

	static inline const String<0xA18, 30> NPCFlagMenuItems;

	struct AdvancedFlags {
		static inline const String<0xA36>       FlagIsSET   = {};
		static inline const String<0xA37>       FlagIsUNSET = {};
		static inline const String<0xA38>       ToggleFlag  = {};
		static inline const stringString<0xA39> NPCFlag     = {};
		static inline const stringString<0xA3A> GlobalFlag  = {};
		static inline const String<0xA3B>       unnamed     = {};
		static inline const stringString<0xA3C> ChangeFlag  = {};
		static inline const stringString<0xA3D> ScrollFlags = {};
		static inline const String<0xA3E>       GlobalFlags = {};
	};

	struct TeleportMenu {
		static inline const String<0xA40> GeographicCoordinates;
		static inline const String<0xA41> TileCoordinates;

		static inline const String<0xA42> NPCNumber;
		static inline const String<0xA43> MapNumber;

		static inline const stringString<0x434> Latitude;
		static inline const String<0xA45>       NorthOr;
		static inline const String<0xA46>       South;
		static inline const stringString<0xA47> Longitude;
		static inline const String<0xA48>       WestOr;
		static inline const String<0xA49>       East;
		static inline const String<0xA4A>       Teleport_Menu;
		static inline const String<0xA4B>       Dangerous   = {};
		static inline const String<0xA4C>       UseWithCare = {};
		static inline const String<0xA4D>       Of          = {};
		static inline const String<0xA4E>       OnMap       = {};
		static inline const String<0xA4F>       Coords      = {};
	};

	struct PaletteEffect {
		struct MenuItems {
			;
			static inline const String<0xA50> RampRemap;
			static inline const String<0xA51> Xform;
			static inline const String<0xA52> Shift;
			static inline const String<0xA53> Clear;
		};

		struct Display {
			static inline const String<0xA54> PaletteEffect;
			static inline const String<0xA55> None;
			static inline const String<0xA56> RampRemapAllTo;
			static inline const String<0xA57> RampRemap;
			static inline const String<0xA58> To;
			static inline const String<0xA59> Xform;
			static inline const String<0xA5A> ShiftBy;
			static inline const String<0xA5B> FromRampall;
			static inline const String<0xA5C> FromRamp;
		};

		struct Prompts {
			static inline const stringString<0xA5D> enterFromRamp;
			static inline const stringString<0xA5E> or255forall;
			static inline const stringString<0xA5F> enterXFORMIndex;
			static inline const stringString<0xA60> entershiftamount;
			static inline const stringString<0xA61> enterToRampnumberIndex;
		};
	};
};

#endif    // CHEAT_STRINGS_INCLUDED