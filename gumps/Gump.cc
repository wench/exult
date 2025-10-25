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

#include "Gump.h"

#include "Gump_button.h"
#include "Gump_manager.h"
#include "U7obj.h"
#include "cheat.h"
#include "contain.h"
#include "data/exult_bg_flx.h"
#include "data/exult_si_flx.h"
#include "databuf.h"
#include "game.h"
#include "gamewin.h"
#include "misc_buttons.h"
#include "msgfile.h"
#include "objiter.h"
#include "utils.h"

#include <algorithm>
#include <cctype>
#include <charconv>

std::unique_ptr<Text_msg_file_reader> Gump::gump_area_info;

/*
 *  Create a gump.
 */

Gump::Gump(
		Container_game_object* cont,    // Container it represents.
		int initx, int inity,           // Coords. on screen.
		int       shnum,                // Shape #.
		ShapeFile shfile)
		: Gump_Base(shnum, shnum == -1 ? -1 : 0, shfile), container(cont),
		  x(initx), y(inity),
		  handles_kbd(false) {
	if (container) {
		if (container->validGumpXY()) {
			x = container->getGumpX();
			y = container->getGumpY();
		}
	}
}

/*
 *  Create, centered on screen.
 */

Gump::Gump(
		Container_game_object* cont,     // Container it represents.
		int                    shnum,    // Shape #.
		ShapeFile              shfile)
		: Gump_Base(shnum, shnum == -1 ? -1 : 0, shfile), container(cont),
		  handles_kbd(false) {
	set_pos();
}

/*
 *  Create for cloning.
 */

Gump::Gump(Container_game_object* cont, int initx, int inity, Gump* from)
		: Gump_Base(
				  from->get_shapenum(), from->get_framenum(),
				  from->get_shapefile()),
		  container(cont), x(initx), y(inity), object_area(from->object_area),
		  handles_kbd(false) {
	// Clone widgets.
	for (auto* elem : from->elems) {
		add_elem(elem->clone(this));
	}
}

/*
 *  Delete gump.
 */

Gump::~Gump() {
	for (auto* elem : elems) {
		delete elem;
	}
	if (container) {    // Probabbly dont need to check.. but would crash the
						// game if it was NULL.
		container->setGumpXY(x, y);
	}
}

/*
 *   Set centered.
 */

void Gump::set_pos() {
	// reset coords to 0 while getting rect
	x         = 0;
	y         = 0;
	auto rect = get_rect();
	// mark old position dirty
	gwin->add_dirty(rect);
	x = (gwin->get_width() - rect.w) / 2 - rect.x;
	y = (gwin->get_height() - rect.h) / 2 - rect.y;

	// mark new position dirty
	gwin->add_dirty(get_rect());
}

void Gump::set_pos(int newx, int newy) {    // Set new spot on screen.
	if (x == newx && y == newy) {
		return;
	}
	// mark old position dirty
	gwin->add_dirty(get_rect());
	x = newx;
	y = newy;
	// mark new position dirty
	gwin->add_dirty(get_rect());
}

/*
 *  Sets object area and creates checkmark button
 */

static bool read_int_and_advance(std::string_view& line, int& val) {
	// Remove whitspace from start
	while (line.size() && std::isspace(line.front())) {
		line.remove_prefix(1);
	}

	if (!line.size()) {
		return false;
	}

	// find the comma or end
	size_t comma = line.find(',', 0);
	if (comma == line.npos) {
		comma = line.size();
	}
	auto sub = line.substr(0, comma);

	// remove white space at end of subsctring befor comma
	while (sub.size() && std::isspace(sub.back())) {
		sub.remove_suffix(1);
	}

	if (!sub.size()) {
		return false;
	}

	auto res = std::from_chars(sub.data(), sub.data() + sub.size(), val, 10);
	if (res.ptr != sub.data() + sub.size()) {
		return false;
	}

	if (comma + 1 >= line.size()) {
		line = std::string_view();
	} else {
		line = line.substr(comma + 1);
	}

	return true;
}

