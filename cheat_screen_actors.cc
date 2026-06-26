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
// NPC Menu
//

std::shared_ptr<CheatScreen::Menu> CheatScreen::NPCMenu(Actor* actor) {
	std::forward_list<std::pair<Hotspot, std::shared_ptr<MenuCommand>>> items;
	std::shared_ptr<MenuCommand>                                        command;
	const char*                                                         label;

#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 74;
	const int offsetx2 = -145;
	const int offsety2 = 65;
	const int offsetx3 = 175;
	const int offsety3 = 63;
	const int offsety4 = 72;
	const int offsetx1 = 6;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	const int offsetx2 = 0;
	const int offsety2 = 0;
	const int offsetx3 = offsetx + 160;
	const int offsety3 = maxy - 45;
	const int offsety4 = maxy - 36;
	const int offsetx1 = 0;
#endif
	// Left Column
	auto hideifnoactor = [](MenuCommand* self) {
		bool hide = !MenuCommand::getDataOrDefault<Actor*>(self->GetMyMenu());
		self->hotspot->setHidden(0, hide);
		self->hotspot->setHidden(1, hide);
	};

	// Business Activity
	command                  = std::make_shared<MenuCommand>();
	command->events.run      = hideifnoactor;
	command->events.Activate = [this](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		return BusinessMenu(MenuCommand::getDataOrDefault<Actor*>(self->GetMyMenu()));
	};
	label = Strings::NPCMenu::BusinessActivity;
	items.emplace_front(Hotspot(offsetx, maxy - offsety1 - 99, label[0], label + 1), command);

	// Change Shape
	command = std::make_shared<MenuCommand>();
	command->inputs.push_back(std::make_shared<InputHandlers::Shape>(false, false));
	command->events.run      = hideifnoactor;
	label                    = Strings::NPCMenu::ChangeShape;
	command->events.Activate = [](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		auto sinput = static_cast<InputHandlers::Shape*>(self->inputs[0].get());
		if (Actor* actor = MenuCommand::getDataOrDefault<Actor*>(self->GetMyMenu())) {
			if (actor->get_npc_num() != 0) {
				actor->set_shape(sinput->shapenum);
			} else {
				actor->set_polymorph(sinput->shapenum);
			}
			throw MenuCommandException{Strings::SHAPE_SET, true};
		}
		return {};
	};
	items.emplace_front(Hotspot(offsetx, maxy - offsety1 - 90, label[0], label + 1), command);

	// XP
	command = std::make_shared<MenuCommand>();
	command->inputs.push_back(std::make_shared<InputHandlers::Integer>(
			false, 0, Actor::max_exp, false, Strings::ENTER_VALUE, Strings::INVALID_VALUE));
	command->events.Activate = [](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		if (Actor* actor = MenuCommand::getDataOrDefault<Actor*>(self->GetMyMenu())) {
			actor->set_property(Actor::exp, static_cast<InputHandlers::Integer*>(self->inputs[0].get())->value);
		}
		return {};
	};
	command->events.run = hideifnoactor;
	label               = Strings::NPCMenu::Exprience;
	items.emplace_front(Hotspot(offsetx, maxy - offsety1 - 81, label[0], label + 1), command);

	label                    = Strings::NPCMenu::Npcflags;
	command                  = std::make_shared<MenuCommand>();
	command->events.run      = hideifnoactor;
	command->events.Activate = [this](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		// return NPCFlagMenu(MenuCommand::getDataOrDefault<Actor*>(self->GetMyMenu()));
		FlagLoop(MenuCommand::getDataOrDefault<Actor*>(self->GetMyMenu()));
		return {};
	};
	items.emplace_front(Hotspot(offsetx, maxy - offsety1 - 72, label[0], label + 1), command);

	// Name
	command = std::make_shared<MenuCommand>();
	command->inputs.push_back(std::make_shared<InputHandlers::String>(false, Strings::ENTER_A_NEW_NAME, Strings::CANCELLED));
	command->events.Activate = [](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		auto menu = self->GetMyMenu();
		if (Actor* actor = MenuCommand::getDataOrDefault<Actor*>(menu)) {
			actor->set_npc_name(self->inputs[0]->getInput());
			throw MenuCommandException{Strings::NAME_CHANGED, true};
		}
		return {};
	};
	command->events.run = hideifnoactor;
	label               = Strings::NPCMenu::Name;
	items.emplace_front(Hotspot(offsetx, maxy - offsety1 - 63, label[0], label + 1), command);

	// Right Column

	// stats -  This is its own menu but we generate it here insteads of using a separate menu function
	std::forward_list<std::pair<Hotspot, std::shared_ptr<MenuCommand>>> stats_items;
	{
		// Stats Left Column - Generated from this array
		std::tuple<const char*, Actor::Item_properties, int, int> stats[] = {
				{   Strings::NPCStatsMenu::Dexterity,    Actor::dexterity, 0, 255},
				{   Strings::NPCStatsMenu::FoodLevel,   Actor::food_level, 0, 255},
				{Strings::NPCStatsMenu::Intellicence, Actor::intelligence, 0, 255},
				{    Strings::NPCStatsMenu::Strength,     Actor::strength, 0, 255},
				{ Strings::NPCStatsMenu::CombatSkill,       Actor::combat, 0, 255},
				{   Strings::NPCStatsMenu::HitPoints,       Actor::health, 0, 255},
				{ Strings::NPCStatsMenu::MagicPoints,        Actor::magic, 0, 255},
				{   Strings::NPCStatsMenu::ManaLevel,         Actor::mana, 0, 255},
		};
		int stat_posy = maxy - offsety1 - 108;
		for (const auto& stat : stats) {
			int prop = std::get<1>(stat);
			command  = std::make_shared<MenuCommand>();
			command->inputs.push_back(std::make_shared<InputHandlers::Integer>(
					false, std::get<2>(stat), std::get<3>(stat), false, Strings::ENTER_VALUE, Strings::INVALID_VALUE));
			command->events.run = [prop](MenuCommand* self) {
				auto menu = self->GetMyMenu()->GetMyMenu();
				if (Actor* actor = MenuCommand::getDataOrDefault<Actor*>(menu)) {
					char buf[16];
					snprintf(buf, sizeof(buf), "%3d", actor->get_property(prop));
					self->hotspot->label_rw = self->hotspot->label + buf;
				}
			};
			command->events.Activate = [prop](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
				auto menu = self->GetMyMenu()->GetMyMenu();
				if (Actor* actor = MenuCommand::getDataOrDefault<Actor*>(menu)) {
					actor->set_property(prop, static_cast<InputHandlers::Integer*>(self->inputs[0].get())->value);
				}
				return {};
			};
			label = std::get<0>(stat);
			stats_items.emplace_front(Hotspot(offsetx, stat_posy, label[0], label + 1), command);
			stat_posy += 9;
		}
	}
	command             = std::make_shared<Menu>(std::move(stats_items));
	command->events.run = hideifnoactor;
	label               = Strings::NPCMenu::Stats;
	items.emplace_front(Hotspot(offsetx + 160, maxy - offsety1 - 99, label[0], label + 1), command);

	// Training Points
	command = std::make_shared<MenuCommand>();
	command->inputs.push_back(std::make_shared<InputHandlers::Integer>(
			false, 0, Actor::max_training, false, Strings::ENTER_VALUE, Strings::INVALID_VALUE));
	command->events.Activate = [](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		auto menu = self->GetMyMenu();
		if (Actor* actor = MenuCommand::getDataOrDefault<Actor*>(menu)) {
			actor->set_property(Actor::training, static_cast<InputHandlers::Integer*>(self->inputs[0].get())->value);
		}
		return {};
	};
	command->events.run = hideifnoactor;
	label               = Strings::NPCMenu::TrainingPoints;
	items.emplace_front(Hotspot(offsetx + 160, maxy - offsety1 - 90, label[0], label + 1), command);

	// Teleport
	command                  = std::make_shared<MenuCommand>();
	command->events.Activate = [](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		auto menu = self->GetMyMenu();
		if (Actor* actor = MenuCommand::getDataOrDefault<Actor*>(menu)) {
			Game_window::get_instance()->teleport_party(actor->get_tile(), false, actor->get_map_num());
		}
		return {};
	};
	command->events.run = hideifnoactor;
	label               = Strings::NPCMenu::TeleporttoNPC;
	items.emplace_front(Hotspot(offsetx + 160, maxy - offsety1 - 81, label[0], label + 1), command);

	// Palette Effect
	command                  = std::make_shared<MenuCommand>();
	command->events.run      = hideifnoactor;
	command->events.Activate = [this](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		auto menu = self->GetMyMenu();
		if (Actor* actor = MenuCommand::getDataOrDefault<Actor*>(menu)) {
			PalEffectLoop(actor);
		}
		return {};
	};
	label = Strings::NPCMenu::PaletteEffect;
	items.emplace_front(Hotspot(offsetx + 160, maxy - offsety1 - 72, label[0], label + 1), command);

	// Walk to Avatar
	command                  = std::make_shared<MenuCommand>();
	command->events.Activate = [](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		auto menu = self->GetMyMenu();
		if (Actor* actor = MenuCommand::getDataOrDefault<Actor*>(menu)) {
			actor->approach_another(Game_window::get_instance()->get_main_actor());
		}
		return {};
	};
	command->events.run = hideifnoactor;
	label               = Strings::NPCMenu::WalktoAvatar;
	items.emplace_front(Hotspot(offsetx2 + 160, maxy - offsety2 - 63, label[0], label + 1), command);

	// Change NPC
	command                  = std::make_shared<MenuCommand>();
	command->events.Activate = [](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		auto npcih = static_cast<InputHandlers::NPC*>(self->inputs[0].get());
		if (npcih->actor) {
			auto menu = self->GetMyMenu();
			menu->setData<Actor*>(npcih->actor);
		}

		return {};
	};
	command->inputs.push_back(std::make_shared<InputHandlers::NPC>(false));

	label = Strings::NPCMenu::ChangeNPC;
	items.emplace_front(Hotspot(offsetx3, offsety3, SDLK_UP, label), command);
	// Scroll NPCs

	command                  = std::make_shared<LeftRightIntegerCommand>(0, gwin->get_num_npcs(), 0, false);
	command->events.Activate = [](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		auto menu = self->GetMyMenu();
		if (Actor* actor = gwin->get_npc(static_cast<LeftRightIntegerCommand*>(self)->currentval)) {
			menu->setData<Actor*>(actor);
		}

		return {};
	};
	command->events.run = [](MenuCommand* self) {
		// Make sure currentval is synced with the npc number because it could have chaged
		auto menu = self->GetMyMenu();
		if (Actor* actor = MenuCommand::getDataOrDefault<Actor*>(menu)) {
			static_cast<LeftRightIntegerCommand*>(self)->currentval = actor->get_npc_num();
		}
	};
	label = Strings::NPCMenu::ScrollNPCs;
	items.emplace_front(Hotspot(offsetx3, offsety4, SDLK_LEFT, label, SDLK_RIGHT), command);

	std::shared_ptr<Menu> menu = std::make_shared<Menu>(std::move(items));
	menu->setData<Actor*>(actor);
	menu->events.paint_display = [this](MenuCommand* self) {
		int  num = -1;
		char buf[512];
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
		const int offsetx  = 15;
		const int offsety1 = 73;
#else
		const int offsetx  = 0;
		const int offsety1 = 0;
#endif

		if (Actor* actor = self->getDataOrDefault<Actor*>()) {
			num                = actor->get_npc_num();
			const Tile_coord t = actor->get_tile();

			// Paint the actors shape
			Shape_frame* shape = actor->get_shape();
			if (shape) {
				actor->paint_shape(shape->get_xright() + 240, shape->get_yabove(), true);
			}

			// Now the info
			const std::string namestr = actor->get_npc_name();
			snprintf(
					buf, sizeof(buf), "%s%i - %s%s", Strings::NPCMenu::NPC_(), num, namestr.c_str(),
					actor->is_unused() ? Strings::NPCMenu::unused() : "");
			font->paint_text_fixedwidth(ibuf, buf, offsetx, 0, 8, fontcolor.colors);

			snprintf(buf, sizeof(buf), "%s(%04i, %04i, %02i)", Strings::NPCMenu::Loc_(), t.tx, t.ty, t.tz);
			font->paint_text_fixedwidth(ibuf, buf, offsetx, 9, 8, fontcolor.colors);

			snprintf(
					buf, sizeof(buf), "%s%04i:%02i  %s", Strings::NPCMenu::Shape_(), actor->get_shapenum(), actor->get_framenum(),
					actor->get_flag(Obj_flags::met) ? Strings::NPCMenu::Met() : Strings::NPCMenu::NotMet());
			font->paint_text_fixedwidth(ibuf, buf, offsetx, 18, 8, fontcolor.colors);

			snprintf(
					buf, sizeof(buf), "%s%2i - %s", Strings::NPCMenu::CurrentActivity_(), actor->get_schedule_type(),
					Strings::ScheduleActivity[actor->get_schedule_type()]);
			font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 36, 8, fontcolor.colors);

			snprintf(buf, sizeof(buf), "%s: %i", Strings::NPCMenu::Exprience(), actor->get_property(Actor::exp));
			font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 45, 8, fontcolor.colors);

			snprintf(buf, sizeof(buf), "%s%i", Strings::NPCMenu::Level_(), actor->get_level());
			font->paint_text_fixedwidth(ibuf, buf, offsetx + 144, offsety1 + 45, 8, fontcolor.colors);

			snprintf(
					buf, sizeof(buf), "%s%2i%s%2i", Strings::NPCMenu::Training_(), actor->get_property(Actor::training),
					Strings::NPCMenu::Health_(), actor->get_property(Actor::health));
			font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 54, 8, fontcolor.colors);

			if (num != -1) {
				int ucitemnum = 0x10000 - num;
				if (!num) {
					ucitemnum = 0xfe9c;
				}
				snprintf(
						buf, sizeof(buf), "%s%4x%s%x", Strings::NPCMenu::Usecodeitem(), ucitemnum, Strings::NPCMenu::function(),
						actor->get_usecode());
				font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 63, 8, fontcolor.colors);
			} else {
				snprintf(buf, sizeof(buf), "%s%x", Strings::NPCMenu::Usecodefunction(), actor->get_usecode());
				font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 63, 8, fontcolor.colors);
			}

			if (actor->get_flag(Obj_flags::charmed)) {
				snprintf(
						buf, sizeof(buf), "%s%s (%s%s)", Strings::NPCMenu::Alignment(),
						Strings::Alignment[actor->get_effective_alignment()], Strings::NPCMenu::orig(),
						Strings::Alignment[actor->get_alignment()]);
			} else {
				snprintf(buf, sizeof(buf), "%s%s", Strings::NPCMenu::Alignment(), Strings::Alignment[actor->get_alignment()]);
			}
			font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 72, 8, fontcolor.colors);

			if (actor->get_polymorph() != -1) {
				snprintf(buf, sizeof(buf), "%s%04i", Strings::NPCMenu::Polymorphedfrom(), actor->get_polymorph());
				font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 81, 8, fontcolor.colors);
			}
		} else {
			snprintf(buf, sizeof(buf), "%s %i - %s", Strings::NPCMenu::NPC_(), num, Strings::NPCMenu::InvalidNPC());
			font->paint_text_fixedwidth(ibuf, buf, offsetx, 0, 8, fontcolor.colors);
		}
		return true;
	};
	return menu;
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
		// NPCDisplay(actor, num);
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

