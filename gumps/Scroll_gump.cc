/*
Copyright (C) 2000-2025 The Exult Team

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

#include "Scroll_gump.h"

#include "game.h"
#include "gamewin.h"

namespace {
	// Above normal/hud gumps, below modal gumps.
	constexpr int text_gump_layer_z = (1 << 18) + (1 << 16) + 1;

	bool begin_text_layer_paint(
			Gump* g, int drawx, int drawy, int& ox, int& oy, int& w, int& h, Image_buffer8*& prev_target) {
		if (!g) {
			return false;
		}
		Game_window* gwin = Game_window::get_instance();
		if (!gwin) {
			return false;
		}
		const TileRect r = g->get_rect();
		w                = r.w;
		h                = r.h;
		if (w <= 0 || h <= 0) {
			return false;
		}
		if (g->render_layer < 0 || g->layer_bounds.w != w || g->layer_bounds.h != h) {
			if (g->render_layer >= 0) {
				gwin->destroy_layer(g->render_layer);
				g->render_layer = -1;
			}
			g->render_layer = gwin->create_layer(w, h, 255, 0, text_gump_layer_z);
			if (g->render_layer < 0) {
				return false;
			}
			gwin->layer_set_ui_kind(g->render_layer, Image_window::UiLayerTextGumps);
		}
		g->layer_bounds = TileRect(0, 0, w, h);
		gwin->layer_set_z(g->render_layer, text_gump_layer_z);

		auto* lbuf = gwin->get_layer_ibuf(g->render_layer);
		if (!lbuf) {
			return false;
		}
		lbuf->clear_clip();
		lbuf->fill8(255);
		prev_target = gwin->push_render_target(lbuf);
		ox          = drawx - r.x;
		oy          = drawy - r.y;
		return true;
	}

	void end_text_layer_paint(Gump* g, int w, int h, Image_buffer8* prev_target) {
		if (!g) {
			return;
		}
		Game_window* gwin = Game_window::get_instance();
		if (!gwin || g->render_layer < 0) {
			return;
		}
		gwin->pop_render_target(prev_target);
		Image_window* iwin = gwin->get_win();
		const float   f    = iwin->get_ui_scale_factor(Image_window::UiLayerTextGumps);
		const float   dw   = static_cast<float>(w) * f;
		const float   dh   = static_cast<float>(h) * f;
		const float   dx   = (static_cast<float>(iwin->get_display_width()) - dw) / 2.0f;
		const float   dy   = (static_cast<float>(iwin->get_display_height()) - dh) / 2.0f;
		gwin->layer_set_dest(g->render_layer, static_cast<int>(dx), static_cast<int>(dy), static_cast<int>(dw), static_cast<int>(dh));
		gwin->layer_set_visible(g->render_layer, true);
		gwin->layer_set_dirty(g->render_layer);
	}
}    // namespace

/*
 *  Create scroll display.
 */

Scroll_gump::Scroll_gump(int fnt, int gump) : Text_gump(gump < 0 ? game->get_shape("gumps/scroll") : gump, fnt) {}

/*
 *  Paint scroll.  Updates curend.
 */

void Scroll_gump::paint() {
	Shape_frame* shape = get_shape();
	if (!shape) {
		return;
	}
	Image_buffer8* prev = nullptr;
	const int      oldx = x;
	const int      oldy = y;
	int            ox   = 0;
	int            oy   = 0;
	int            w    = 0;
	int            h    = 0;
	if (!begin_text_layer_paint(this, oldx, oldy, ox, oy, w, h, prev)) {
		return;
	}

	x              = ox;
	y              = oy;
	paint_shape(x, y);
	curend = paint_page(TileRect(51, 31, 142, 118), curtop);
	x      = oldx;
	y      = oldy;

	end_text_layer_paint(this, w, h, prev);
}
