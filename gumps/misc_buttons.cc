/*
Copyright (C) 2000-2024 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "misc_buttons.h"

#include "Audio.h"
#include "Gamemenu_gump.h"
#include "Gump_manager.h"
#include "Modal_gump.h"
#include "actors.h"
#include "game.h"
#include "gamewin.h"
#include "mouse.h"
#include "party.h"

/*
 *  A checkmark for closing its parent:
 */
Checkmark_button::Checkmark_button(Gump* par, int px, int py)
		: Gump_button(par, game->get_shape("gumps/check"), px, py) {}

/*
 *  Handle click on a 'checkmark'.
 */

bool Checkmark_button::activate(MouseButton button) {
	if (button != MouseButton::Left) {
		return false;
	}
	Audio::get_ptr()->play_sound_effect(Audio::game_sfx(74));
	parent->close();
	return true;
}

/*
 *  A 'heart' button for bringing up stats.
 */

Heart_button::Heart_button(Gump* par, int px, int py)
		: Gump_button(par, game->get_shape("gumps/heart"), px, py) {}

/*
 *  Handle click on a heart.
 */

bool Heart_button::activate(MouseButton button) {
	if (button != MouseButton::Left) {
		return false;
	}
	gumpman->add_gump(
			parent->get_container(), game->get_shape("gumps/statsdisplay"));
	return true;
}

/*
 *  A diskette for bringing up the 'save' box.
 */

Disk_button::Disk_button(Gump* par, int px, int py)
		: Gump_button(par, game->get_shape("gumps/disk"), px, py) {}

/*
 *  Handle click on a diskette.
 */

bool Disk_button::activate(MouseButton button) {
	if (button != MouseButton::Left) {
		return false;
	}
	auto* menu = new Gamemenu_gump();
	gumpman->do_modal_gump(menu, Mouse::hand);
	delete menu;
	return true;
}

/*
 *  The combat toggle button.
 */

Combat_button::Combat_button(Gump* par, int px, int py)
		: Gump_button(par, game->get_shape("gumps/combat"), px, py) {
	set_pushed(gwin->in_combat());
}

/*
 *  Handle click on a combat toggle button.
 */

bool Combat_button::activate(MouseButton button) {
	if (button != MouseButton::Left) {
		return false;
	}
	gwin->toggle_combat();
	set_pushed(gwin->in_combat());
	parent->paint();
	return true;
}

/*
 *  Check combat mode before painting.
 */

void Combat_button::paint() {
	set_pushed(gwin->in_combat());
	Gump_button::paint();
}

/*
 *  The halo button.
 */

Halo_button::Halo_button(Gump* par, int px, int py, Actor* a)
		: Gump_button(par, game->get_shape("gumps/halo"), px, py), actor(a) {
	set_pushed(actor->is_combat_protected());
}

/*
 *  Handle click on a halo toggle button.
 */

bool Halo_button::activate(MouseButton button) {
	if (button != MouseButton::Left) {
		return false;
	}
	// Want to toggle it.
	const bool prot = !actor->is_combat_protected();
	set_pushed(prot);
	parent->paint();
	actor->set_combat_protected(prot);
	if (!prot) {    // Toggled off?
		return true;
	}
	// On?  Got to turn off others.
	Actor*    party[9];    // Get entire party, including Avatar.
	const int cnt = gwin->get_party(party, 1);
	for (int i = 0; i < cnt; i++) {
		if (party[i] != actor && party[i]->is_combat_protected()) {
			party[i]->set_combat_protected(false);
		}
		// +++++Should also update gumps.
	}
	return true;
}

/*
 *  Combat mode.  Has 10 frames corresponding to Actor::Attack_mode.
 */

Combat_mode_button::Combat_mode_button(Gump* par, int px, int py, Actor* a)
		: Gump_button(par, game->get_shape("gumps/combatmode"), px, py),
		  actor(a) {
	set_frame(static_cast<int>(actor->get_attack_mode()));
}

/*
 *  Handle click on a combat toggle button.
 */

bool Combat_mode_button::activate(MouseButton button) {
	if (button != MouseButton::Left) {
		return false;
	}
	// Only Avatar gets last frame (manual)
	const int nframes = actor == gwin->get_main_actor() ? 10 : 9;
	set_frame((get_framenum() + 1) % nframes);
	// Flag that player set the mode.
	actor->set_attack_mode(
			static_cast<Actor::Attack_mode>(get_framenum()), true);
	paint();
	gwin->set_painted();
	return true;
}

/*
 *  The Serpent Isle Combat Stats Button.
 */

Cstats_button::Cstats_button(Gump* par, int px, int py)
		: Gump_button(par, game->get_shape("gumps/combat_stats"), px, py)

{}

/*
 *  Handle click on a combat stats button
 */

bool Cstats_button::activate(MouseButton button) {
	if (button != MouseButton::Left) {
		return false;
	}
	const int cnt = partyman->get_count();
	gumpman->add_gump(nullptr, game->get_shape("gumps/cstats/1") + cnt);
	return true;
}

Arrow_Button::Arrow_Button(
		Gump_Base* parent, int px, int py, Direction direction,
		bool double_arrow)
		: Basic_button(
				  parent, px, py, direction >= Left ? 12 : 11,
				  direction >= Left ? 11 : 12),
		  direction(direction), double_(double_arrow) {}

void Arrow_Button::paint() {
	Basic_button::paint();
	auto ib8       = Shape_frame::get_to_render();
	auto draw_area = get_draw_area();
	local_to_screen(draw_area.x , draw_area.y);
	if (!double_) {
		// Single arrow
		for (int l = 0; l < 4; l++) {
			int startx = 3 - l;
			int endx   = 3 + l;
			int starty = 2 + l;

			// If down or right flip the y
			if (direction == Down || direction == Right) {
				starty = 7 - starty;
			}

			int endy = starty;
			// if left or right swap x and y
			if (direction >= Left) {
				std::swap(startx, starty);
				std::swap(endx, endy);
			}

			if (startx == endx && starty == endy) {
				ib8->put_pixel8(0, draw_area.x + startx, draw_area.y + starty);
			} else {
				ib8->draw_line8(
						0, draw_area.x + startx, draw_area.y + starty,
						draw_area.x + endx, draw_area.y + endy);
			}
		}

	} else {
		// double arrow
		for (int l = 0; l < 6; l++) {
			int l3     = l % 3;
			int startx = 3 - l3;
			int endx   = 3 + l3;
			int starty = 1 + l;

			// If down or right flip the y
			if (direction == Down || direction == Right) {
				starty = 7 - starty;
			}

			int endy = starty;

			// if left or right swap x and y
			if (direction >= Left) {
				std::swap(startx, starty);
				std::swap(endx, endy);
			}

			if (startx == endx && starty == endy) {
				ib8->put_pixel8(0, draw_area.x + startx, draw_area.y + starty);
			} else {
				ib8->draw_line8(
						0, draw_area.x + startx, draw_area.y + starty,
						draw_area.x + endx, draw_area.y + endy);
			}
		}
	}
}
