/*
 *  Copyright (C) 2000-2026  The Exult Team
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "actors.h"
#include "cheat.h"
#include "cheat_screen.h"
#include "cheat_screen_strings.h"
#include "font.h"
#include "game.h"
#include "gamemap.h"
#include "gamewin.h"
#include "ignore_unused_variable_warning.h"
#include "miscinf.h"
#include "party.h"
#include "schedule.h"
#include "vgafile.h"

#include <algorithm>
#include <cstring>



CheatScreen::InputHandlers::NPC::NPC(bool empty_allowed, std::string&& promptmsg)
		: GameObject(empty_allowed, std::move(promptmsg)) {
	hotspots.clear();
	const char* label = Strings::Pick_NPC_from_World;
	hotspots.emplace_back(0, 0, label[0], label + 1);
}

CheatScreen::InputHandlers::NPC::NPC(bool empty_allowed) : NPC(empty_allowed, Strings::WHICH_NPC) {}

void CheatScreen::InputHandlers::NPC::Parse() {
	if (!actor) {
		GameObject::Parse();
		actor = dynamic_cast<Actor*>(object);
	}

	if (!actor) {
		throw MenuCommandException{Strings::INVALID_NPC, false};
	}
}

//
// DISPLAYS
//

//
// Activity Display
//

void CheatScreen::ActivityDisplay() {
	char buf[512];
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsety1 = 7;
#else
	const int offsety1 = 9;
#endif

	for (int i = 0; i < 11; i++) {
		snprintf(buf, sizeof(buf), "%2i %s", i, Strings::ScheduleActivity[i]);
		font->paint_text_fixedwidth(ibuf, buf, 0, i * offsety1, 8, fontcolor.colors);

		snprintf(buf, sizeof(buf), "%2i %s", i + 11, Strings::ScheduleActivity[i + 11]);
		font->paint_text_fixedwidth(ibuf, buf, 112, i * offsety1, 8, fontcolor.colors);

		if (i != 10) {
			snprintf(buf, sizeof(buf), "%2i %s", i + 22, Strings::ScheduleActivity[i + 22]);
			font->paint_text_fixedwidth(ibuf, buf, 224, i * offsety1, 8, fontcolor.colors);
		}
	}
}

//
// NPCs
//

CheatScreen::Cheat_Prompt CheatScreen::NPCLoop(int num) {
	Actor* actor;

	bool looping = true;

	ClearState clear(state);
	while (looping) {
		hotspots.clear();
		if (num == -1) {
			actor = grabbed;
		} else {
			actor = gwin->get_npc(num);
		}
		grabbed = actor;
		if (actor) {
			num = actor->get_npc_num();
		}

		gwin->clear_screen();

		// First the display
		NPCDisplay(actor, num);

		// Now the Menu Column
		NPCMenu(actor, num);

		// Finally the Prompt...
		SharedPrompt();
		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			NPCActivate(actor, num);
			state.activate = false;
			continue;
		}

		if (SharedInput()) {
			looping = NPCCheck(actor, num);
		}
	}
	return CP_Command;
}

void CheatScreen::NPCDisplay(Actor* actor, int& num) {
	char buf[512];
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 73;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
#endif
	if (actor) {
		const Tile_coord t = actor->get_tile();

		// Paint the actors shape
		Shape_frame* shape = actor->get_shape();
		if (shape) {
			actor->paint_shape(shape->get_xright() + 240, shape->get_yabove());
		}

		// Now the info
		const std::string namestr = actor->get_npc_name();
		snprintf(buf, sizeof(buf), "NPC %i - %s%s", num, namestr.c_str(), actor->is_unused() ? " (Unused)" : "");
		font->paint_text_fixedwidth(ibuf, buf, offsetx, 0, 8, fontcolor.colors);

		snprintf(buf, sizeof(buf), "Loc (%04i, %04i, %02i)", t.tx, t.ty, t.tz);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, 9, 8, fontcolor.colors);

		snprintf(
				buf, sizeof(buf), "Shape %04i:%02i  %s", actor->get_shapenum(), actor->get_framenum(),
				actor->get_flag(Obj_flags::met) ? "Met" : "Not Met");
		font->paint_text_fixedwidth(ibuf, buf, offsetx, 18, 8, fontcolor.colors);

		snprintf(
				buf, sizeof(buf), "Current Activity: %2i - %s", actor->get_schedule_type(),
				Strings::ScheduleActivity[actor->get_schedule_type()]);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 36, 8, fontcolor.colors);

		snprintf(buf, sizeof(buf), "Experience: %i", actor->get_property(Actor::exp));
		font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 45, 8, fontcolor.colors);

		snprintf(buf, sizeof(buf), "Level: %i", actor->get_level());
		font->paint_text_fixedwidth(ibuf, buf, offsetx + 144, offsety1 + 45, 8, fontcolor.colors);

		snprintf(
				buf, sizeof(buf), "Training: %2i  Health: %2i", actor->get_property(Actor::training),
				actor->get_property(Actor::health));
		font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 54, 8, fontcolor.colors);

		if (num != -1) {
			int ucitemnum = 0x10000 - num;
			if (!num) {
				ucitemnum = 0xfe9c;
			}
			snprintf(buf, sizeof(buf), "Usecode item %4x function %x", ucitemnum, actor->get_usecode());
			font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 63, 8, fontcolor.colors);
		} else {
			snprintf(buf, sizeof(buf), "Usecode function %x", actor->get_usecode());
			font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 63, 8, fontcolor.colors);
		}

		if (actor->get_flag(Obj_flags::charmed)) {
			snprintf(
					buf, sizeof(buf), "Alignment: %s (orig: %s)", Strings::Alignment[actor->get_effective_alignment()],
					Strings::Alignment[actor->get_alignment()]);
		} else {
			snprintf(buf, sizeof(buf), "Alignment: %s", Strings::Alignment[actor->get_alignment()]);
		}
		font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 72, 8, fontcolor.colors);

		if (actor->get_polymorph() != -1) {
			snprintf(buf, sizeof(buf), "Polymorphed from %04i", actor->get_polymorph());
			font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 81, 8, fontcolor.colors);
		}
	} else {
		snprintf(buf, sizeof(buf), "NPC %i - Invalid NPC!", num);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, 0, 8, fontcolor.colors);
	}
}

void CheatScreen::NPCMenu(Actor* actor, int& num) {
	ignore_unused_variable_warning(num);
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 74;
	const int offsetx2 = -145;
	const int offsety2 = 65;
	const int offsetx3 = 175;
	const int offsety3 = 63;
	const int offsety4 = 72;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	const int offsetx2 = 0;
	const int offsety2 = 0;
	const int offsetx3 = offsetx + 160;
	const int offsety3 = maxy - 45;
	const int offsety4 = maxy - 36;
#endif
	// Left Column

	if (actor) {
		// Business Activity
		AddMenuItem(offsetx, maxy - offsety1 - 99, SDLK_B, "usiness Activity");

		// Change Shape
		AddMenuItem(offsetx, maxy - offsety1 - 90, SDLK_C, "hange Shape");

		// XP
		AddMenuItem(offsetx, maxy - offsety1 - 81, SDLK_E, "xperience");

		// NPC Flags
		AddMenuItem(offsetx, maxy - offsety1 - 72, SDLK_N, "pc Flags");

		// Name
		AddMenuItem(offsetx, maxy - offsety1 - 63, SDLK_1, " Name");
	}

	SharedMenu();

	// Right Column

	if (actor) {
		// Stats
		AddMenuItem(offsetx + 160, maxy - offsety1 - 99, SDLK_S, "tats");

		// Training Points
		AddMenuItem(offsetx + 160, maxy - offsety1 - 90, SDLK_2, " Training Points");

		// Teleport
		AddMenuItem(offsetx + 160, maxy - offsety1 - 81, SDLK_T, "eleport to NPC");

		// Palette Effect
		AddMenuItem(offsetx + 160, maxy - offsety1 - 72, SDLK_P, "alette Effect");

		// Walk to Avatar
		AddMenuItem(offsetx2 + 160, maxy - offsety2 - 63, SDLK_W, "alk to Avatar");
	}

	// Change NPC

	AddMenuItem(offsetx3, offsety3, SDLK_UP, " Change NPC");

	// Scroll NPCs

	AddLeftRightMenuItem(offsetx3, offsety4, "Scroll NPCs", num > 0, num < gwin->get_num_npcs(), false, true);
}

void CheatScreen::NPCActivate(Actor* actor, int& num) {
	int       i       = std::atoi(state.input);
	const int nshapes = Shape_manager::get_instance()->get_shapes().get_num_shapes();

	state.SetMode(CP_Command, false);

	if (state.command == '<') {
		num--;
		if (num < 0) {
			num = 0;
		} else if (num >= 356 && num <= 359) {
			num = 355;
		}
	} else if (state.command == '>') {
		num++;
		if (num >= 356 && num <= 359) {
			num = 360;
		}
	} else if (state.command == '^') {    // Change NPC
		if (i < 0 || (i >= 356 && i <= 359)) {
			state.SetMode(CP_InvalidNPC, false);
		} else if (state.input[0]) {
			num = i;
		}
	} else if (actor) {
		switch (state.command) {
		case SDLK_B:    // Business
			BusinessLoop(actor);
			break;

		case SDLK_N:    // Npc flags
			FlagLoop(actor);
			break;

		case SDLK_S:    // stats
			StatLoop(actor);
			break;

		case SDLK_P:
			PalEffectLoop(actor);
			break;

		case SDLK_T:    // Teleport

			Game_window::get_instance()->teleport_party(actor->get_tile(), false, actor->get_map_num());
			break;

		case SDLK_W:    // Walk to Avatar
			actor->approach_another(Game_window::get_instance()->get_main_actor());
			break;

		case SDLK_E:    // Experience
			if (i < 0) {
				state.SetMode(CP_Canceled, false);
			} else {
				actor->set_property(Actor::exp, i);
			}
			break;

		case SDLK_2:    // Training Points
			if (i < 0) {
				state.SetMode(CP_Canceled, false);
			} else {
				actor->set_property(Actor::training, i);
			}
			break;

		case SDLK_C:                        // Change shape
			if (state.input[0] == 'b') {    // Browser
				int n;
				clear_buttons();    // Clear all button states before browser
				if (!cheat.get_browser_shape(i, n)) {
					state.SetMode(CP_WrongShapeFile);
					break;
				}
			}

			if (i < 0) {
				state.SetMode(CP_InvalidShape, false);
			} else if (i >= nshapes) {
				state.SetMode(CP_InvalidShape, false);
			} else if (state.input[0]) {
				if (actor->get_npc_num() != 0) {
					actor->set_shape(i);
				} else {
					actor->set_polymorph(i);
				}
				state.SetMode(CP_ShapeSet, false);
			}
			break;

		case SDLK_1:    // Name
			if (!std::strlen(state.input)) {
				state.SetMode(CP_Canceled, false);
			} else {
				actor->set_npc_name(state.input);
				state.SetMode(CP_NameSet, false);
			}
			break;

		default:
			break;
		}
	}
	std::memset(state.input, 0, sizeof(state.input));

	state.command = 0;
}

// Checks the state.input
bool CheatScreen::NPCCheck(Actor* actor, int& num) {
	ignore_unused_variable_warning(num);
	switch (state.command) {
		// Simple commands
	case SDLK_A:    // Attack mode
	case SDLK_B:    // BUsiness
	case SDLK_N:    // Npc flags
	case SDLK_D:    // pop weapon
	case SDLK_S:    // stats
	case SDLK_Z:    // Target
	case SDLK_T:    // Teleport
	case SDLK_W:    // Walk
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		if (!actor) {
			state.SetMode(CP_InvalidCom, false);
		} else {
			state.activate = true;
		}
		break;

		// Value entries
	case SDLK_E:    // Experience
	case SDLK_2:    // Training Points
		if (!actor) {
			state.SetMode(CP_InvalidCom, false);
		} else {
			state.SetMode(CP_EnterValue);
			state.val_min = 255;
		}
		break;

		// Palette Effect
	case 'p':
		if (!actor) {
			state.SetMode(CP_InvalidCom, false);
		} else {
			state.activate = true;
		}
		break;

		// Change shape
	case SDLK_C:
		if (!actor) {
			state.SetMode(CP_InvalidCom, false);
		} else {
			state.SetMode(CP_Shape);
			state.val_min = 0;
			state.val_max = Shape_manager::get_instance()->get_shapes().get_num_shapes() - 1;
		}
		break;

		// Name
	case SDLK_1:
		if (!actor) {
			state.SetMode(CP_InvalidCom, false);
		} else {
			state.SetMode(CP_Name);
		}
		break;

		// - NPC
	case SDLK_LEFT:
		state.command = '<';
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		state.activate = true;
		break;

		// + NPC
	case SDLK_RIGHT:
		state.command = '>';
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		state.activate = true;
		break;

		// * Change NPC
	case SDLK_UP:
		state.command  = '^';
		state.input[0] = 0;
		state.SetMode(CP_ChooseNPC);
		state.val_min = 0;
		state.val_max = gwin->get_num_npcs() - 1;
		break;

		// X and Escape leave
	case SDLK_ESCAPE:
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		return false;

	default:
		state.SetMode(CP_InvalidCom, false);
		if (!state.input[0]) {
			state.input[0] = (state.command < 128 ? state.command : 0);
		}
		state.command = 0;
		break;
	}

	return true;
}

//
// NPC Flags
//

void CheatScreen::FlagLoop(Actor* actor) {
#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(CHEAT_SCREEN_TEST_MOBILE)
	int num = actor->get_npc_num();
#endif
	bool looping = true;

	ClearState clear(state);
	while (looping) {
		hotspots.clear();
		gwin->clear_screen();

#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(CHEAT_SCREEN_TEST_MOBILE)
		// First the display
		NPCDisplay(actor, num);
#endif

		// Now the Menu Column
		FlagMenu(actor);

		// Finally the Prompt...
		SharedPrompt();

		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			FlagActivate(actor);
			state.activate = false;
			continue;
		}

		if (SharedInput()) {
			looping = FlagCheck(actor);
		}
	}
}

void CheatScreen::FlagMenu(Actor* actor) {
	char buf[512];
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx  = 10;
	const int offsetx1 = 6;
	const int offsety1 = 92;
#else
	const int offsetx  = 0;
	const int offsetx1 = 0;
	const int offsety1 = 0;
#endif

	// Left Column

	// Asleep
	snprintf(buf, sizeof(buf), " Asleep.%c", actor->get_flag(Obj_flags::asleep) ? 'Y' : 'N');
	AddMenuItem(offsetx, maxy - offsety1 - 108, SDLK_A, buf);

	// Charmed
	snprintf(buf, sizeof(buf), " Charmd.%c", actor->get_flag(Obj_flags::charmed) ? 'Y' : 'N');

	AddMenuItem(offsetx, maxy - offsety1 - 99, SDLK_B, buf);

	// Cursed
	snprintf(buf, sizeof(buf), " Cursed.%c", actor->get_flag(Obj_flags::cursed) ? 'Y' : 'N');
	AddMenuItem(offsetx, maxy - offsety1 - 90, SDLK_C, buf);

	// Paralyzed
	snprintf(buf, sizeof(buf), " Prlyzd.%c", actor->get_flag(Obj_flags::paralyzed) ? 'Y' : 'N');
	AddMenuItem(offsetx, maxy - offsety1 - 81, SDLK_D, buf);

	// Poisoned
	snprintf(buf, sizeof(buf), " Poisnd.%c", actor->get_flag(Obj_flags::poisoned) ? 'Y' : 'N');
	AddMenuItem(offsetx, maxy - offsety1 - 72, SDLK_E, buf);

	// Protected
	snprintf(buf, sizeof(buf), " Prtctd.%c", actor->get_flag(Obj_flags::protection) ? 'Y' : 'N');
	AddMenuItem(offsetx, maxy - offsety1 - 63, SDLK_F, buf);

	// Tournament (Original is SI only -- allowing for BG in Exult)
	snprintf(buf, sizeof(buf), " Tourna.%c", actor->get_flag(Obj_flags::tournament) ? 'Y' : 'N');
	AddMenuItem(offsetx, maxy - offsety1 - 54, SDLK_G, buf);

	// Polymorph
	snprintf(buf, sizeof(buf), " Polymo.%c", actor->get_flag(Obj_flags::polymorph) ? 'Y' : 'N');
	AddMenuItem(offsetx, maxy - offsety1 - 45, SDLK_H, buf);
	// Advanced Editor

	AddMenuItem(offsetx, maxy - offsety1 - 36, SDLK_UP, "Advanced");

	SharedMenu();

	// Center Column

	// Party
	snprintf(buf, sizeof(buf), " Party..%c", actor->get_flag(Obj_flags::in_party) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 104, maxy - offsety1 - 108, SDLK_I, buf);

	// Invisible
	snprintf(buf, sizeof(buf), " Invsbl.%c", actor->get_flag(Obj_flags::invisible) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 104, maxy - offsety1 - 99, SDLK_J, buf);

	// Fly
	snprintf(buf, sizeof(buf), " Fly....%c", actor->get_type_flag(Actor::tf_fly) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 104, maxy - offsety1 - 90, SDLK_K, buf);

	// Walk
	snprintf(buf, sizeof(buf), " Walk...%c", actor->get_type_flag(Actor::tf_walk) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 104, maxy - offsety1 - 81, SDLK_L, buf);

	// Swim
	snprintf(buf, sizeof(buf), " Swim...%c", actor->get_type_flag(Actor::tf_swim) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 104, maxy - offsety1 - 72, SDLK_M, buf);

	// Ethereal
	snprintf(buf, sizeof(buf), " Ethrel.%c", actor->get_type_flag(Actor::tf_ethereal) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 104, maxy - offsety1 - 63, SDLK_N, buf);

	// Protectee
	snprintf(buf, sizeof(buf), " Prtcee.%c", '?');
	AddMenuItem(offsetx1 + 104, maxy - offsety1 - 54, SDLK_O, buf);

	// Conjured
	snprintf(buf, sizeof(buf), " Conjrd.%c", actor->get_type_flag(Actor::tf_conjured) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 104, maxy - offsety1 - 45, SDLK_P, buf);

	// Naked (AV ONLY)
	if (!actor->get_npc_num()) {
		snprintf(buf, sizeof(buf), " Naked..%c", actor->get_flag(Obj_flags::naked) ? 'Y' : 'N');
		AddMenuItem(offsetx1 + 104, maxy - offsety1 - 36, SDLK_7, buf);
	}

	// Right Column

	// Summoned
	snprintf(buf, sizeof(buf), " Summnd.%c", actor->get_type_flag(Actor::tf_summonned) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 208, maxy - offsety1 - 108, SDLK_Q, buf);

	// Bleeding
	snprintf(buf, sizeof(buf), " Bleedn.%c", actor->get_type_flag(Actor::tf_bleeding) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 208, maxy - offsety1 - 99, SDLK_R, buf);

	if (!actor->get_npc_num()) {    // Avatar
		// Sex
		snprintf(buf, sizeof(buf), " Sex....%c", actor->get_type_flag(Actor::tf_sex) ? 'F' : 'M');
		AddMenuItem(offsetx1 + 208, maxy - offsety1 - 90, SDLK_S, buf);

		// Skin
		snprintf(buf, sizeof(buf), " Skin...%d", actor->get_skin_color());
		AddMenuItem(offsetx1 + 208, maxy - offsety1 - 81, SDLK_1, buf);

		// Read
		snprintf(buf, sizeof(buf), " Read...%c", actor->get_flag(Obj_flags::read) ? 'Y' : 'N');
		AddMenuItem(offsetx1 + 208, maxy - offsety1 - 72, SDLK_4, buf);
	} else {    // Not Avatar
		// Met
		snprintf(buf, sizeof(buf), " Met....%c", actor->get_flag(Obj_flags::met) ? 'Y' : 'N');
		AddMenuItem(offsetx1 + 208, maxy - offsety1 - 90, SDLK_T, buf);

		// NoCast
		snprintf(buf, sizeof(buf), " NoCast.%c", actor->get_flag(Obj_flags::no_spell_casting) ? 'Y' : 'N');
		AddMenuItem(offsetx1 + 208, maxy - offsety1 - 81, SDLK_U, buf);

		// ID
		snprintf(buf, sizeof(buf), " ID#:%02i", actor->get_ident());
		AddMenuItem(offsetx1 + 208, maxy - offsety1 - 72, SDLK_V, buf);
	}

	// Freeze
	snprintf(buf, sizeof(buf), " Freeze.%c", actor->get_flag(Obj_flags::freeze) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 208, maxy - offsety1 - 63, SDLK_W, buf);

	// Party
	if (actor->is_in_party()) {
		// Temp
		snprintf(buf, sizeof(buf), " Temp: %02i", actor->get_temperature());
		AddMenuItem(offsetx1 + 208, maxy - offsety1 - 54, SDLK_Y, buf);

		// Warmth
		snprintf(buf, sizeof(buf), "Warmth: %04i", actor->figure_warmth());
		font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 208, maxy - offsety1 - 45, 8, fontcolor.colors);
	}

	// Petra (AV SI ONLY)
	if (!actor->get_npc_num()) {
		snprintf(buf, sizeof(buf), " Petra..%c", actor->get_flag(Obj_flags::petra) ? 'Y' : 'N');
		AddMenuItem(offsetx1 + 208, maxy - offsety1 - 36, SDLK_5, buf);
	}
}

void CheatScreen::FlagActivate(Actor* actor) {
	int       i       = std::atoi(state.input);
	const int nshapes = Shape_manager::get_instance()->get_shapes().get_num_shapes();

	state.SetMode(CP_Command, false);
	switch (state.command) {
		// Everyone

		// Toggles
	case SDLK_A:    // Asleep
		if (actor->get_flag(Obj_flags::asleep)) {
			actor->clear_flag(Obj_flags::asleep);
		} else {
			actor->set_flag(Obj_flags::asleep);
		}
		break;

	case SDLK_B:    // Charmed
		if (actor->get_flag(Obj_flags::charmed)) {
			actor->clear_flag(Obj_flags::charmed);
		} else {
			actor->set_flag(Obj_flags::charmed);
		}
		break;

	case SDLK_C:    // Cursed
		if (actor->get_flag(Obj_flags::cursed)) {
			actor->clear_flag(Obj_flags::cursed);
		} else {
			actor->set_flag(Obj_flags::cursed);
		}
		break;

	case SDLK_D:    // Paralyzed
		if (actor->get_flag(Obj_flags::paralyzed)) {
			actor->clear_flag(Obj_flags::paralyzed);
		} else {
			actor->set_flag(Obj_flags::paralyzed);
		}
		break;

	case SDLK_E:    // Poisoned
		if (actor->get_flag(Obj_flags::poisoned)) {
			actor->clear_flag(Obj_flags::poisoned);
		} else {
			actor->set_flag(Obj_flags::poisoned);
		}
		break;

	case SDLK_F:    // Protected
		if (actor->get_flag(Obj_flags::protection)) {
			actor->clear_flag(Obj_flags::protection);
		} else {
			actor->set_flag(Obj_flags::protection);
		}
		break;

	case SDLK_J:    // Invisible
		if (actor->get_flag(Obj_flags::invisible)) {
			actor->clear_flag(Obj_flags::invisible);
		} else {
			actor->set_flag(Obj_flags::invisible);
		}
		pal.apply();
		break;

	case SDLK_K:    // Fly
		if (actor->get_type_flag(Actor::tf_fly)) {
			actor->clear_type_flag(Actor::tf_fly);
		} else {
			actor->set_type_flag(Actor::tf_fly);
		}
		break;

	case SDLK_L:    // Walk
		if (actor->get_type_flag(Actor::tf_walk)) {
			actor->clear_type_flag(Actor::tf_walk);
		} else {
			actor->set_type_flag(Actor::tf_walk);
		}
		break;

	case SDLK_M:    // Swim
		if (actor->get_type_flag(Actor::tf_swim)) {
			actor->clear_type_flag(Actor::tf_swim);
		} else {
			actor->set_type_flag(Actor::tf_swim);
		}
		break;

	case SDLK_N:    // Ethrel
		if (actor->get_type_flag(Actor::tf_ethereal)) {
			actor->clear_type_flag(Actor::tf_ethereal);
		} else {
			actor->set_type_flag(Actor::tf_ethereal);
		}
		break;

	case SDLK_P:    // Conjured
		if (actor->get_type_flag(Actor::tf_conjured)) {
			actor->clear_type_flag(Actor::tf_conjured);
		} else {
			actor->set_type_flag(Actor::tf_conjured);
		}
		break;

	case SDLK_Q:    // Summoned
		if (actor->get_type_flag(Actor::tf_summonned)) {
			actor->clear_type_flag(Actor::tf_summonned);
		} else {
			actor->set_type_flag(Actor::tf_summonned);
		}
		break;

	case SDLK_R:    // Bleeding
		if (actor->get_type_flag(Actor::tf_bleeding)) {
			actor->clear_type_flag(Actor::tf_bleeding);
		} else {
			actor->set_type_flag(Actor::tf_bleeding);
		}
		break;

	case SDLK_S:    // Sex
		if (actor->get_type_flag(Actor::tf_sex)) {
			actor->clear_type_flag(Actor::tf_sex);
		} else {
			actor->set_type_flag(Actor::tf_sex);
		}
		break;

	case SDLK_4:    // Read
		if (actor->get_flag(Obj_flags::read)) {
			actor->clear_flag(Obj_flags::read);
		} else {
			actor->set_flag(Obj_flags::read);
		}
		break;

	case SDLK_5:    // Petra
		if (actor->get_flag(Obj_flags::petra)) {
			actor->clear_flag(Obj_flags::petra);
		} else {
			actor->set_flag(Obj_flags::petra);
		}
		break;

	case SDLK_7:    // Naked
		if (actor->get_flag(Obj_flags::naked)) {
			actor->clear_flag(Obj_flags::naked);
		} else {
			actor->set_flag(Obj_flags::naked);
		}
		break;

	case SDLK_T:    // Met
		if (actor->get_flag(Obj_flags::met)) {
			actor->clear_flag(Obj_flags::met);
		} else {
			actor->set_flag(Obj_flags::met);
		}
		break;

	case SDLK_U:    // No Cast
		if (actor->get_flag(Obj_flags::no_spell_casting)) {
			actor->clear_flag(Obj_flags::no_spell_casting);
		} else {
			actor->set_flag(Obj_flags::no_spell_casting);
		}
		break;

	case SDLK_Z:    // Zombie
		if (actor->get_flag(Obj_flags::si_zombie)) {
			actor->clear_flag(Obj_flags::si_zombie);
		} else {
			actor->set_flag(Obj_flags::si_zombie);
		}
		break;

	case SDLK_W:    // Freeze
		if (actor->get_flag(Obj_flags::freeze)) {
			actor->clear_flag(Obj_flags::freeze);
		} else {
			actor->set_flag(Obj_flags::freeze);
		}
		break;

	case SDLK_I:    // Party
		if (actor->get_flag(Obj_flags::in_party)) {
			gwin->get_party_man()->remove_from_party(actor);
			gwin->revert_schedules(actor);
			// Just to be sure.
			actor->clear_flag(Obj_flags::in_party);
		} else if (gwin->get_party_man()->add_to_party(actor)) {
			actor->set_schedule_type(Schedule::follow_avatar);
		}
		break;

	case SDLK_O:    // Protectee
		break;

		// Value
	case SDLK_V:    // ID
		if (i < 0) {
			state.SetMode(CP_InvalidValue, false);
		} else if (i > 63) {
			state.SetMode(CP_InvalidValue, false);
		} else if (i == -1 || !state.input[0]) {
			state.SetMode(CP_Canceled);
		} else {
			actor->set_ident(unsigned(i));
		}
		break;

	case SDLK_1:    // Skin color
		actor->set_skin_color(Shapeinfo_lookup::GetNextSkin(
				actor->get_skin_color(), actor->get_type_flag(Actor::tf_sex), Shape_manager::get_instance()->have_si_shapes()));
		break;

	case SDLK_G:    // Tournament
		if (actor->get_flag(Obj_flags::tournament)) {
			actor->clear_flag(Obj_flags::tournament);
		} else {
			actor->set_flag(Obj_flags::tournament);
		}
		break;

	case SDLK_Y:    // Warmth
		if (i < -1) {
			state.SetMode(CP_InvalidValue, false);
		} else if (i > 63) {
			state.SetMode(CP_InvalidValue, false);
		} else if (i == -1 || !state.input[0]) {
			state.SetMode(CP_Canceled);
		} else {
			actor->set_temperature(i);
		}
		break;

	case SDLK_H:    // Polymorph

		// Clear polymorph
		if (actor->get_polymorph() != -1) {
			actor->set_polymorph(actor->get_polymorph());
			break;
		}

		if (state.input[0] == 'b') {    // Browser
			int n;
			clear_buttons();    // Clear all button states before browser
			if (!cheat.get_browser_shape(i, n)) {
				state.SetMode(CP_WrongShapeFile);
				break;
			}
		}

		if (i == -1) {
			state.SetMode(CP_Canceled);
		} else if (i < 0) {
			state.SetMode(CP_InvalidShape, false);
		} else if (i >= nshapes) {
			state.SetMode(CP_InvalidShape, false);
		} else if (state.input[0] && (state.input[0] != '-' || state.input[1])) {
			actor->set_polymorph(i);
			state.SetMode(CP_ShapeSet);
		}

		break;

		// Advanced Numeric Flag Editor
	case '^':
		if (i < 0 || i > 63) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_Canceled);
		} else {
			state.SetMode(AdvancedFlagLoop(i, actor));
		}
		break;

	default:
		break;
	}
	std::memset(state.input, 0, sizeof(state.input));

	state.command = 0;
}

// Checks the state.input
bool CheatScreen::FlagCheck(Actor* actor) {
	switch (state.command) {
		// Everyone

		// Toggles
	case SDLK_A:    // Asleep
	case SDLK_B:    // Charmed
	case SDLK_C:    // Cursed
	case SDLK_D:    // Paralyzed
	case SDLK_E:    // Poisoned
	case SDLK_F:    // Protected
	case SDLK_I:    // Party
	case SDLK_J:    // Invisible
	case SDLK_K:    // Fly
	case SDLK_L:    // Walk
	case SDLK_M:    // Swim
	case SDLK_N:    // Ethrel
	case SDLK_O:    // Protectee
	case SDLK_P:    // Conjured
	case SDLK_Q:    // Summoned
	case SDLK_R:    // Bleedin
	case SDLK_W:    // Freeze
	case SDLK_G:    // Tournament
		state.activate = true;
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		break;

		// Value
	case SDLK_H:    // Polymorph
		if (actor->get_polymorph() == -1) {
			state.SetMode(CP_Shape);
			state.val_min  = 0;
			state.val_max  = Shape_manager::get_instance()->get_shapes().get_num_shapes() - 1;
			state.input[0] = 0;
		} else {
			state.activate = true;
			if (!state.input[0]) {
				state.input[0] = state.command;
			}
		}
		break;

		// Party Only

		// Value
	case SDLK_Y:    // Temp
		if (!actor->is_in_party()) {
			state.command = 0;
		} else {
			state.SetMode(CP_TempNum);
			state.val_max = 0;
			state.val_min = 63;
		}
		state.input[0] = 0;
		break;

		// Avatar Only

		// Toggles
	case SDLK_S:    // Sex
	case SDLK_4:    // Read
		if (actor->get_npc_num()) {
			state.command = 0;
		} else {
			state.activate = true;
		}
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		break;

		// Toggles SI
	case SDLK_5:    // Petra
	case SDLK_7:    // Naked
		if (actor->get_npc_num()) {
			state.command = 0;
		} else {
			state.activate = true;
		}
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		break;

		// Value SI
	case SDLK_1:    // Skin
		if (actor->get_npc_num()) {
			state.command = 0;
		} else {
			state.activate = true;
		}
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		break;

		// Everyone but avatar

		// Toggles
	case SDLK_T:    // Met
	case SDLK_U:    // No Cast
	case SDLK_Z:    // Zombie
		if (!actor->get_npc_num()) {
			state.command = 0;
		} else {
			state.activate = true;
		}
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		break;

		// Value
	case SDLK_V:    // ID
		if (!actor->get_npc_num()) {
			state.command = 0;
		} else {
			state.SetMode(CP_EnterValue);
			state.val_min = 0;
			state.val_max = 63;
		}
		state.input[0] = 0;
		break;

		// NPC Flag Editor

	case SDLK_UP:
		state.command  = '^';
		state.input[0] = 0;
		state.SetMode(CP_NFlagNum);
		state.val_max = 0;
		state.val_min = 63;
		break;

		// X and Escape leave
	case SDLK_ESCAPE:
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		return false;

		// Unknown
	default:
		state.command = 0;
		break;
	}

	return true;
}

//
// Business Schedules
//

void CheatScreen::BusinessLoop(Actor* actor) {
	bool looping = true;

	int time = 0;
	int prev = 0;

	ClearState clear(state);
	while (looping) {
		hotspots.clear();
		gwin->clear_screen();

		// First the display
		if (state.GetMode() == CP_Activity) {
			ActivityDisplay();
		} else {
			BusinessDisplay(actor);
		}

		// Now the Menu Column
		BusinessMenu(actor);

		// Finally the Prompt...
		SharedPrompt();

		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			BusinessActivate(actor, time, prev);
			state.activate = false;
			continue;
		}

		if (SharedInput()) {
			looping = BusinessCheck(actor, time);
		}
	}
}

void CheatScreen::BusinessDisplay(Actor* actor) {
	char             buf[512];
	const Tile_coord t = actor->get_tile();
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx  = 10;
	const int offsety1 = 20;
	const int offsetx2 = 171;
	const int offsety2 = 8;
	const int offsety3 = 0;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	const int offsetx2 = offsetx;
	const int offsety2 = 28;
	const int offsety3 = 16;
#endif

	// Now the info
	const std::string namestr = actor->get_npc_name();
	snprintf(buf, sizeof(buf), "NPC %i - %s", actor->get_npc_num(), namestr.c_str());
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 0, 8, fontcolor.colors);

	snprintf(buf, sizeof(buf), "Loc (%04i, %04i, %02i)", t.tx, t.ty, t.tz);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 8, 8, fontcolor.colors);

#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const char activity_msg[] = "%2i %s";
#else
	const char activity_msg[] = "Current Activity:  %2i - %s";
#endif
	snprintf(buf, sizeof(buf), activity_msg, actor->get_schedule_type(), Strings::ScheduleActivity[actor->get_schedule_type()]);
	font->paint_text_fixedwidth(ibuf, buf, offsetx2, offsety3, 8, fontcolor.colors);

	const Actor::Schedule_list* scheds = actor->get_schedules();

	if (scheds != nullptr) {
		font->paint_text_fixedwidth(ibuf, "Schedules:", offsetx2, offsety2, 8, fontcolor.colors);

		int types[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
		int x[8]     = {0};
		int y[8]     = {0};

		for (const auto& sched : *scheds) {
			const int time        = sched.get_time();
			types[time]           = sched.get_type();
			const Tile_coord tile = sched.get_pos();
			x[time]               = tile.tx;
			y[time]               = tile.ty;
		}

		font->paint_text_fixedwidth(ibuf, "12 AM:", offsetx, 36 - offsety1, 8, fontcolor.colors);
		font->paint_text_fixedwidth(ibuf, " 3 AM:", offsetx, 44 - offsety1, 8, fontcolor.colors);
		font->paint_text_fixedwidth(ibuf, " 6 AM:", offsetx, 52 - offsety1, 8, fontcolor.colors);
		font->paint_text_fixedwidth(ibuf, " 9 AM:", offsetx, 60 - offsety1, 8, fontcolor.colors);
		font->paint_text_fixedwidth(ibuf, "12 PM:", offsetx, 68 - offsety1, 8, fontcolor.colors);
		font->paint_text_fixedwidth(ibuf, " 3 PM:", offsetx, 76 - offsety1, 8, fontcolor.colors);
		font->paint_text_fixedwidth(ibuf, " 6 PM:", offsetx, 84 - offsety1, 8, fontcolor.colors);
		font->paint_text_fixedwidth(ibuf, " 9 PM:", offsetx, 92 - offsety1, 8, fontcolor.colors);

		for (int i = 0; i < 8; i++) {
			if (types[i] != -1) {
				snprintf(buf, sizeof(buf), "%2i (%4i,%4i) - %s", types[i], x[i], y[i], Strings::ScheduleActivity[types[i]]);
				font->paint_text_fixedwidth(ibuf, buf, offsetx + 56, (36 - offsety1) + i * 8, 8, fontcolor.colors);
			}
		}
	}
}

void CheatScreen::BusinessMenu(Actor* actor) {
	char buf[64];
	// Left Column
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx = 10;
	const int offsety = 0;
#else
	const int offsetx = 0;
	const int offsety = 4;
#endif
	int nextx;
	// Might break on monster npcs?
	if (actor->get_npc_num() > 0 && !actor->is_monster()) {
		for (int h = 0; h < 24; h += 3) {
			int row = h / 3;
			int h12 = h % 12;
			h12     = h12 ? h12 : 12;
			int y   = 96 - row * 8;
			snprintf(buf, sizeof(buf), "%2i %cM:", h12, h / 12 ? 'P' : 'A');
			nextx = offsetx;
			nextx += 8 + font->paint_text_fixedwidth(ibuf, buf, nextx, maxy - offsety - y, 8, fontcolor.colors);
			nextx += 8 + AddMenuItem(nextx, maxy - offsety - y, SDLK_A + row, " Set");
			nextx += 8 + AddMenuItem(nextx, maxy - offsety - y, SDLK_I + row, " Location");
			AddMenuItem(nextx, maxy - offsety - y, SDLK_1 + row, " Clear");
		}
		nextx = offsetx;
		nextx += 8 + AddMenuItem(nextx, maxy - offsety - 32, SDLK_S, "et Current Activity");

		AddMenuItem(nextx, maxy - offsety - 32, SDLK_R, "evert");

	} else {
		AddMenuItem(offsetx, maxy - offsety - 96, SDLK_S, "et Current Activity");
	}
	SharedMenu();
}

void CheatScreen::BusinessActivate(Actor* actor, int& time, int& prev) {
	int i = std::atoi(state.input);

	state.SetMode(CP_Command, false);
	const int old = state.command;
	state.command = 0;
	switch (old) {
	case SDLK_A:    // Set Activity
		if (i < 0 || i > 31) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_Activity);
			state.val_min = 0;
			state.val_max = 31;
			state.command = 'a';
		} else {
			actor->set_schedule_time_type(time, i);
		}
		break;

	case SDLK_I:    // X Coord
		if (i < 0 || i > c_num_tiles) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_XCoord);
			state.val_min = 0;
			state.val_max = c_num_tiles;
			state.command = 'i';
		} else {
			prev = i;
			state.SetMode(CP_YCoord);
			state.val_min = 0;
			state.val_max = c_num_tiles;
			state.command = 'j';
		}
		break;

	case SDLK_J:    // Y Coord
		if (i < 0 || i > c_num_tiles) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_YCoord);
			state.val_min = 0;
			state.val_max = c_num_tiles;
			state.command = 'j';
		} else {
			actor->set_schedule_time_location(time, prev, i);
		}
		break;

	case SDLK_1:    // Clear
		actor->remove_schedule(time);
		break;

	case SDLK_S:    // Set Current
		if (i < 0 || i > 31) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_Activity);
			state.val_min = 0;
			state.val_max = 31;
			state.command = 's';
		} else {
			actor->set_schedule_type(i);
		}
		break;

	case SDLK_R:    // Revert
		Game_window::get_instance()->revert_schedules(actor);
		break;

	default:
		break;
	}
	std::memset(state.input, 0, sizeof(state.input));
}

// Checks the state.input
bool CheatScreen::BusinessCheck(Actor* actor, int& time) {
	// Might break on monster npcs?
	if (actor->get_npc_num() > 0) {
		switch (state.command) {
		case SDLK_A:
		case SDLK_B:
		case SDLK_C:
		case SDLK_D:
		case SDLK_E:
		case SDLK_F:
		case SDLK_G:
		case SDLK_H:
			time          = state.command - 'a';
			state.command = 'a';
			state.SetMode(CP_Activity);
			state.val_min = 0;
			state.val_max = 31;
			return true;

		case SDLK_I:
		case SDLK_J:
		case SDLK_K:
		case SDLK_L:
		case SDLK_M:
		case SDLK_N:
		case SDLK_O:
		case SDLK_P:
			time          = state.command - 'i';
			state.command = 'i';
			state.SetMode(CP_XCoord);
			state.val_min = 0;
			state.val_max = c_num_tiles;
			return true;

		case SDLK_1:
		case SDLK_2:
		case SDLK_3:
		case SDLK_4:
		case SDLK_5:
		case SDLK_6:
		case SDLK_7:
		case SDLK_8:
			time           = state.command - '1';
			state.command  = '1';
			state.activate = true;
			return true;

		case SDLK_R:
			state.command  = 'r';
			state.activate = true;
			return true;

		default:
			break;
		}
	}

	switch (state.command) {
		// Set Current
	case SDLK_S:
		state.command  = 's';
		state.input[0] = 0;
		state.SetMode(CP_Activity);
		state.val_min = 0;
		state.val_max = 31;
		break;

		// X and Escape leave
	case SDLK_ESCAPE:
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		return false;

		// Unknown
	default:
		state.command = 0;
		state.SetMode(CP_InvalidCom, false);
		break;
	}

	return true;
}

//
// NPC Stats
//

void CheatScreen::StatLoop(Actor* actor) {
#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(CHEAT_SCREEN_TEST_MOBILE)
	int num = actor->get_npc_num();
#endif
	bool looping = true;

	ClearState clear(state);
	while (looping) {
		hotspots.clear();
		gwin->clear_screen();

#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(CHEAT_SCREEN_TEST_MOBILE)
		// First the display
		NPCDisplay(actor, num);
#endif

		// Now the Menu Column
		StatMenu(actor);

		// Finally the Prompt...
		SharedPrompt();

		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			StatActivate(actor);
			state.activate = false;
			continue;
		}

		if (SharedInput()) {
			looping = StatCheck(actor);
		}
	}
}

void CheatScreen::StatMenu(Actor* actor) {
	char buf[512];
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 92;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
#endif

	// Left Column

	// Dexterity
	snprintf(buf, sizeof(buf), "exterity....%3i", actor->get_property(Actor::dexterity));
	AddMenuItem(offsetx, maxy - offsety1 - 108, SDLK_D, buf);

	// Food Level
	snprintf(buf, sizeof(buf), "ood Level...%3i", actor->get_property(Actor::food_level));
	AddMenuItem(offsetx, maxy - offsety1 - 99, SDLK_F, buf);

	// Intelligence
	snprintf(buf, sizeof(buf), "ntellicence.%3i", actor->get_property(Actor::intelligence));
	AddMenuItem(offsetx, maxy - offsety1 - 90, SDLK_I, buf);

	// Strength
	snprintf(buf, sizeof(buf), "trength.....%3i", actor->get_property(Actor::strength));
	AddMenuItem(offsetx, maxy - offsety1 - 81, SDLK_S, buf);

	// Combat Skill
	snprintf(buf, sizeof(buf), "ombat Skill.%3i", actor->get_property(Actor::combat));
	AddMenuItem(offsetx, maxy - offsety1 - 72, SDLK_C, buf);

	// Hit Points
	snprintf(buf, sizeof(buf), "it Points...%3i", actor->get_property(Actor::health));
	AddMenuItem(offsetx, maxy - offsety1 - 63, SDLK_H, buf);

	// Magic
	// Magic Points
	snprintf(buf, sizeof(buf), "agic Points.%3i", actor->get_property(Actor::magic));
	AddMenuItem(offsetx, maxy - offsety1 - 54, SDLK_M, buf);

	// Mana
	snprintf(buf, sizeof(buf), "ana Level...%3i", actor->get_property(Actor::mana));
	AddMenuItem(offsetx, maxy - offsety1 - 45, SDLK_V, buf);

	SharedMenu();
}

void CheatScreen::StatActivate(Actor* actor) {
	int i = std::atoi(state.input);
	state.SetMode(CP_Command, false);
	// Enforce sane bounds.
	if (i > 60) {
		i = 60;
	} else if (i < 0 && state.command != 'h') {
		if (i == -1) {    // canceled
			std::memset(state.input, 0, sizeof(state.input));

			state.command = 0;
			return;
		}
		i = 0;
	} else if (i < -50) {
		i = -50;
	}

	switch (state.command) {
	case SDLK_D:    // Dexterity
		actor->set_property(Actor::dexterity, i);
		break;

	case SDLK_F:    // Food Level
		actor->set_property(Actor::food_level, i);
		break;

	case SDLK_I:    // Intelligence
		actor->set_property(Actor::intelligence, i);
		break;

	case SDLK_S:    // Strength
		actor->set_property(Actor::strength, i);
		break;

	case SDLK_C:    // Combat Points
		actor->set_property(Actor::combat, i);
		break;

	case SDLK_H:    // Hit Points
		actor->set_property(Actor::health, i);
		break;

	case SDLK_M:    // Magic
		actor->set_property(Actor::magic, i);
		break;

	case SDLK_V:    // [V]ana
		actor->set_property(Actor::mana, i);
		break;

	default:
		break;
	}
	std::memset(state.input, 0, sizeof(state.input));

	state.command = 0;
}

// Checks the state.input
bool CheatScreen::StatCheck(Actor* actor) {
	ignore_unused_variable_warning(state.activate, actor);

	switch (state.command) {
		// Everyone
	case SDLK_H:    // Hit Points
		state.input[0] = 0;
		state.SetMode(CP_EnterValueNoCancel);
		state.val_min = 0;
		state.val_max = actor->get_property(Actor::strength);
		;
		break;
	case SDLK_D:    // Dexterity
	case SDLK_F:    // Food Level
	case SDLK_I:    // Intelligence
	case SDLK_S:    // Strength
	case SDLK_C:    // Combat Points
	case SDLK_M:    // Magic
	case SDLK_V:    // [V]ana
		state.input[0] = 0;
		state.SetMode(CP_EnterValue);
		state.val_min = 0;
		state.val_max = 255;
		break;

		// X and Escape leave
	case SDLK_ESCAPE:
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		return false;

		// Unknown
	default:
		state.command = 0;
		break;
	}

	return true;
}

//
// Pallete Effect
//

void CheatScreen::PalEffectLoop(Actor* actor) {
#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(CHEAT_SCREEN_TEST_MOBILE)
	int num = actor->get_npc_num();
#endif
	bool looping = true;

	ClearState clear(state);
	while (looping) {
		hotspots.clear();
		gwin->clear_screen();

#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(CHEAT_SCREEN_TEST_MOBILE)
		// First the display
		NPCDisplay(actor, num);
#endif

		// Now the Menu Column
		PalEffectMenu(actor);

		// Finally the Prompt...
		SharedPrompt();

		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			PalEffectActivate(actor);
			state.activate = false;
			continue;
		}

		if (SharedInput()) {
			looping = PalEffectCheck(actor);
		}
	}
}

void CheatScreen::PalEffectMenu(Actor* actor) {
	char buf[512];
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 81;
	// const int offsety2 = 72;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	// const int offsety2 = maxy - 36;

#endif
	int pt = actor->get_palette_transform();
	if (pt == 0) {
		snprintf(buf, sizeof(buf), "Palette effect: None");
	} else if ((pt & ShapeID::PT_RampRemapAllFrom) == ShapeID::PT_RampRemapAllFrom) {
		snprintf(buf, sizeof(buf), "Palette effect: Ramp Remap All To %i", pt & 31);
	} else if ((pt & ShapeID::PT_RampRemap) == ShapeID::PT_RampRemap) {
		snprintf(buf, sizeof(buf), "Palette effect: Ramp Remap %i To %i", (pt >> 5) & 31, pt & 31);
	} else if ((pt & ShapeID::PT_xForm) == ShapeID::PT_xForm) {
		snprintf(buf, sizeof(buf), "Palette effect: XForm %i", pt & 31);
	} else if (pt < 256) {
		snprintf(buf, sizeof(buf), "Palette effect: Shift by %i", pt & 0xff);
	}
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 119, 8, fontcolor.colors);

	if (state.command == 't') {
		if (state.saved_value == 255) {
			snprintf(buf, sizeof(buf), "From Ramp: All");
		} else {
			snprintf(buf, sizeof(buf), "From Ramp: %i", state.saved_value & 31);
		}

		font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 110, 8, fontcolor.colors);
	}

	// Left Column

	// ramp remap
	AddMenuItem(offsetx, maxy - offsety1 - 99, SDLK_R, "amp Remap");

	// xform
	AddMenuItem(offsetx, maxy - offsety1 - 90, SDLK_X, "form");

	// Shift
	AddMenuItem(offsetx, maxy - offsety1 - 81, SDLK_S, "hift");

	// clear
	AddMenuItem(offsetx, maxy - offsety1 - 72, SDLK_C, "lear");

	SharedMenu();
}

void CheatScreen::PalEffectActivate(Actor* actor) {
	int   i = std::atoi(state.input);
	char* end;
	auto  u = std::strtoul(state.input, &end, 10);

	switch (state.command) {
	case 'x':    // XForm
		actor->set_palette_transform(ShapeID::PT_xForm | i % int(Shape_manager::get_instance()->get_xforms_cnt()));
		break;

	case 'f':    // from Ramp
	{
		const char   prompttext[] = "enter To Ramp number Index (0-%i)";
		static char  staticPrompttext[sizeof(prompttext) + 16];
		unsigned int numramps = 0;
		gwin->get_pal()->get_ramps(numramps);
		if (numramps) {
			numramps--;
		}
		if (u >= numramps && u != 255) {
			state.SetMode(CP_InvalidValue, false);
			break;
		}
		std::snprintf(staticPrompttext, sizeof(staticPrompttext), prompttext, numramps);
		state.input[0]      = 0;
		state.custom_prompt = staticPrompttext;
		state.SetMode(CP_CustomValue);
		state.command     = 't';
		state.saved_value = i;
		state.val_min     = 0;
		state.val_max     = int(numramps);
		return;
	}

	case 't':    // to ramp
	{
		unsigned int numramps = 0;
		gwin->get_pal()->get_ramps(numramps);
		if (u >= numramps) {
			state.SetMode(CP_InvalidValue, false);
			break;
		}
		if (state.saved_value == 255) {
			actor->set_palette_transform(ShapeID::PT_RampRemapAllFrom | (i & 31));
		} else {
			actor->set_palette_transform(ShapeID::PT_RampRemap | (i & 31) | ((state.saved_value & 0xff) << 5));
		}
	} break;

	case 's':    // Shift
		actor->set_palette_transform(ShapeID::PT_Shift | (i & 0xff));
		break;

	case 'c':    // clear
		actor->set_palette_transform(0);
		break;

	default:
		break;
	}
	ClearState clear(state, false, true);
	state.command = 0;
}

// Checks the state.input
bool CheatScreen::PalEffectCheck(Actor* actor) {
	ignore_unused_variable_warning(state.activate, actor);
	switch (state.command) {
	case 'r':    // [R]amp Remap
	{
		const char   prompttext[] = "enter From Ramp (0-%u) or 255 for all";
		static char  staticPrompttext[sizeof(prompttext) + 16];
		unsigned int numramps = 0;
		gwin->get_pal()->get_ramps(numramps);
		if (numramps) {
			numramps--;
		}
		std::snprintf(staticPrompttext, sizeof(staticPrompttext), prompttext, numramps);
		state.input[0]      = 0;
		state.custom_prompt = staticPrompttext;
		state.SetMode(CP_CustomValue);
		state.command = 'f';
		state.val_min = 0;
		state.val_max = 255;
	} break;

	case 'x':    // [X]Form
	{
		const char  prompttext[] = "enter XFORM Index (0-%zu)";
		static char staticPrompttext[sizeof(prompttext) + 16];
		size_t      numxforms = Shape_manager::get_instance()->get_xforms_cnt();
		if (numxforms) {
			numxforms--;
		}
		std::snprintf(staticPrompttext, sizeof(staticPrompttext), prompttext, numxforms);
		state.input[0]      = 0;
		state.custom_prompt = staticPrompttext;
		state.SetMode(CP_CustomValue);
		state.val_min = 0;
		state.val_max = int(numxforms);
	} break;

	case 's':    // [S]hift
		state.input[0]      = 0;
		state.custom_prompt = "enter shift amount (0-255)";
		state.SetMode(CP_EnterValue);
		state.val_min = 0;
		state.val_max = 255;
		break;

		// Escape leaves
		// X and Escape leave
	case SDLK_ESCAPE:
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		return false;

		// clear
	case 'c':
		state.activate = true;
		break;

		// Unknown
	default:
		state.command = 0;
		break;
	}

	return true;
}

//
// Advanced Flag Editor
//

CheatScreen::Cheat_Prompt CheatScreen::AdvancedFlagLoop(int num, Actor* actor) {
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 83;
	// const int offsety2 = 72;
#else
	int       npc_num  = actor->get_npc_num();
	const int offsetx  = 0;
	const int offsety1 = 0;
	// const int offsety2 = maxy - 36;
#endif
	bool looping = true;
	char buf[64];

	ClearState clear(state);
	while (looping) {
		hotspots.clear();
		gwin->clear_screen();

#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(CHEAT_SCREEN_TEST_MOBILE)
		NPCDisplay(actor, npc_num);
#endif

		if (num < 0) {
			num = 0;
		} else if (num > 63) {
			num = 63;
		}

		// First the info
		auto flag_name = Strings::NPCFlag[num];
		if (flag_name && *flag_name) {
			snprintf(buf, sizeof(buf), "NPC Flag %i: %s", num, flag_name);
		} else {
			snprintf(buf, sizeof(buf), "NPC Flag %i", num);
		}
		font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 108, 8, fontcolor.colors);

		snprintf(buf, sizeof(buf), "Flag is %s", actor->get_flag(num) ? "SET" : "UNSET");
		font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 90, 8, fontcolor.colors);

		// Now the Menu Column
		if (!actor->get_flag(num)) {
			AddMenuItem(offsetx + 160, maxy - offsety1 - 90, SDLK_S, "et Flag");
		} else {
			AddMenuItem(offsetx + 160, maxy - offsety1 - 90, SDLK_U, "nset Flag");
		}

		// Change Flag
		AddMenuItem(offsetx, maxy - offsety1 - 72, SDLK_UP, " Change Flag");

		AddLeftRightMenuItem(offsetx, maxy - offsety1 - 63, "Scroll Flags", num > 0, num < 63, false, true);

		SharedMenu();

		// Finally the Prompt...
		SharedPrompt();

		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			state.SetMode(CP_Command);
			if (state.command == '<') {    // Decrement
				num--;
				if (num < 0) {
					num = 0;
				}
			} else if (state.command == '>') {    // Increment
				num++;
				if (num > 63) {
					num = 63;
				}
			} else if (state.command == '^') {    // Change Flag
				int i = std::atoi(state.input);
				if (i < 0 || i > 63) {
					state.SetMode(CP_InvalidValue, false);
				} else if (state.input[0]) {
					num = i;
				}
			} else if (state.command == 's') {    // Set
				actor->set_flag(num);
				if (num == Obj_flags::in_party) {
					gwin->get_party_man()->add_to_party(actor);
					actor->set_schedule_type(Schedule::follow_avatar);
				}
			} else if (state.command == 'u') {    // Unset
				if (num == Obj_flags::in_party) {
					gwin->get_party_man()->remove_from_party(actor);
					gwin->revert_schedules(actor);
				}
				actor->clear_flag(num);
			}

			ClearState clearer(state);
			continue;
		}

		if (SharedInput()) {
			switch (state.command) {
				// Simple commands
			case SDLK_S:    // Set Flag
			case SDLK_U:    // Unset flag
				if (!state.input[0]) {
					state.input[0] = state.command;
				}
				state.activate = true;
				break;

				// Decrement
			case SDLK_LEFT:
				state.command = '<';
				if (!state.input[0]) {
					state.input[0] = state.command;
				}
				state.activate = true;
				break;

				// Increment
			case SDLK_RIGHT:
				state.command = '>';
				if (!state.input[0]) {
					state.input[0] = state.command;
				}
				state.activate = true;
				break;

				// * Change Flag
			case SDLK_UP:
				state.command  = '^';
				state.input[0] = 0;
				state.SetMode(CP_NFlagNum);
				state.val_max = 0;
				state.val_min = 63;
				break;

				// X and Escape leave
			case SDLK_ESCAPE:
				if (!state.input[0]) {
					state.input[0] = state.command;
				}
				looping = false;
				break;

			default:
				state.SetMode(CP_InvalidCom, false);
				if (!state.input[0]) {
					state.input[0] = (state.command < 128 ? state.command : 0);
				}
				state.command = 0;
				break;
			}
		}
	}
	return CP_Command;
}