void Gump::set_object_area(
		TileRect area, int checkx, int checky, bool set_check) {
	if (!gump_area_info) {
		File_spec flx;
		if (GAME_BG) {
			flx = File_spec(
					BUNDLE_CHECK(BUNDLE_EXULT_BG_FLX, EXULT_BG_FLX),
					EXULT_BG_FLX_GUMP_AREA_INFO_TXT);
		} else if (GAME_SI) {
			flx = File_spec(
					BUNDLE_CHECK(BUNDLE_EXULT_SI_FLX, EXULT_SI_FLX),
					EXULT_SI_FLX_GUMP_AREA_INFO_TXT);
		}

		IExultDataSource datasource(
				flx, GUMP_AREA_INFO, PATCH_GUMP_AREA_INFO, 0);
		gump_area_info = std::make_unique<Text_msg_file_reader>(datasource);
	}

	// if we sucesfully read it  try to use it
	if (gump_area_info && get_shapenum() >= 0
		&& get_shapefile() == SF_GUMPS_VGA) {
		auto section = gump_area_info->get_global_section();
		if (size_t(get_shapenum()) < section.size()) {
			auto osv = section[(get_shapenum())];
			auto sv  = osv.value_or(std::string_view());
			if (sv.size()) {
				// Read 6 ints
				int  vals[6];
				bool success = true;

				for (int& v : vals) {
					if (!(success = read_int_and_advance(sv, v))) {
						break;
					}
				}

				// succeeded in parsing line, so update the values
				if (success) {
					area.x = vals[0];
					area.y = vals[1];
					area.w = vals[2];
					area.h = vals[3];
					checkx = vals[4];
					checky = vals[5];
				} else {
					std::cerr << "Failed to parse line in "
								 "gump_area_info.txt for gump "
							  << get_shapenum() << std::endl;
				}
			}
		}
	}
	object_area = area;
	if (set_check
		&& std::none_of(elems.begin(), elems.end(), [](auto elem) -> bool {
			   return dynamic_cast<Checkmark_button*>(elem) != nullptr;
		   })) {
		checkx += 16;
		checky -= 12;
		elems.push_back(new Checkmark_button(this, checkx, checky));
	}
}

/*
 *  Get screen rectangle for one of our objects.
 */

TileRect Gump::get_shape_rect(const Game_object* obj) const {
	const Shape_frame* s = obj->get_shape();
	if (!s) {
		return TileRect(0, 0, 0, 0);
	}
	return TileRect(
			x + object_area.x + obj->get_tx() - s->get_xleft(),
			y + object_area.y + obj->get_ty() - s->get_yabove(), s->get_width(),
			s->get_height());
}

/*
 *  Get screen location of object within.
 */

void Gump::get_shape_location(const Game_object* obj, int& ox, int& oy) const {
	ox = x + object_area.x + obj->get_tx();
	oy = y + object_area.y + obj->get_ty();
}

/*
 *  Find object a screen point is on.
 *
 *  Output: Object found, or null.
 */

Game_object* Gump::find_object(
		int mx, int my    // Mouse pos. on screen.
) {
	int          cnt = 0;
	Game_object* list[100];
	if (!container) {
		return nullptr;
	}
	Object_iterator next(container->get_objects());
	Game_object*    obj;
	Shape_frame*    s;

	int ox;
	int oy;

	while ((obj = next.get_next()) != nullptr) {
		const TileRect box = get_shape_rect(obj);
		if (box.has_point(mx, my)) {
			s = obj->get_shape();
			get_shape_location(obj, ox, oy);
			if (s->has_point(mx - ox, my - oy)) {
				list[cnt++] = obj;
			}
		}
	}
	// ++++++Return top item.
	return cnt ? list[cnt - 1] : nullptr;
}

/*
 *  Get the entire screen rectangle covered by this gump and its contents.
 */

TileRect Gump::get_dirty() {
	TileRect rect = get_rect();
	if (!container) {
		return rect;
	}
	Object_iterator next(container->get_objects());
	Game_object*    obj;
	while ((obj = next.get_next()) != nullptr) {
		const TileRect orect = get_shape_rect(obj);
		rect                 = rect.add(orect);
	}
	return rect;
}

/*
 *  Get object this belongs to.
 */

Game_object* Gump::get_owner() {
	return container;
}

