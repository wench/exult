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

CheatScreen::InputHandlers::Actor::Actor(bool empty_allowed, std::string&& promptmsg)
		: GameObject(empty_allowed, std::move(promptmsg)) {
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsety = 90;
#else
	const int offsety = cscreen->maxy - 9;
#endif
	hotspots.pop_back();
	const char* label = Strings::Pick_NPC_from_World;
	hotspots.emplace_back(0, offsety, label[0], label + 1);
	hotspots.back().PositionLeftOf();
}

CheatScreen::InputHandlers::Actor::Actor(bool empty_allowed) : Actor(empty_allowed, Strings::WHICH_NPC) {}

void CheatScreen::InputHandlers::Actor::Parse() {
	if (!actor) {
		GameObject::Parse();
		actor = dynamic_cast<::Actor*>(object);
	}

	if (!actor) {
		throw MenuCommandException{Strings::INVALID_NPC, false};
	}
}

//
// NPC Menu
//

std::shared_ptr<CheatScreen::Menu> CheatScreen::NPCMenu(Actor* actor) {
	std::forward_list<Menu::Item> items;
	std::shared_ptr<MenuCommand>  command;
	const char*                   label;

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
	auto hideifnoactor = [](MenuCommand* self) {
		bool hide = !MenuCommand::getDataOrDefault<Actor*>(self->GetMyMenu());
		if (auto hotspot = self->getHotspot()) {
			hotspot->setHidden(0, hide);
			hotspot->setHidden(1, hide);
		}
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
		return NPCFlagMenu(MenuCommand::getDataOrDefault<Actor*>(self->GetMyMenu()));
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

	// stats -  This is its own menu but we generate it here instead of using a separate menu function
	// because it's so simple
	std::forward_list<Menu::Item> stats_items;
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
					if (auto hotspot = self->getHotspot()) {
						char buf[16];
						snprintf(buf, sizeof(buf), "%3d", actor->get_property(prop));
						hotspot->label_rw = hotspot->label + buf;
					}
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
			if (!actor->get_chunk()) {
				// Should not get here if NPC not on map but just in case don't allow it
				throw MenuCommandException{Strings::TeleportMenu::CantTeleporttoNPCnotonmap, false};
			} else {
				Game_window::get_instance()->teleport_party(actor->get_tile(), false, actor->get_map_num());
			}
		}
		return {};
	};
	command->events.run = [](MenuCommand* self) {
		Actor* actor = MenuCommand::getDataOrDefault<Actor*>(self->GetMyMenu());
		// If the actor is not on the map hide teleport to NPC hotspot
		bool hide = !actor || !actor->get_chunk();
		if (auto hotspot = self->getHotspot()) {
			hotspot->setHidden(0, hide);
		}
	};
	label = Strings::NPCMenu::TeleporttoNPC;
	items.emplace_front(Hotspot(offsetx + 160, maxy - offsety1 - 81, label[0], label + 1), command);

	// Palette Effect
	command                  = std::make_shared<MenuCommand>();
	command->events.run      = hideifnoactor;
	command->events.Activate = [this](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		auto menu = self->GetMyMenu();
		if (Actor* actor = MenuCommand::getDataOrDefault<Actor*>(menu)) {
			return PalEffectMenu(actor);
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
		auto npcih = static_cast<InputHandlers::Actor*>(self->inputs[0].get());
		if (npcih->actor) {
			auto menu = self->GetMyMenu();
			menu->setData<Actor*>(npcih->actor);
		}

		return {};
	};
	command->inputs.push_back(std::make_shared<InputHandlers::Actor>(false));

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
			num = actor->get_npc_num();

			// Paint the actors shape
			Shape_frame* shape = actor->get_shape();
			if (shape) {
				actor->paint_shape(shape->get_xright() + 280, shape->get_yabove(), true);
			}

			// Now the info
			const std::string namestr = actor->get_npc_name();
			snprintf(
					buf, sizeof(buf), "%s%i - %s%s", Strings::NPCMenu::NPC_(), num, namestr.c_str(),
					actor->is_unused() ? Strings::NPCMenu::unused() : "");
			font->paint_text_fixedwidth(ibuf, buf, offsetx, 0, 8, fontcolor.colors);

			if (actor->get_chunk()) {
				const Tile_coord t = actor->get_tile();
				snprintf(
						buf, sizeof(buf), "%s(%04i, %04i, %02i) %s%i", Strings::NPCMenu::Loc_(), t.tx, t.ty, t.tz,
						Strings::TeleportMenu::OnMap(), actor->get_map_num());
				font->paint_text_fixedwidth(ibuf, buf, offsetx, 9, 8, fontcolor.colors);
			} else {
				font->paint_text_fixedwidth(ibuf, Strings::NPCMenu::NoLocNotonMap, offsetx, 9, 8, fontcolor.colors);
			}

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
						buf, sizeof(buf), "%s0x%04x%s0x%x", Strings::NPCMenu::Usecodeitem(), ucitemnum,
						Strings::NPCMenu::function(), actor->get_usecode());
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