std::shared_ptr<CheatScreen::Menu> CheatScreen::BusinessMenu(Actor* actor) {
	std::forward_list<std::pair<Hotspot, std::shared_ptr<MenuCommand>>> items;
	std::shared_ptr<MenuCommand>                                        command;

#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx  = 10;
	const int offsety  = 0;
	const int offsety1 = 20;
	const int offsetx2 = 171;
	const int offsety2 = 8;
	const int offsety3 = 0;
#else
	const int offsetx  = 0;
	const int offsety  = 4;
	const int offsety1 = 0;
	const int offsetx2 = offsetx;
	const int offsety2 = 28;
	const int offsety3 = 16;
#endif
	const char* label;

	if (!actor) {
		return {};
	}
	std::function<bool(MenuCommand*)> ActivityListDisplay = [this](MenuCommand*) -> bool {
		char buf[64];
		// 31 activities
		const int num_acts = 31;
		// This defaults to 3 columns of 11. On mobile the vertical space is limited so row spacing is squeezed to 7 pixels
		// If there is enough space on mobile for 4 columns we do 4 columns of 9 with row spacing of 8
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
		const int rowheight = maxx >= 420 ? 8 : 7;
		const int colsize   = maxx >= 420 ? 9 : 11;
#else
		const int rowheight = 8;
		const int colsize   = 11;
#endif
		const int numcols = (num_acts + colsize - 1) / colsize;

		for (int a = 0; a <= num_acts; a++) {
			int row = a % colsize;
			int col = a / colsize;
			snprintf(buf, sizeof(buf), "%2i:%s", a, Strings::ScheduleActivity[a]);
			buf[std::size(buf) - 1] = 0;
			font->paint_text_fixedwidth(
					ibuf, buf, (maxx * col) / numcols - (colsize <= 10 ? 8 : 0), rowheight * row, 8, fontcolor.colors);
		}

		return true;
	};

	int y_current;    // y pos for set current activity menu item
	// Might break on monster npcs?
	if (actor->get_npc_num() > 0 && !actor->is_monster()) {
		char buf[64];
		int  nextx;
		for (int time = 0; time < 8; time++) {
			int h12 = (time & 3) * 3;
			h12     = h12 ? h12 : 12;
			int y   = 96 - time * 8;
			nextx   = offsetx;
			snprintf(buf, sizeof(buf), "%2i %2s:", h12, time < 4 ? Strings::am() : Strings::pm());
			buf[std::size(buf) - 1] = 0;
			label                   = buf;
			items.emplace_front(Hotspot(nextx, maxy - offsety - y, 0, label), nullptr);
			nextx += 8 + 8 * strlen(label);
			command = std::make_shared<MenuCommand>();
			command->inputs.push_back(std::make_shared<InputHandlers::Integer>(false, 0, 31, false, Strings::ENTER_ACTIVITY));
			command->events.Activate = [actor, time](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
				if (auto input = dynamic_cast<InputHandlers::Integer*>(self->inputs.front().get())) {
					actor->set_schedule_time_type(time, input->value);
				}
				return {};
			};
			command->events.paint_display = ActivityListDisplay;
			label                         = Strings::BusinessMenu::Set;

			items.emplace_front(Hotspot(nextx, maxy - offsety - y, 'A' + time, label), command);
			nextx += 32 + 8 * strlen(label);
			command = std::make_shared<MenuCommand>();
			command->inputs.reserve(3);
			command->inputs.push_back(
					std::make_shared<InputHandlers::Integer>(false, 0, c_num_tiles, false, Strings::ENTER_X_COORD));
			command->inputs.push_back(
					std::make_shared<InputHandlers::Integer>(false, 0, c_num_tiles, false, Strings::ENTER_Y_COORD));
			command->inputs.push_back(std::make_shared<InputHandlers::Integer>(true, 0, 255, false, Strings::ENTER_Z_COORD));
			command->events.Activate = [actor, time](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
				auto xinput = dynamic_cast<InputHandlers::Integer*>(self->inputs[0].get());
				auto yinput = dynamic_cast<InputHandlers::Integer*>(self->inputs[1].get());
				auto zinput = dynamic_cast<InputHandlers::Integer*>(self->inputs[2].get());
				if (xinput && yinput && zinput) {
					actor->set_schedule_time_location(time, xinput->value, yinput->value, zinput->value);
				}
				return {};
			};
			label = Strings::BusinessMenu::Location;
			items.emplace_front(Hotspot(nextx, maxy - offsety - y, 'I' + time, label), command);
			nextx += 32 + 8 * strlen(label);
			command                  = std::make_shared<MenuCommand>();
			command->events.Activate = [actor, time](MenuCommand*, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
				actor->remove_schedule(time);
				return {};
			};
			items.emplace_front(Hotspot(nextx, maxy - offsety - y, SDLK_1 + time, Strings::BusinessMenu::Clear), command);
		}
		nextx = offsetx + 32 + 8 * strlen(Strings::BusinessMenu::SetCurrentActivity + 1);

		command                  = std::make_shared<MenuCommand>();
		command->events.Activate = [actor](MenuCommand*, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
			Game_window::get_instance()->revert_schedules(actor);
			return {};
		};
		label = Strings::BusinessMenu::Revert;
		items.emplace_front(Hotspot(nextx, maxy - offsety - 32, label[0], label + 1), command);
		y_current = 32;

	} else {
		y_current = 96;
	}
	command = std::make_shared<MenuCommand>();
	command->inputs.push_back(std::make_shared<InputHandlers::Integer>(false, 0, 31, false, Strings::ENTER_ACTIVITY));
	command->events.Activate = [actor](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		if (auto input = dynamic_cast<InputHandlers::Integer*>(self->inputs.front().get())) {
			actor->set_schedule_type(input->value);
		}
		return {};
	};
	command->events.paint_display = ActivityListDisplay;
	label                         = Strings::BusinessMenu::SetCurrentActivity;
	items.emplace_front(Hotspot(offsetx, maxy - offsety - y_current, label[0], label + 1), command);

	auto menu                  = std::make_shared<Menu>(std::move(items));
	menu->events.paint_display = [=](MenuCommand*) -> bool {
		char             buf[512];
		const Tile_coord t = actor->get_tile();

		// Now the info
		const std::string namestr = actor->get_npc_name();
		snprintf(buf, sizeof(buf), "%s%i - %s", Strings::NPCMenu::NPC_(), actor->get_npc_num(), namestr.c_str());
		font->paint_text_fixedwidth(ibuf, buf, offsetx, 0, 8, fontcolor.colors);

		snprintf(buf, sizeof(buf), "%s(%04i, %04i, %02i)", Strings::NPCMenu::Loc_(), t.tx, t.ty, t.tz);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, 8, 8, fontcolor.colors);

#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
		const char  activity_msg[]     = "%s%2i %s";
		const char* activity_msg_label = "";
#else
		const char  activity_msg[]     = "%s%2i - %s";
		const char* activity_msg_label = Strings::NPCMenu::CurrentActivity_;
#endif
		snprintf(
				buf, sizeof(buf), activity_msg, activity_msg_label, actor->get_schedule_type(),
				Strings::ScheduleActivity[actor->get_schedule_type()]);
		font->paint_text_fixedwidth(ibuf, buf, offsetx2, offsety3, 8, fontcolor.colors);

		const Actor::Schedule_list* scheds = actor->get_schedules();

		// Monsters return an empty list so don't show anything
		if (scheds != nullptr && !actor->is_monster()) {
			font->paint_text_fixedwidth(ibuf, "Schedules:", offsetx2, offsety2, 8, fontcolor.colors);

			int        types[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
			Tile_coord tile[8];

			for (const auto& sched : *scheds) {
				const int time = sched.get_time();
				types[time]    = sched.get_type();
				tile[time]     = sched.get_pos();
			}

			for (int i = 0; i < 8; i++) {
				int h12 = (i % 4) * 3;
				h12     = h12 ? h12 : 12;

				snprintf(buf, sizeof(buf), "%2i %2s:", h12, i < 4 ? Strings::am() : Strings::pm());
				font->paint_text_fixedwidth(ibuf, buf, offsetx, (36 - offsety1) + i * 8, 8, fontcolor.colors);
				if (types[i] != -1) {
					snprintf(
							buf, sizeof(buf), "%2i (%4i,%4i,%2i) - %s", types[i], tile[i].tx, tile[i].ty, tile[i].tz,
							Strings::ScheduleActivity[types[i]]);
					font->paint_text_fixedwidth(ibuf, buf, offsetx + 56, (36 - offsety1) + i * 8, 8, fontcolor.colors);
				}
			}
		}
		return true;
	};
	return menu;
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
		// NPCDisplay(actor, num);
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
		// NPCDisplay(actor, num);
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
		// NPCDisplay(actor, npc_num);
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