/*
 *  Is a given screen point on the checkmark?
 *
 *  Output: ->button if so.
 */

Gump_button* Gump::on_button(
		int mx, int my    // Point in window.
) {
	for (auto* w : elems) {
		if (w->on_button(mx, my)) {
			return w->as_button();
		}
	}
	return nullptr;
}

/*
 *  Add an object.  If mx, my, sx, sy are all -1, the object's position
 *  is calculated by 'paint()'.  If they're all -2, it's assumed that
 *  obj->tx, obj->ty are already correct.
 *
 *  Output: false if cannot add it.
 */

bool Gump::add(
		Game_object* obj, int mx, int my,    // Mouse location.
		int sx, int sy,     // Screen location of obj's hotspot.
		bool dont_check,    // Skip volume check.
		bool combine        // True to try to combine obj.  MAY
							//   cause obj to be deleted.
) {
	ignore_unused_variable_warning(combine);
	if (!container
		|| (!cheat.in_hack_mover() && !dont_check
			&& !container->has_room(obj))) {
		return false;    // Full.
	}
	// Dropping on same thing?
	Game_object* onobj = find_object(mx, my);
	// If possible, combine.

	if (onobj && onobj != obj && onobj->drop(obj)) {
		return true;
	}

	if (!container->add(obj, dont_check)) {    // DON'T combine here.
		return false;
	}

	// Not a valid spot?
	if (sx == -1 && sy == -1 && mx == -1 && my == -1) {
		// Let paint() set spot.
		obj->set_shape_pos(255, 255);
	}
	// -2's mean tx, ty are already set.
	else if (sx != -2 && sy != -2 && mx != -2 && my != -2) {
		// Put it where desired.
		sx -= x + object_area.x;    // Get point rel. to object_area.
		sy -= y + object_area.y;
		Shape_frame* shape = obj->get_shape();
		// But shift within range.
		if (sx - shape->get_xleft() < 0) {
			sx = shape->get_xleft();
		} else if (sx + shape->get_xright() > object_area.w) {
			sx = object_area.w - shape->get_xright();
		}
		if (sy - shape->get_yabove() < 0) {
			sy = shape->get_yabove();
		} else if (sy + shape->get_ybelow() > object_area.h) {
			sy = object_area.h - shape->get_ybelow();
		}
		obj->set_shape_pos(sx, sy);
	}
	return true;
}

/*
 *  Remove object.
 */

void Gump::remove(Game_object* obj) {
	container->remove(obj);

	// Paint Objects
	TileRect box = object_area;    // Paint objects inside.
	box.shift(x, y);               // Set box to screen location.

	gwin->set_all_dirty();
	gwin->paint_dirty();
}

/*
 *  Paint all elems.
 */

void Gump::paint_elems() {
	for (auto* elem : elems) {
		elem->paint();
	}
}

void check_elem_positions(Object_list& objects) {
	int             prevx = -1;
	int             prevy = -1;
	Game_object*    obj;
	Object_iterator next(objects);
	// See if all have the same position, indicating from a new game.
	while ((obj = next.get_next()) != nullptr) {
		const int tx = obj->get_tx();
		const int ty = obj->get_ty();
		if (prevx == -1) {
			prevx = tx;
			prevy = ty;
		} else if (tx != prevx || ty != prevy) {
			return;
		}
	}
	next.reset();
	while ((obj = next.get_next()) != nullptr) {
		obj->set_shape_pos(255, 255);
	}
}

/*
 *  Paint on screen.
 */