std::shared_ptr<CheatScreen::Menu> CheatScreen::NPCFlagMenu(Actor* actor) {
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx  = 10;
	const int offsety1 = 92;
	const int offsetx1 = 6;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	const int offsetx1 = 0;
#endif
	if (!actor) {
		return {};
	}

	//
	// Menu layout is mostly automated being generated from 3 arrays containing the flags that should be in each of the three
	// columns
	//
	std::shared_ptr<MenuCommand>  command;
	std::forward_list<Menu::Item> items;

	enum FlagType {
		NotAFlag   = 0,       // entry is not a flag and Menu Item should not be shown unless custom Menu Comand is supplied
		Unknown    = 1,       // entry is an unknoen flag. It is drawn as a flag that is false. Interacting with it does nothing
		ObjFlag    = 2,       // Entry is from Obj_flags
		TypeFlag   = 3,       // Entry is an Actor Type Flag
		AvatarOnly = 256,     // Should be shown for Avatar Only
		PartyOnly  = 512,     // Should be shown for Party members only (inc Avatar)
		NotAvatar  = 1024,    // Should be shown for everyone except the Avatar
		Mask       = AvatarOnly | PartyOnly | NotAvatar
	};

	struct Flag_s {
		SDL_Keycode                  keycode;        // If 0, keycode is taken to be first character of label
		int                          label_index;    // in Strings::NPCFlagsMenu::MenuItems
		int                          flag_number;
		std::shared_ptr<MenuCommand> command; /*custom MenuCommand to use instead of auto generated one. Required for NotAFlag*/

		int                               FlagType;
		const std::array<const char*, 2>& false_true_strings;
	};

	// Flag state String arrays
	// These two need to be static because they do not need to be translatable and their life time needs to exist at least as long
	// as the menu non static lifetime ends when the method returns but the menu is returned by the method
	static std::array<const char*, 2> empty   = {};
	static std::array<const char*, 2> unknown = {"?", "?"};
	// These 2 are translatable so they can be const refernces because the lifetime of the array owned by the string object is going
	// to be longer than this menu there is nothing in this menu that can cause message strings to be reloaded, invalidting them so
	// the string array life time will be longer than the menu
	const std::array<const char*, 2>& NY = Strings::NPCFlagsMenu::NY.ToArray();
	const std::array<const char*, 2>& MF = Strings::NPCFlagsMenu::MF.ToArray();

	// Flags Column Arrrays - These are used to generate each menu column
	Flag_s flags_left[] = {
			{      0, 0,     Obj_flags::asleep, nullptr,  ObjFlag,    NY},
            {      0, 1,    Obj_flags::charmed, nullptr,  ObjFlag,    NY},
			{      0, 2,     Obj_flags::cursed, nullptr,  ObjFlag,    NY},
            {      0, 3,  Obj_flags::paralyzed, nullptr,  ObjFlag,    NY},
			{      0, 4,   Obj_flags::poisoned, nullptr,  ObjFlag,    NY},
            {      0, 5, Obj_flags::protection, nullptr,  ObjFlag,    NY},
			{      0, 6, Obj_flags::tournament, nullptr,  ObjFlag,    NY},
            {      0, 7,  Obj_flags::polymorph, nullptr,  ObjFlag,    NY},
			{SDLK_UP, 8,                     0, nullptr, NotAFlag, empty}, // Advanced
	};
	Flag_s flags_centre[] = {
			{0,  9,  Obj_flags::in_party, nullptr,              ObjFlag,      NY},
			{0, 10, Obj_flags::invisible, nullptr,              ObjFlag,      NY},
			{0, 11,        Actor::tf_fly, nullptr,             TypeFlag,      NY},
			{0, 12,       Actor::tf_walk, nullptr,             TypeFlag,      NY},
			{0, 13,       Actor::tf_swim, nullptr,             TypeFlag,      NY},
			{0, 14,   Actor::tf_ethereal, nullptr,             TypeFlag,      NY},
			{0, 15,					0, nullptr,              Unknown, unknown}, // Prtcee
			{0, 16,   Actor::tf_conjured, nullptr,             TypeFlag,      NY},
			{0, 17,     Obj_flags::naked, nullptr, ObjFlag | AvatarOnly,      NY},
	};
	Flag_s flags_right[] = {
			{0, 18,         Actor::tf_summonned, nullptr,              TypeFlag,    NY},
			{0, 19,          Actor::tf_bleeding, nullptr,              TypeFlag,    NY},
			{0, 20,               Actor::tf_sex, nullptr, TypeFlag | AvatarOnly,    MF},
			{0, 21,						   0, nullptr, NotAFlag | AvatarOnly, empty}, // Skin
			{0, 22,             Obj_flags::read, nullptr,  ObjFlag | AvatarOnly,    NY},
			{0, 23,              Obj_flags::met, nullptr,   ObjFlag | NotAvatar,    NY},
			{0, 24, Obj_flags::no_spell_casting, nullptr,   ObjFlag | NotAvatar,    NY},
			{0, 25,						   0, nullptr,  NotAFlag | NotAvatar, empty}, // ID
			{0, 26,           Obj_flags::freeze, nullptr,               ObjFlag,    NY},
			{0, 27,						   0, nullptr,              NotAFlag, empty}, // Temp
			{0, 28,						   0, nullptr,              NotAFlag, empty}, //  Warmth
			{0, 29,            Obj_flags::petra, nullptr,  ObjFlag | AvatarOnly,    NY},
	};
	// MenuCommand for Skin (right column)
	flags_right[3].command = command = std::make_shared<MenuCommand>();
	command->events.run              = [actor](MenuCommand* self) {
        if (auto hotspot = self->getHotspot()) {
            hotspot->setHidden(0, actor->get_npc_num() != 0);
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", actor->get_skin_color());
            hotspot->label_rw = hotspot->label + buf;
        }
	};
	command->events.Activate = [actor](MenuCommand*, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		actor->set_skin_color(Shapeinfo_lookup::GetNextSkin(
				actor->get_skin_color(), actor->get_type_flag(Actor::tf_sex), Shape_manager::get_instance()->have_si_shapes()));
		return {};
	};
	// MenuCommand for Temperature (right column)
	flags_right[9].command = command = std::make_shared<MenuCommand>();
	command->inputs.push_back(
			std::make_shared<InputHandlers::Integer>(false, 0, 63, false, Strings::ENTER_TEMPERATURE, Strings::INVALID_VALUE));

	command->events.run = [actor](MenuCommand* self) {
		if (auto hotspot = self->getHotspot()) {
			char buf[8];
			snprintf(buf, sizeof(buf), " %02i", actor->get_temperature());
			hotspot->label_rw = hotspot->label + buf;
		}
	};
	command->events.Activate = [actor](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		actor->set_temperature(static_cast<InputHandlers::Integer*>(self->inputs[0].get())->value);
		return {};
	};
	// MenuCommand for ID#: (right column)
	flags_right[7].command = command = std::make_shared<MenuCommand>();
	command->events.run              = [actor](MenuCommand* self) {
        if (auto hotspot = self->getHotspot()) {
            int num = actor->get_npc_num();
            hotspot->setHidden(0, num == 0);
            hotspot->label_only = num != 0;
            char buf[8];
            snprintf(buf, sizeof(buf), " %02i", actor->get_ident());
            hotspot->label_rw = hotspot->label + buf;
        }
	};
	// MenuCommand for Warmth (right column)
	flags_right[10].command = command = std::make_shared<MenuCommand>();
	command->events.run               = [actor](MenuCommand* self) {
        if (auto hotspot = self->getHotspot()) {
            hotspot->label_only = true;
            char buf[8];
            snprintf(buf, sizeof(buf), " %04i", actor->figure_warmth());
            hotspot->label_rw = hotspot->label + buf;
        }
	};
	// MenuCommand for advanced flags  (left column) This is a menu
	flags_left[8].command = command = std::make_shared<MenuCommand>();
	command->inputs.push_back(
			std::make_shared<InputHandlers::Integer>(false, 0, 63, false, Strings::ENTER_NPC_FLAG, Strings::INVALID_VALUE));
	command->events.Activate = [actor, this](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		return AdvancedFlagMenu(static_cast<InputHandlers::Integer*>(self->inputs[0].get())->value, actor);
	};

	// Generate all the menu items for a column
	auto do_column = [&](tcb::span<Flag_s> column, int offsetx) {
		int offsety  = maxy - offsety1 - 108;
		int avtype   = 0;
		int runstart = offsety;
		for (auto& flag : column) {
			int flagtype = flag.FlagType;
			// Special Handling so AvatarOnly and NotAvatar flags can take up the same rows when drawn
			if ((flagtype & (AvatarOnly | NotAvatar)) != avtype) {
				bool wasall = avtype == 0;

				if (wasall) {
					runstart = offsety;
				} else if (flagtype & (AvatarOnly | NotAvatar)) {
					offsety = runstart;
				}
			}
			avtype = flagtype & (AvatarOnly | NotAvatar);

			int                          flagnum = flag.flag_number;
			std::shared_ptr<MenuCommand> command = flag.command;
			SDL_Keycode                  keycode = flag.keycode;
			const char*                  label   = Strings::NPCFlagsMenu::MenuItems[flag.label_index];

			if (!command && ((flagtype & ~Mask) != NotAFlag)) {
				command             = std::make_shared<ToggleCommand>(false, flag.false_true_strings);
				command->events.run = [flagnum, flagtype, actor](MenuCommand* self) {
					if (auto hotspot = self->getHotspot()) {
						hotspot->setHidden(0, false);
						auto menu = self->GetMyMenu()->GetMyMenu();
						hotspot->setHidden(
								0, (flagtype & AvatarOnly && actor->get_npc_num() != 0)
										   || (flagtype & NotAvatar && actor->get_npc_num() == 0)
										   || (flagtype & PartyOnly && !actor->is_in_party()));
					}

					auto& state = static_cast<ToggleCommand*>(self)->state;
					if ((flagtype & ~Mask) == ObjFlag) {
						state = actor->get_flag(flagnum);
					} else if ((flagtype & ~Mask) == TypeFlag) {
						state = actor->get_type_flag(flagnum);
					}
				};
				command->events.Activate
						= [flagnum, flagtype, actor, this](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
					if ((flagtype & AvatarOnly && actor->get_npc_num() != 0) || (flagtype & NotAvatar && actor->get_npc_num() == 0)
						|| (flagtype & PartyOnly && !actor->is_in_party())) {
						throw MenuCommandException{Strings::INVALID_COMMAND, true};
					}

					auto state = static_cast<ToggleCommand*>(self)->state;

					if ((flagtype & ~Mask) == ObjFlag) {
						if (state) {
							actor->set_flag(flagnum);
						} else {
							actor->clear_flag(flagnum);
						}
					} else if ((flagtype & ~Mask) == TypeFlag) {
						if (state) {
							actor->set_type_flag(flagnum);
						} else {
							actor->clear_type_flag(flagnum);
						}
					}
					// apply palette because changing flags may change the palette
					pal.apply(false);
					return {};
				};

				flag.command = command;
			}
			if (command) {
				if (label && *label) {
					if (command->events.Activate && !keycode) {
						// Items with no specified key binding but can be activated take their keybinding from the label
						keycode = *label++;
					}
					items.emplace_front(Hotspot(offsetx, offsety, keycode, label), command);
				}
			}
			offsety += 9;
		}
	};
	do_column(flags_left, offsetx);
	do_column(flags_centre, offsetx1 + 104);
	do_column(flags_right, offsetx1 + 208);

	auto menu                  = std::make_shared<Menu>(std::move(items));
	menu->events.paint_display = [](MenuCommand*) -> bool {
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
		// return true if mobile to supress painting of the NPCMenu display
		return true;

#else
		// Not mobile so return false so NPCMenu Display is painted
		return false;
#endif
	};
	return menu;
}

