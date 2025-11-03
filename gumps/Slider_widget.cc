/*
Copyright (C) 2024 The Exult Team

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
#include "Slider_widget.h"

#include "Gump_button.h"
#include "Gump_manager.h"
#include "gamewin.h"
#include "misc_buttons.h"

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include <cmath>

using std::cout;
using std::endl;

// #define SW_FORCE_AUTO_LOG_SLIDERS 1

Slider_widget::Slider_widget(
		Gump_Base* par, int px, int py, std::optional<ShapeID> osidLeft,
		std::optional<ShapeID> osidRight, std::optional<ShapeID> osidDiamond,
		int mival, int mxval, int step, int defval, int width,
		std::shared_ptr<Font> font, int digits_width,
		bool left_align,bool logarithmic)
		: Gump_widget(par, -1, px, py, -1), callback(nullptr),
		  logarithmic(logarithmic), min_val(mival), max_val(mxval),
		  step_val(step), val(defval), prev_dragx(INT32_MIN), font(font), left_align(left_align) {
	diamond = std::make_unique<Diamond>(this, 0, 0, osidDiamond);

	if (osidLeft) {
		auto sid = osidLeft.value_or(ShapeID(-1, -1, SF_OTHER));
		left = std::make_unique < SelfManaged<CallbackButton<Slider_widget>>>(
                this, &Slider_widget::clicked_left_arrow, sid.get_shapenum(), 0,
                0, sid.get_shapefile());
	} else {
		left = std::make_unique< SelfManaged<CallbackButtonBase<Slider_widget, Arrow_Button>>>(
				this, &Slider_widget::clicked_left_arrow, 0, 0,
				Arrow_Button::Left, false);
	}
	if (osidRight) {
		auto sid = osidRight.value_or(ShapeID(-1, -1, SF_OTHER));
		right = std::make_unique < SelfManaged<CallbackButton<Slider_widget>>>(
                this, &Slider_widget::clicked_right_arrow, sid.get_shapenum(),
                0, 0, sid.get_shapefile());
	} else {
		right = std::make_unique<
				SelfManaged<CallbackButtonBase<Slider_widget, Arrow_Button>>>(
				this, &Slider_widget::clicked_right_arrow, 0, 0,
				Arrow_Button::Right, false);
	}
	leftbtnx = left->get_xleft();    //- 1;
	xmin     = leftbtnx + left->get_xright() + diamond->get_xleft() + 1;
	xmax     = xmin + width - diamond->get_width();
	xdist    = xmax - xmin;
	// rightbtnx   = xmax + dshape->get_xright();//+right->get_xleft();
	rightbtnx = leftbtnx + left->get_xright() + width + right->get_xleft() + 1;
	btny      = left->get_height() - 1;

	left->set_pos(leftbtnx, btny);
	right->set_pos(rightbtnx, btny);

	// centre the diamond between button height
	// int buttontop    = btny + left->get_yabove();
	int buttonbottom = btny + left->get_ybelow();
	diamond->set_pos(
			0, buttonbottom - diamond->get_ybelow()
					   - (left->get_height() - diamond->get_height()) / 2);

#ifdef SW_FORCE_AUTO_LOG_SLIDERS
	if (!this->logarithmic) {
		int range = mxval - mival;
		if ((range / xdist) > 2) {
			this->logarithmic = true;
		}
	}
#endif

	callback = dynamic_cast<ICallback*>(par);

#ifdef DEBUG
	cout << "SliderWidget:  " << min_val << " to " << max_val << " by " << step
		 << endl;
#endif

	// Init. to middle value.
	if (defval < min_val) {
		defval = min_val;
	} else if (defval > max_val) {
		defval = max_val;
	}
	max_digits_width = digits_width;
	if (font) {
		char buf[20];
		snprintf(buf, sizeof(buf), "%d", max_val);
		max_digits_width
				= std::max(max_digits_width, font->get_text_width(buf) + 2);

		snprintf(buf, sizeof(buf), "%d", min_val);
		max_digits_width
				= std::max(max_digits_width, font->get_text_width(buf) + 2);
	}
	if (left_align) {
		max_digits_width += 2;
	}
	set_val(defval, true);
}

/*
 *  Set slider value.
 */

void Slider_widget::set_val(int newval, bool recalcdiamond) {
	val = newval;
	int diamondx;
	if (recalcdiamond) {
		if (max_val - min_val == 0) {
			val      = 0;
			diamondx = xmin;
		} else {
			diamondx = xmin
					   + lineartolog(
							   (val - min_val) * xdist / (max_val - min_val),
							   xdist);
		}
		diamond->set_pos(diamondx, diamond->get_y());
	}
	if (callback) {
		callback->OnSliderValueChanged(this, val);
	}
}

/*
 *  An arrow on the slider was clicked.
 */

void Slider_widget::clicked_left_arrow() {
	move_diamond(-step_val);
}

void Slider_widget::clicked_right_arrow() {
	move_diamond(step_val);
}