void Gump::paint() {
	// Paint the gump itself.
	if (get_shape()) {
		paint_shape(x, y);
	}
	gwin->set_painted();

	// Paint red "checkmark".
	paint_elems();

	if (!container) {
		return;    // Empty.
	}
	Object_list& objects = container->get_objects();
	if (objects.is_empty()) {
		return;    // Empty.
	}
	TileRect box = object_area;    // Paint objects inside.
	box.shift(x, y);               // Set box to screen location.
	int       cury = 0;
	int       curx = 0;
	const int endy = box.h;
	const int endx = box.w;
	int       loop = 0;               // # of times covering container.
	check_elem_positions(objects);    // Set to place if new game.
	Game_object*    obj;
	Object_iterator next(objects);
	while ((obj = next.get_next()) != nullptr) {
		Shape_frame* shape = obj->get_shape();
		if (!shape) {
			continue;
		}
		const int objx = obj->get_tx() - shape->get_xleft() + 1 + object_area.x;
		const int objy
				= obj->get_ty() - shape->get_yabove() + 1 + object_area.y;
		// Does obj. appear to be placed?
		if (!object_area.has_point(objx, objy)
			|| !object_area.has_point(
					objx + shape->get_xright() - 1,
					objy + shape->get_ybelow() - 1)) {
			// No.
			int px = curx + shape->get_width();
			int py = cury + shape->get_height();
			if (px > endx) {
				px = endx;
			}
			if (py > endy) {
				py = endy;
			}
			obj->set_shape_pos(
					px - shape->get_xright(), py - shape->get_ybelow());
			// Mostly avoid overlap.
			curx += shape->get_width() - 1;
			if (curx >= endx) {
				cury += 8;
				curx = 0;
				if (cury >= endy) {
					cury = 2 * (++loop);
				}
			}
		}
		obj->paint_shape(box.x + obj->get_tx(), box.y + obj->get_ty());
	}
	// Outline selections in this gump.
	const Game_object_shared_vector& sel = cheat.get_selected();
	for (const auto& it : sel) {
		Game_object* obj = it.get();
		if (container == obj->get_owner()) {
			int x;
			int y;
			get_shape_location(obj, x, y);
			obj->ShapeID::paint_outline(x, y, HIT_PIXEL);
		}
	}
}

/*
 *  Close and delete.
 */

void Gump::close() {
	gumpman->close_gump(this);
}

/*
 *  Does the gump have this spot
 */
bool Gump::has_point(int sx, int sy) const {
	Shape_frame* s = get_shape();

	return s && s->has_point(sx - x, sy - y);
}


bool Gump::isOffscreen(bool partially) const {
	auto rect = get_rect();

	if (partially) {
		return rect.x < 0 || rect.y < 0
			   || (rect.x + rect.w) > gwin->get_game_width()
			   || (rect.y + rect.h) > gwin->get_game_height();
	} else {
		return rect.x >= gwin->get_game_width()
			   || rect.y >= gwin->get_game_height() || (rect.x + rect.w) <= 0
			   || (rect.y + rect.h) <= 0;
	}
}

/*
 *  Container_gump Initialize
 */

void Container_gump::initialize(int shnum) {
	if (shnum == game->get_shape("gumps/box")) {
		set_object_area(TileRect(46, 28, 74, 32), 8, 56);
	} else if (shnum == game->get_shape("gumps/crate")) {
		set_object_area(TileRect(50, 20, 80, 24), 8, 64);
	} else if (shnum == game->get_shape("gumps/barrel")) {
		set_object_area(TileRect(32, 32, 40, 40), 12, 124);
	} else if (shnum == game->get_shape("gumps/bag")) {
		set_object_area(TileRect(48, 20, 66, 44), 8, 66);
	} else if (shnum == game->get_shape("gumps/backpack")) {
		set_object_area(TileRect(36, 36, 85, 40), 8, 62);
	} else if (shnum == game->get_shape("gumps/basket")) {
		set_object_area(TileRect(42, 32, 70, 26), 8, 56);
	} else if (shnum == game->get_shape("gumps/chest")) {
		set_object_area(TileRect(40, 18, 60, 37), 8, 46);
	} else if (shnum == game->get_shape("gumps/shipshold")) {
		set_object_area(TileRect(38, 10, 82, 80), 8, 92);
	} else if (shnum == game->get_shape("gumps/drawer")) {
		set_object_area(TileRect(36, 12, 70, 26), 8, 46);
	} else if (shnum == game->get_shape("gumps/tree")) {
		set_object_area(TileRect(62, 22, 36, 44), 9, 100);
	} else if (shnum == game->get_shape("gumps/body")) {
		set_object_area(TileRect(36, 46, 84, 40), 8, 70);
	} else {
		set_object_area(TileRect(52, 22, 60, 40), 8, 64);
	}
}