//
// Business Schedules
//

std::shared_ptr<CheatScreen::Menu> CheatScreen::BusinessMenu(Actor* actor) {
	std::forward_list<Menu::Item> items;
	std::shared_ptr<MenuCommand>  command;

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
// Pallete Effect
//

std::shared_ptr<CheatScreen::Menu> CheatScreen::PalEffectMenu(Actor* actor) {
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 81;
	const int offsety2 = 90;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	const int offsety2 = maxy - 9;
#endif
	std::shared_ptr<MenuCommand>  command;
	std::forward_list<Menu::Item> items;
	const char*                   label;

	// Left Column

	// ramp remap
	command               = std::make_shared<MenuCommand>();
	unsigned int numramps = 0;
	gwin->get_pal()->get_ramps(numramps);
	if (numramps) {
		numramps--;
	}
	command->inputs.push_back(std::make_shared<InputHandlers::Integer>(
			false, 0, numramps, false, Strings::PaletteEffect::Prompts::enterFromRamp, Strings::INVALID_VALUE, 255));
	command->inputs[0]->hotspots.emplace_back(0, offsety2, 0, Strings::PaletteEffect::Prompts::or255forall);
	command->inputs[0]->hotspots.back().PositionLeftOf();
	command->inputs.push_back(std::make_shared<InputHandlers::Integer>(
			false, 0, numramps, false, Strings::PaletteEffect::Prompts::enterToRampnumberIndex, Strings::INVALID_VALUE));
	command->events.Activate = [=](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		auto from = static_cast<InputHandlers::Integer*>(self->inputs[0].get());
		auto to   = static_cast<InputHandlers::Integer*>(self->inputs[1].get());
		if (from->value == 255) {
			actor->set_palette_transform(ShapeID::PT_RampRemapAllFrom | (to->value & 31));
		} else {
			actor->set_palette_transform(ShapeID::PT_RampRemap | (to->value & 31) | ((from->value & 0xff) << 5));
		}
		return {};
	};
	command->events.paint_display = [=](MenuCommand* self) -> bool {
		char buf[41];
		if (self->phase > 0) {
			auto from = static_cast<InputHandlers::Integer*>(self->inputs[0].get());

			if (from->value == 255) {
				snprintf(buf, sizeof(buf), "%s", Strings::PaletteEffect::Display::FromRampall());
			} else {
				snprintf(buf, sizeof(buf), "%s%i", Strings::PaletteEffect::Display::FromRamp(), from->value & 31);
			}

			font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 110, 8, fontcolor.colors);
		}
		return false;
	};
	label = Strings::PaletteEffect::MenuItems::RampRemap;
	items.emplace_front(Hotspot(offsetx, maxy - offsety1 - 99, label[0], label + 1), command);

	// xform
	command = std::make_shared<MenuCommand>();
	command->inputs.push_back(std::make_shared<InputHandlers::Integer>(
			false, 0, Shape_manager::get_instance()->get_xforms_cnt(), false, Strings::PaletteEffect::Prompts::enterXFORMIndex));
	command->events.Activate = [=](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		auto x = static_cast<InputHandlers::Integer*>(self->inputs[0].get());
		actor->set_palette_transform(ShapeID::PT_xForm | x->value % int(Shape_manager::get_instance()->get_xforms_cnt()));
		return {};
	};
	label = Strings::PaletteEffect::MenuItems::Xform;
	items.emplace_front(Hotspot(offsetx, maxy - offsety1 - 90, label[0], label + 1), command);

	// Shift
	command = std::make_shared<MenuCommand>();
	command->inputs.push_back(
			std::make_shared<InputHandlers::Integer>(false, 0, 255, false, Strings::PaletteEffect::Prompts::entershiftamount));
	command->events.Activate = [=](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		auto s = static_cast<InputHandlers::Integer*>(self->inputs[0].get());
		actor->set_palette_transform(ShapeID::PT_Shift | (s->value & 0xff));
		return {};
	};
	label = Strings::PaletteEffect::MenuItems::Shift;
	items.emplace_front(Hotspot(offsetx, maxy - offsety1 - 81, label[0], label + 1), command);

	// clear
	command                  = std::make_shared<MenuCommand>();
	command->events.Activate = [=](MenuCommand*, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		actor->set_palette_transform(0);
		return {};
	};
	label = Strings::PaletteEffect::MenuItems::Clear;
	items.emplace_front(Hotspot(offsetx, maxy - offsety1 - 72, label[0], label + 1), command);

	auto menu                  = std::make_shared<Menu>(std::move(items));
	menu->events.paint_display = [=](MenuCommand*) -> bool {
		char buf[41];
		int  pt = actor->get_palette_transform();
		if (pt == 0) {
			snprintf(
					buf, sizeof(buf), "%s%s", Strings::PaletteEffect::Display::PaletteEffect(),
					Strings::PaletteEffect::Display::None());
		} else if ((pt & ShapeID::PT_RampRemapAllFrom) == ShapeID::PT_RampRemapAllFrom) {
			snprintf(
					buf, sizeof(buf), "%s%s%i", Strings::PaletteEffect::Display::PaletteEffect(),
					Strings::PaletteEffect::Display::RampRemapAllTo(), pt & 31);
		} else if ((pt & ShapeID::PT_RampRemap) == ShapeID::PT_RampRemap) {
			snprintf(
					buf, sizeof(buf), "%s%s%i%s%i", Strings::PaletteEffect::Display::PaletteEffect(),
					Strings::PaletteEffect::Display::RampRemap(), (pt >> 5) & 31, Strings::PaletteEffect::Display::To(), pt & 31);
		} else if ((pt & ShapeID::PT_xForm) == ShapeID::PT_xForm) {
			snprintf(
					buf, sizeof(buf), "%s%s%i", Strings::PaletteEffect::Display::PaletteEffect(),
					Strings::PaletteEffect::Display::Xform(), pt & 31);
		} else if (pt < 256) {
			snprintf(
					buf, sizeof(buf), "%s%s%i", Strings::PaletteEffect::Display::PaletteEffect(),
					Strings::PaletteEffect::Display::ShiftBy(), pt & 0xff);
		}
		else {
			buf[0] = 0;
		}
		font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 119, 8, fontcolor.colors);

#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(CHEAT_SCREEN_TEST_MOBILE)
		// Return false so our parent Menu's (NPC Menu) display is shown
		return false;
#else
		// But not on mobile because the changed layout doesn't fit

		// Paint the actors shape here because the NPC menu display wont be painting it
		if (Shape_frame* shape = actor->get_shape()) {
			actor->paint_shape(shape->get_xright() + 240, shape->get_yabove() + 8, true);
		}
		return true;
#endif
	};
	return menu;
}