void Slider_widget::move_diamond(int dir) {
	int newval = val;
	newval += dir;
	if (newval < min_val) {
		newval = min_val;
	}
	if (newval > max_val) {
		newval = max_val;
	}

	set_val(newval);
	gwin->add_dirty(get_rect());
}

TileRect Slider_widget::get_rect() const {
	// Widget has no background shape so get rect needs to calculate the
	// union of all the parts of the widget

	TileRect leftrect  = left->get_rect();
	TileRect rightrect = right->get_rect();

	if (font) {
		rightrect.w += max_digits_width;
	}
	return leftrect.add(rightrect).add(diamond->get_rect());
}

/*
 *  Paint on screen.
 */

void Slider_widget::paint() {
	diamond->paint();

	left->paint();
	right->paint();

	if (font) {
		auto rect = right->get_rect();
		gumpman->paint_num(
				val, rect.x + rect.w + (left_align?2:max_digits_width),
				rect.y + (rect.h - font->get_text_height() + 1) / 2, font,left_align);
	}
}

bool Slider_widget::run() {
	Gump_button* pushed=nullptr;
	if (left->is_pushed()) {
		pushed = left.get();
	} else if (right->is_pushed()) {
		pushed = right.get();
	}

	if (pushed) {
	
		if (next_auto_push_time == 0) {
			// First time pushed, set 0.5 second delay for auto repeat
			next_auto_push_time = SDL_GetTicks() + 500;
		}
		else if (SDL_GetTicks() >= next_auto_push_time) {
			next_auto_push_time
					+= 50;    // 0.05 second delay for next repeat
			// Simulate click
			pushed->activate(MouseButton::Left);

			return true;
		}
 
	} 
	else {
		// No pushed buttons reset the timer
		next_auto_push_time = 0;
	}

	return false;
}

void Slider_widget::Diamond::paint() {
	if (get_framenum() != -1) {
		return Gump_widget::paint();
	}

	auto ibuf8 = gwin->get_win()->get_ib8();
	int  x = 0, y = 0;

	local_to_screen(x, y);

	struct {
		uint8 col;
		int   startx;
		int   starty;
		int   endx;
		int   endy;

		bool ispoint() const {
			return startx == endx && starty == endy;
		}
	} lines[] = {
			{24, 0, 3, 3, 6},
            {18, 3, 0, 3, 2},
            {18, 4, 3, 6, 3},
			{22, 1, 3, 2, 3},
            {22, 3, 4, 3, 5},
            {20, 4, 5, 5, 4},
			{23, 2, 4, 2, 4},
            {20, 2, 1, 1, 2},
            {20, 2, 2, 2, 2},
			{20, 4, 4, 4, 4},
            {15, 4, 2, 4, 2},
            {16, 4, 1, 5, 2},
			{19, 3, 3, 3, 3}
    };

	for (const auto& line : lines) {
		if (line.ispoint()) {
			ibuf8->put_pixel8(line.col, x + line.startx, y + line.starty);
		} else {
			ibuf8->draw_line8(
					line.col, x + line.startx, y + line.starty, x + line.endx,
					y + line.endy);
		}
	}
}

bool Slider_widget::Diamond::on_widget(int x, int y) const {
	if (get_framenum() != -1) {
		return Gump_widget::on_widget(x, y);
	}
	screen_to_local(x, y);

	if (y < 0 || y > 6) {
		return false;
	}
	const int o = y < 4 ? 3 - y : y - 3;
	if (x < o || x >= 7 - o) {
		return false;
	}
	return true;
}


/*
 *  Handle mouse-down events.
 */

bool Slider_widget::mouse_down(
		int mx, int my, MouseButton button    // Position in window.
) {
	if (button != MouseButton::Left) {
		return Gump_widget::mouse_down(mx, my, button);
	}

	// try buttons first.
	Gump_widget* first = left->Input_first();
	if (!first) {
		first = right->Input_first();
	}
	if (first) {
		return first->mouse_down(mx, my, button);
	}
	if (left->mouse_down(mx, my, button) || right->mouse_down(mx, my, button)) {
		return true;
	}

	// See if on diamond.
	if (diamond->on_widget(mx, my)) {
		// Start to drag it.
		prev_dragx = diamond->get_x();
	} else {
		int lx = mx, ly = my;
		screen_to_local(lx, ly);
		if (ly < diamond->get_y()
			|| ly > diamond->get_y() + diamond->get_height()) {
			return Gump_widget::mouse_down(mx, my, button);
		} else if (
				lx < xmin - diamond->get_width()
				|| lx >= xmax + diamond->get_width()) {
			return Gump_widget::mouse_down(mx, my, button);
		} else if (lx < xmin) {
			lx = xmin;
		} else if (lx > xmax) {
			lx = xmax;
		}
		int delta = logtolinear(lx - xmin, xdist) * (max_val - min_val) / xdist;
		// Round down to nearest step.
		delta -= delta % step_val;
		set_val(min_val + delta, true);

		gwin->add_dirty(get_rect());
	}

	return true;
}

/*
 *  Handle mouse-up events.
 */