//
// Advanced Flag Editor
//

std::shared_ptr<CheatScreen::Menu> CheatScreen::AdvancedFlagMenu(unsigned flag_num, Actor* actor) {
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 83;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
#endif

	std::shared_ptr<MenuCommand>  command;
	std::forward_list<Menu::Item> items;

	// "NPC Flag number: name" message
	command             = std::make_shared<MenuCommand>();
	command->events.run = [](MenuCommand* self) {
		if (auto hotspot = self->getHotspot()) {
			auto flag_num = MenuCommand::getDataOrDefault<unsigned>(self->GetMyMenu());

			char buf[8];
			snprintf(buf, sizeof(buf), "%d", flag_num);
			auto flag_name = Strings::NPCFlag[flag_num];

			hotspot->label_rw.reserve(hotspot->label.size() + 4 + flag_name.size());

			hotspot->label_rw = hotspot->label;
			hotspot->label_rw += buf;
			hotspot->label_rw += flag_name;
			hotspot->label_only = true;
		}
	};
	items.emplace_front(Hotspot(offsetx, maxy - offsety1 - 108, 0, Strings::AdvancedFlagsMenu::NPCFlag), command);

	// "Flag is " message
	command             = std::make_shared<MenuCommand>();
	command->events.run = [actor](MenuCommand* self) {
		if (auto hotspot = self->getHotspot()) {
			auto flag_num       = MenuCommand::getDataOrDefault<unsigned>(self->GetMyMenu());
			hotspot->label_rw   = actor->get_flag(flag_num) ? Strings::AdvancedFlagsMenu::FlagIsSET()
															: Strings::AdvancedFlagsMenu::FlagIsUNSET();
			hotspot->label_only = true;
		}
	};
	items.emplace_front(Hotspot(offsetx, maxy - offsety1 - 90, 0, ""), command);

	// toggle command
	command = std::make_shared<MenuCommand>();

	command->events.Activate = [actor](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		auto flag_num = MenuCommand::getDataOrDefault<unsigned>(self->GetMyMenu());
		if (actor->get_flag(flag_num)) {
			actor->clear_flag(flag_num);
		} else {
			actor->set_flag(flag_num);
		}

		return {};
	};
	const char* label = Strings::AdvancedFlagsMenu::ToggleFlag;
	if (label && *label) {
		items.emplace_front(Hotspot(offsetx + 160, maxy - offsety1 - 90, *label, label + 1), command);
	}

	// Change Flag
	command = std::make_shared<MenuCommand>();
	command->inputs.push_back(
			std::make_shared<InputHandlers::Integer>(false, 0, 63, false, Strings::ENTER_NPC_FLAG, Strings::INVALID_VALUE));
	command->events.Activate = [](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		self->GetMyMenu()->setData<int>(static_cast<InputHandlers::Integer*>(self->inputs[0].get())->value);

		return {};
	};
	items.emplace_front(Hotspot(offsetx, maxy - offsety1 - 72, SDLK_UP, " Change Flag"), command);

	// Scroll Flags
	command                  = std::make_shared<LeftRightIntegerCommand>(0, 63, 0, false);
	command->events.Activate = [](MenuCommand* self, SDL_Keycode) -> std::shared_ptr<MenuCommand> {
		self->GetMyMenu()->setData<unsigned>(static_cast<LeftRightIntegerCommand*>(self)->currentval);
		return {};
	};
	command->events.run = [](MenuCommand* self) {
		// Make sure currentval is synced with the flag number
		static_cast<LeftRightIntegerCommand*>(self)->currentval = MenuCommand::getDataOrDefault<unsigned>(self->GetMyMenu());
	};
	items.emplace_front(Hotspot(offsetx, maxy - offsety1 - 63, SDLK_LEFT, "Scroll Flags", SDLK_RIGHT), command);

	auto menu = std::make_shared<Menu>(std::move(items));
	menu->setData<unsigned>(std::min<unsigned>(63, flag_num));
	return menu;
}