bool Slider_widget::mouse_up(
		int mx, int my, MouseButton button    // Position in window.
) {
	// try input first buttons
	Gump_widget* first = left->Input_first();
	if (!first) {
		first = right->Input_first();
	}
	if (first) {
		return first->mouse_up(mx, my, button);
	}
	if (button != MouseButton::Left || !is_dragging()) {
		// pass to buttons
		if (left->mouse_down(mx, my, button)
			|| right->mouse_down(mx, my, button)) {
			return true;
		}

		return Gump_widget::mouse_up(mx, my, button);
	}

	prev_dragx = INT32_MIN;
	set_val(val);    // Set diamond in correct pos.
	gwin->add_dirty(get_rect());
	return true;
}

/*
 *  Mouse was dragged with left button down.
 */
#ifdef EXTRA_DEBUG
#	define DEBUG_VAL(v) std::cout << #v ": " << (v) << std::endl
#else
#	define DEBUG_VAL(v) \
		do {             \
		} while (0)
#endif

bool Slider_widget::mouse_drag(
		int mx, int my    // Where mouse is.
) {
	ignore_unused_variable_warning(mx, my);

	// try input first buttons
	Gump_widget* first = left->Input_first();
	if (!first) {
		first = right->Input_first();
	}
	if (first) {
		return first->mouse_drag(mx, my);
	}

	if (!is_dragging()) {
		// pass to buttons
		if (left->mouse_drag(mx, my) || right->mouse_drag(mx, my)) {
			return true;
		}
		return Gump_widget::mouse_drag(mx, my);
	}

	// clamp the mouse position to the slidable region
	int lx = mx, ly = my;
	screen_to_local(lx, ly);
	DEBUG_VAL(lx);
	if (lx > rightbtnx) {
		lx = rightbtnx;
	} else if (lx < xmin) {
		lx = xmin;
	}
	int diamondx = diamond->get_x();
	DEBUG_VAL((diamondx));
	DEBUG_VAL(lx);
	DEBUG_VAL(prev_dragx);
	DEBUG_VAL(lx - prev_dragx);
	diamondx += lx - prev_dragx;
	prev_dragx = lx;
	if (diamondx < xmin) {    // Stop at ends.
		diamondx = xmin;
	} else if (diamondx > xmax) {
		diamondx = xmax;
	}
	DEBUG_VAL(diamondx);
	DEBUG_VAL(xdist);
	DEBUG_VAL(xmax);
	DEBUG_VAL(xmin);
	int delta
			= logtolinear(diamondx - xmin, xdist) * (max_val - min_val) / xdist;
	DEBUG_VAL(delta);
	DEBUG_VAL(diamondx);
	DEBUG_VAL(max_val);
	DEBUG_VAL(min_val);

	// Round down to nearest step.
	delta -= delta % step_val;
	DEBUG_VAL(delta);
	DEBUG_VAL(step_val);
	set_val(min_val + delta, false);
	diamond->set_pos(diamondx, diamond->get_y());
	gwin->add_dirty(get_rect());
	return true;
}

/*
 *  Handle ASCII character typed.
 */

bool Slider_widget::key_down(SDL_Keycode chr, SDL_Keycode unicode) {
	ignore_unused_variable_warning(unicode);
	switch (chr) {
	case SDLK_LEFT:
		clicked_left_arrow();
		break;
	case SDLK_RIGHT:
		clicked_right_arrow();
		break;
	}
	return true;
}

bool Slider_widget::mousewheel_up(int mx, int my) {
	ignore_unused_variable_warning(mx, my);
	const SDL_Keymod mod = SDL_GetModState();
	if (mod & SDL_KMOD_ALT) {
		move_diamond(-10 * step_val);
	} else {
		move_diamond(-step_val);
	}
	return true;
}

bool Slider_widget::mousewheel_down(int mx, int my) {
	ignore_unused_variable_warning(mx, my);
	const SDL_Keymod mod = SDL_GetModState();
	if (mod & SDL_KMOD_ALT) {
		move_diamond(10 * step_val);
	} else {
		move_diamond(step_val);
	}
	return true;
}
#ifdef SW_INVERT_LOGS
#	define logtolinear lineartolog
#endif
int Slider_widget::logtolinear(int logvalue, int base) {
	if (!logarithmic) {
		return logvalue;
	}
	double b  = base + 1;
	double lv = logvalue + 1;

	double scaled = lv / b;
	double res    = std::pow(b, scaled);
	res -= 1;
	// int check = lineartolog(res, base);
	return static_cast<int>(res);
}
#ifdef SW_INVERT_LOGS
#	undef logtolinear
#	define lineartolog logtolinear
#endif
int Slider_widget::lineartolog(int linearvalue, int base) {
	if (!logarithmic) {
		return linearvalue;
	}
	double b  = base + 1;
	double lv = linearvalue + 1;

	double res = log(lv) / log(b);
	res        = res * b - 1;

	return static_cast<int>(res);
}

Gump_widget* Slider_widget::Input_first() {
	Gump_widget* first = left->Input_first();
	if (!first) {
		first = right->Input_first();
	}
	if (first) {
		return first;
	}
	if (is_dragging()) {
		return this;
	}
	return Gump_widget::Input_first();
}
