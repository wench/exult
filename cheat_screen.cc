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

#include "cheat_screen.h"

#include "Configuration.h"
#include "Gump_manager.h"
#include "actors.h"
#include "cheat.h"
#include "cheat_screen_strings.h"
#include "chunks.h"
#include "exult.h"
#include "files/U7file.h"    // IWYU pragma: keep
#include "files/databuf.h"
#include "files/msgfile.h"
#include "files/utils.h"
#include "fnames.h"
#include "font.h"
#include "game.h"
#include "gameclk.h"
#include "gamemap.h"
#include "gamewin.h"
#include "gump_utils.h"
#include "ignore_unused_variable_warning.h"
#include "miscinf.h"
#include "party.h"
#include "schedule.h"
#include "touchui.h"
#include "ucmachine.h"
#include "version.h"
#include "vgafile.h"

#include <algorithm>
#include <cstring>

static const SDL_MouseID EXSDL_TOUCH_MOUSEID  = SDL_TOUCH_MOUSEID;
CheatScreen*             CheatScreen::cscreen = nullptr;

int CheatScreen::Get_highest_map() {
	if (highest_map != INT_MIN) {
		return highest_map;
	}
	int n = 0;
	int next;
	while ((next = Find_next_map(n + 1, 10)) != -1) {
		n = next;
	}
	highest_map = n;
	return n;
}

void CheatScreen::show_screen() {
	cscreen                              = this;
	ibuf                                 = gwin->get_win()->get_ib8();
	const str_int_pair& pal_tuple_static = game->get_resource("palettes/0");
	const str_int_pair& pal_tuple_patch  = game->get_resource("palettes/patch/0");
	pal.load(pal_tuple_static.str, pal_tuple_patch.str, pal_tuple_static.num);

	// init fontcolor transform table, default does nothing
	for (size_t i = 0; i < std::size(fontcolor.colors); i++) {
		fontcolor.colors[i]  = i;
		fontcolor2.colors[i] = i;
	}

	// Try to use SMALL_BLACK_FONT and Remap black to white in fontcolor
	font                 = fontManager.get_font("SMALL_BLACK_FONT");
	fontcolor.colors[0]  = pal.find_color(256, 256, 256);
	fontcolor2.colors[0] = pal.find_color(256, 0, 0);

	if (!font) {
		// reset fontcolor black so it maps to black
		fontcolor.colors[0] = 0;
		// Try to get the Font form Blackgate first because it looks better than
		// the SI one
		font = std::make_shared<Font>();
		if (font->load(U7MAINSHP_FLX, 9, 1) != 0) {
			font.reset();
		}
	}

	// Get the font for this game if don't already have it
	if (!font) {
		font = fontManager.get_font("MENU_FONT");
	}
	clock = gwin->get_clock();
	maxx  = gwin->get_width();
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	maxy = 200;
#else
	maxy = gwin->get_height();
#endif
	centerx            = maxx / 2;
	centery            = maxy / 2;
	SDL_Window* window = gwin->get_win()->get_screen_window();
	if (touchui != nullptr) {
		touchui->hideGameControls();
		// Set the text input area to be just to the right of the prompt, and 10 pixels high.
		const int prompt_y = 81;
		const int input_x  = 64 + 15;
		TouchUI::setTextInputArea(window, input_x, prompt_y, maxx, prompt_y + 10);
		TouchUI::startTextInput(window);
	}

	// Pause the game
	gwin->get_tqueue()->pause(SDL_GetTicks());

	pal.apply();

	const int remaps[] = {
			5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 - 1,
	};

	pal.Generate_remap_xformtable(highlighttable.colors, remaps);

	const int hoverremaps[] = {
			1, 1, 1, 1, 1, 6, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 - 1,
	};

	pal.Generate_remap_xformtable(hovertable.colors, hoverremaps);

	// Make sure the font colour is properly remapped
	if (fontcolor[0]) {
		highlighttable.colors[fontcolor[0]] = highlighttable.colors[8];
		hovertable.colors[fontcolor[0]]     = hovertable.colors[8];
	}

	ClearState clear(state);

	Mouse::mouse()->hide();
	Mouse::Mouse_shapes saveshape = Mouse::mouse()->get_shape();
	Mouse::mouse()->set_shape(Mouse::hand);

	buttons_down.clear();
	// Start the loop
	NormalLoop();

	Mouse::mouse()->set_shape(saveshape);
	Mouse::mouse()->hide();
	gwin->paint();
	Mouse::mouse()->show();

	// Resume the game clock
	gwin->get_tqueue()->resume(SDL_GetTicks());

	// Reset the palette
	clock->reset_palette();
	if (touchui != nullptr) {
		Gump_manager* gumpman = gwin->get_gump_man();
		if (!gumpman->gump_mode()) {
			touchui->showGameControls();
		}
		if (SDL_TextInputActive(window)) {
			SDL_StopTextInput(window);
		}
	}
}

const char* CheatScreen::getKeyName(SDL_Keycode keycode) {
	switch (keycode) {
	case SDLK_UNKNOWN:
		return "";

	case SDLK_ESCAPE:
		return Strings::ESC;

	case SDLK_TAB:
		return Strings::TAB;

	case SDLK_RETURN:
		return Strings::RET;

		// Using Macro for Function Keys becasue the keycodes for them aren't cobsecutive
#define F_KEY_CASE(n) \
	case SDLK_F##n:   \
		return "F" #n

		F_KEY_CASE(1);
		F_KEY_CASE(2);
		F_KEY_CASE(3);
		F_KEY_CASE(4);
		F_KEY_CASE(5);
		F_KEY_CASE(6);
		F_KEY_CASE(7);
		F_KEY_CASE(8);
		F_KEY_CASE(9);
		F_KEY_CASE(10);
		F_KEY_CASE(11);
		F_KEY_CASE(12);

		F_KEY_CASE(13);
		F_KEY_CASE(14);
		F_KEY_CASE(15);
		F_KEY_CASE(16);
		F_KEY_CASE(17);
		F_KEY_CASE(18);
		F_KEY_CASE(19);
		F_KEY_CASE(20);
		F_KEY_CASE(21);
		F_KEY_CASE(22);
		F_KEY_CASE(23);
		F_KEY_CASE(24);

#undef F_KEY_CASE
	default:
		// Get rid of the scancode bit
		keycode &= ~SDLK_SCANCODE_MASK;

		if (keycode < 0x1000) {
			// Use a static array for each key name
			static char names[0x1000][4] = {};

			// only if not already set
			if (!names[keycode][0]) {
				// Keycode is an ASCII Character
				if (keycode < 128 && std::isprint(keycode)) {
					names[keycode][0] = std::toupper(keycode);
					names[keycode][1] = 0;
				}
				// not ascii so output 3 digit hexcode
				else {
					for (size_t i = 0; i < 4; i++) {
						int  d                = (keycode >> i * 4) & 0xf;
						char c                = d + (d >= 0xA ? 'A' : '0');
						names[keycode][3 - i] = c;
					}
					names[keycode][3] = 0;
				}
			}
			return names[keycode];
		}
	}
	return Strings::UNKNOWNKEYNAME;
}

//
// Input Handlers
//

// Parse the input , returns false on error
void CheatScreen::InputHandler::Parse() {
	if (!input[0]) {
		if (!empty_allowed) {
			throw MenuCommandException{Strings::INPUT_REQUIRED};
		} else {
			was_empty = true;
		}
	}
}

void CheatScreen::InputHandler::ArrangeHotspots(int x, int y, unsigned lines) {
	// If only one and it would fit put it on the buttom line
	if (hotspots.size() == 1 && x <= (320 - hotspots.back().getWidth())) {
		auto& hs = hotspots.back();
		hs.x     = x;
		hs.y     = y + 9 * (lines - 1);
	}
	// One per line right align to edge of screen
	else if (lines >= hotspots.size()) {
		for (auto& hs : hotspots) {
			hs.x = 320 - 8 - hs.getWidth();
			hs.y = y;
			y += 9;
		}
	} else {    // put them all on 1 line
		for (auto& hs : hotspots) {
			hs.x = x;
			hs.y = y;
			x += hs.getWidth() + 8;
		}
	}
}

int CheatScreen::InputHandler::PaintPrompt(int x, int y, SDL_Keycode lastkey) {
	auto  font      = cscreen->font;
	auto  ibuf      = cscreen->ibuf;
	auto& fontcolor = cscreen->fontcolor;
	int   offset    = font->paint_text_fixedwidth(ibuf, Strings::SELECT, x, y, 8, fontcolor.colors);
	if (curlen) {
		offset += font->paint_text_fixedwidth(ibuf, input, curlen, x + offset, y, 8, fontcolor.colors);
	} else {
		offset += cscreen->PaintKeyName(x + offset, y, lastkey);
	}

	return offset;
}

//
//
//

inline void CheatScreen::InputHandlers::KeyOnly::Parse() {
	// Parse makes sure the pressed key is in hotspots
	if (!hotspots.empty()) {
		auto it = std::find_if(hotspots.begin(), hotspots.end(), [this](const auto& hotspot) {
			return hotspot.IsKeycode(key_sym);
		});
		if (it == hotspots.end()) {
			throw MenuCommandException{Strings::INVALID_COMMAND, false};
		}
	}
	if (events.Parsed) {
		events.Parsed(this);
	}
}

//
// String Input Handler
//

bool CheatScreen::InputHandlers::String::OnInput(SDL_Keycode key_sym) {
	// Backspace last character
	if (key_sym == SDLK_BACKSPACE) {
		if (curlen) {
			input[--curlen] = 0;
		}
	}
	// Append the character
	else if (key_sym < 256 && std::isprint(key_sym)) {
		if (curlen < (std::size(input) - 1)) {
			input[curlen++] = key_sym;
			input[curlen]   = 0;
		}
	} else if (key_sym == SDLK_RETURN) {
		return true;
	}

	return false;
}

void CheatScreen::InputHandlers::String::Parse() {
	if (!curlen && !empty_allowed) {
		throw MenuCommandException{invalidmsg, false};
	}
}

// Integer Input Handler

inline void CheatScreen::InputHandlers::Integer::GetPromptMessage(char* buf, size_t buf_size) {
	bool hasmin = val_min != INT_MIN;
	bool hasmax = val_max != INT_MAX;
	// treat as hex if hex only flag is set or the input has a hex prefix
	bool hex = hexonly || !strncmp(input, "0x", 2) || !strncmp(input, "-0x", 3);

	if (hasmin && hasmax) {
		snprintf(buf, buf_size, (hex ? "%s  0x%lx-0x%lx " : "%s  %ld-%ld "), promptmsg.c_str(), val_min, val_max);
	} else if (hasmin || hasmax) {
		snprintf(
				buf, buf_size, (hex ? "%s  %s 0x%lx " : "%s  %s %ld "), promptmsg.c_str(),
				hasmin ? "Min:" : "Max:", hasmin ? val_min : val_max);

	} else {
		snprintf(buf, buf_size, "%s ", promptmsg.c_str());
	}
}

bool CheatScreen::InputHandlers::Integer::OnInput(SDL_Keycode key_sym) {
	// treat as hex if hex only flag is set or the input has a hex prefix
	bool hex = hexonly || !strncmp(input, "0x", 2) || !strncmp(input, "-0x", 3);

	// Increment/Decrement with arrow keys
	if (key_sym == SDLK_LEFT || key_sym == SDLK_RIGHT) {
		char* end     = nullptr;
		input[curlen] = 0;
		long val      = std::strtol(input, &end, hex ? 16 : 10);

		if (end == input + curlen) {
			// Decrement
			if (key_sym == SDLK_LEFT && val > val_min) {
				val--;
			}
			// Increment
			else if (key_sym == SDLK_RIGHT && val < val_max) {
				val++;
			} else {
				val = val_min;
			}
			// Format value back to a string
			// Separate handling for positive and negative so negaive hex numbers have a minus sign
			int len = 0;
			if (val < 0) {
				len = snprintf(input, std::size(input), hex ? "-%lx" : "-%ld", -val);
			} else {
				len = snprintf(input, std::size(input), hex ? "%lx" : "%ld", val);
			}
			if (len < 0) {
				curlen   = 0;
				input[0] = 0;
				throw MenuCommandException{"Unexepected Error calling snprintf", false};
			}
			if (size_t(len) >= std::size(input)) {
				input[curlen = std::size(input) - 1] = 0;
			} else {
				curlen = len;
			}
		}
	}
	// Add x for hex prefix but only if input is currently "0" or "-0" to form "0x or "-0x
	else if ((key_sym == SDLK_X) && (!strcmp(input, "0") || !strcmp(input, "-0"))) {
		input[curlen++] = 'x';
		input[curlen]   = 0;
	}
	// Backspace last character
	else if (key_sym == SDLK_BACKSPACE) {
		if (curlen) {
			input[--curlen] = 0;
		}

	}
	// Append pressed digit, or minus sign if start of input
	else if (key_sym < 256 && (std::isdigit(key_sym) || (hex && std::isxdigit(key_sym)) || (key_sym == SDLK_MINUS && !input[0]))) {
		if (curlen < (std::size(input) - 1)) {
			// SDLK_MINUS should be '-' but being  careful I'm not assuming that
			input[curlen++] = key_sym == SDLK_MINUS ? '-' : char(std::tolower(key_sym));
			input[curlen]   = 0;
		}
	} else if (key_sym == SDLK_RETURN) {
		return true;
	}

	return false;
}

CheatScreen::InputHandlers::Integer::Integer(
		bool empty_allowed, long min, long max, bool hex, std::string&& promptmsg, std::string&& invalidmsg)
		: InputHandler(empty_allowed, std::move(promptmsg)), hexonly(hex), val_min(std::min(max, min)), val_max(std::max(max, min)),
		  invalidmsg(std::move(invalidmsg)) {}

CheatScreen::InputHandlers::Integer::Integer(bool empty_allowed, long min, long max, bool hex, std::string&& promptmsg)
		: Integer(empty_allowed, min, max, hex, std::move(promptmsg), Strings::INVALID_VALUE) {}

CheatScreen::InputHandlers::Integer::Integer(bool empty_allowed, long min, long max, bool hex)
		: Integer(empty_allowed, min, max, hex, Strings::ENTER_VALUE) {}

void CheatScreen::InputHandlers::Integer::Parse() {
	InputHandler::Parse();

	if (empty_allowed && was_empty) {
		// If empty set to minimum
		value = val_min;
		return;
	}

	// treat as hex if hex only flag is set or the input has a hex prefix
	bool hex = hexonly || !strncmp(input, "0x", 2) || !strncmp(input, "-0x", 3);

	if (input[0]) {
		char* input_end = nullptr;

		const long val = std::strtol(input, &input_end, hex ? 16 : 10);
		if (input_end != input && val >= val_min && val <= val_max) {
			value = val;
		} else {
			throw MenuCommandException{Strings::INVALID_VALUE};
		}
	}
}

// Game Object input Handler

CheatScreen::InputHandlers::GameObject::GameObject(bool empty_allowed, std::string&& promptmsg, std::string&& invalidmsg)
		: Integer(empty_allowed, 0, gwin->get_num_npcs(), false, std::move(promptmsg), std::move(invalidmsg)) {
	const char* label = Strings::Pick_Object_from_World;
	hotspots.emplace_back(0, 0, label[0], label + 1);
}

CheatScreen::InputHandlers::GameObject::GameObject(bool empty_allowed, std::string&& promptmsg)
		: GameObject(empty_allowed, std::move(promptmsg), Strings::INVALID_OBJECT) {}

CheatScreen::InputHandlers::GameObject::GameObject(bool empty_allowed) : GameObject(empty_allowed, Strings::ENTERNPCNUMBER) {}

bool CheatScreen::InputHandlers::GameObject::OnInput(SDL_Keycode key_sym) {
	// Enter Pick Mode
	if (key_sym == SDLK_P) {
		cscreen->WaitButtonsUp(true);
		gwin->set_all_dirty();
		gwin->paint();
		gwin->show(true);

		int x, y;

		if (Get_click(x, y, Mouse::greenselect, nullptr, true, nullptr, true)) {
			object = gwin->find_object(x, y);
		} else {
			throw MenuCommandException{Strings::CANCELLED, false};
		}
		if (!object) {
			throw MenuCommandException{invalidmsg, false};
		}
		return true;
	}
	return Integer::OnInput(key_sym);
}

void CheatScreen::InputHandlers::GameObject::Parse() {
	if (!object) {
		Integer::Parse();

		// Empty input means use grabbed NPC
		if (empty_allowed && was_empty) {
			object = cscreen->grabbed;
		} else {
			object = gwin->get_npc(value);
		}
	}

	if (!object) {
		throw MenuCommandException{invalidmsg};
	}
}

//
// DISPLAYS
//

//
// Shared
//

void CheatScreen::SharedPrompt() {
	char buf[64];

#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int prompt    = 81;
	const int promptmes = 90;
	const int offsetx   = 15;
#else
	const int prompt    = maxy - 18;
	const int promptmes = maxy - 9;
	const int offsetx   = 0;
#endif
	font->paint_text_fixedwidth(ibuf, "Select->", offsetx, prompt, 8, fontcolor.colors);

	// Special handling for arrows when not doing text input
	const char* input = state.input;
	if (state.GetMode() < CP_ChooseNPC && !input[1] && (*input == '<' || *input == '>' || *input == '^' || *input == 'V')) {
		PaintArrow(64 + offsetx, prompt, *input);
		input = " ";
	}
	if (input && std::strlen(input)) {
		font->paint_text_fixedwidth(ibuf, input, 64 + offsetx, prompt, 8, fontcolor.colors);
		font->paint_text_fixedwidth(ibuf, "_", 64 + offsetx + int(std::strlen(input)) * 8, prompt, 8, fontcolor.colors);
	} else {
		font->paint_text_fixedwidth(ibuf, "_", 64 + offsetx, prompt, 8, fontcolor.colors);
	}
	// Clear the input
	if (state.GetMode() < CP_ChooseNPC) {
		state.input[0] = 0;
	}
	// ...and Prompt Message
	switch (state.GetMode()) {
	default:

	case CP_Command:
		font->paint_text_fixedwidth(ibuf, "Enter Command.", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_HitKey:
		font->paint_text_fixedwidth(ibuf, "Hit a key.", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_NotAvail:
		font->paint_text_fixedwidth(ibuf, "Not yet available. Hit a key.", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_InvalidNPC:
		font->paint_text_fixedwidth(ibuf, "Invalid NPC. Hit a key", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_InvalidCom:
		font->paint_text_fixedwidth(ibuf, "Invalid Command. Hit a key.", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_Canceled:
		font->paint_text_fixedwidth(ibuf, "Canceled. Hit a key.", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_ClockSet:
		font->paint_text_fixedwidth(ibuf, "Clock Set. Hit a key.", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_InvalidTime:
		font->paint_text_fixedwidth(ibuf, "Invalid Time. Hit a key.", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_InvalidShape:
		font->paint_text_fixedwidth(ibuf, "Invalid Shape. Hit a key.", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_InvalidValue:
		font->paint_text_fixedwidth(ibuf, "Invalid Value. Hit a key.", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_Created:
		font->paint_text_fixedwidth(ibuf, "Item Created. Hit a key.", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_ShapeSet:
		font->paint_text_fixedwidth(ibuf, "Shape Set. Hit a key.", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_ValueSet:
		font->paint_text_fixedwidth(ibuf, "Clock Set. Hit a key.", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_NameSet:
		font->paint_text_fixedwidth(ibuf, "Name Changed. Hit a key.", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_WrongShapeFile:
		font->paint_text_fixedwidth(ibuf, "Wrong shape file. Must be SHAPES.VGA.", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_ChooseNPC:
		font->paint_text_fixedwidth(ibuf, "Which NPC? (ESC to cancel.)", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_EnterValue:
		font->paint_text_fixedwidth(ibuf, "Enter Value. (ESC to cancel.)", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_CustomValue:
		if (state.custom_prompt) {
			font->paint_text_fixedwidth(ibuf, state.custom_prompt, offsetx, promptmes, 8, fontcolor.colors);
		}
		break;

	case CP_EnterValueNoCancel:
		font->paint_text_fixedwidth(ibuf, "Enter Value.", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_Minute:
		font->paint_text_fixedwidth(ibuf, "Enter Minute. (ESC to cancel.)", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_Hour:
		font->paint_text_fixedwidth(ibuf, "Enter Hour. (ESC to cancel.)", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_Day:
		font->paint_text_fixedwidth(ibuf, "Enter Day. (ESC to cancel.)", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_Shape:
		snprintf(
				buf, std::size(buf), "Enter Shape Max %i (B=Browse or ESC=Cancel)",
				Shape_manager::get_instance()->get_shapes().get_num_shapes() - 1);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_Activity:
		font->paint_text_fixedwidth(ibuf, "Enter Activity 0-31. (ESC to cancel.)", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_XCoord:
		snprintf(buf, sizeof(buf), "Enter X Coord. Max %i (ESC to cancel)", c_num_tiles);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_YCoord:
		snprintf(buf, sizeof(buf), "Enter Y Coord. Max %i (ESC to cancel)", c_num_tiles);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_Lift:
		font->paint_text_fixedwidth(ibuf, "Enter Lift. (ESC to cancel)", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_GFlagNum: {
		snprintf(buf, sizeof(buf), "Enter Global Flag 0-%zu. (ESC to cancel)", Usecode_machine::last_gflag);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8, fontcolor.colors);
		break;
	}

	case CP_NFlagNum:
		font->paint_text_fixedwidth(ibuf, "Enter NPC Flag 0-63. (ESC to cancel)", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_TempNum:
		font->paint_text_fixedwidth(ibuf, "Enter Temperature 0-63. (ESC to cancel)", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_NLatitude:
		font->paint_text_fixedwidth(ibuf, "Enter Latitude. Max 113 (ESC to cancel)", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_SLatitude:
		font->paint_text_fixedwidth(ibuf, "Enter Latitude. Max 193 (ESC to cancel)", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_WLongitude:
		font->paint_text_fixedwidth(ibuf, "Enter Longitude. Max 93 (ESC to cancel)", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_ELongitude:
		font->paint_text_fixedwidth(ibuf, "Enter Longitude. Max 213 (ESC to cancel)", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_Name:
		font->paint_text_fixedwidth(ibuf, "Enter a new Name...", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_NorthSouth:
		font->paint_text_fixedwidth(ibuf, "Latitude [N]orth or [S]outh?", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_WestEast:
		font->paint_text_fixedwidth(ibuf, "Longitude [W]est or [E]ast?", offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_HexXCoord:
		snprintf(buf, sizeof(buf), "Enter X Coord. Max %04x (ESC to cancel)", c_num_tiles);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8, fontcolor.colors);
		break;

	case CP_HexYCoord:
		snprintf(buf, sizeof(buf), "Enter Y Coord. Max %04x (ESC to cancel)", c_num_tiles);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8, fontcolor.colors);
		break;
	}
}

static void resizeline(float& axis1, float delta1, float& axis2) {
	if (axis1) {
		float slope = axis2 / axis1;
		axis1 += delta1;
		axis2 = axis1 * slope;
	}
}

SDL_Keycode CheatScreen::GetKey(const std::vector<Hotspot*>& hotspots, SDL_Keycode& unicode) {
	SDL_Event     event;
	SDL_Renderer* renderer = SDL_GetRenderer(gwin->get_win()->get_screen_window());
	SDL_Window*   window   = gwin->get_win()->get_screen_window();
	// Do repaint after 100 ms to allow for time dependant effects. 10 FPS seems
	// more that adequate for Cheat Screen If anyone needs to do smooth animaion
	// the can change this
	auto repainttime = SDL_GetTicks() + 100;
	Mouse::mouse()->hide();    // Turn off mouse.
	std::memset(&event, 0, sizeof(event));

	while (SDL_GetTicks() < repainttime) {
		Delay();
		if (state.highlighttime && state.highlighttime < SDL_GetTicks()) {
			state.highlight     = 0;
			state.highlighttime = 0;
		}

		// How far finger needs to move for swipe to be interpreted as an a
		// cursor key input)
		bool do_swipe = std::abs(state.swipe_dx) > swipe_threshold || std::abs(state.swipe_dy) > swipe_threshold;

		if (!do_swipe && state.last_swipe && ((state.last_swipe + 200) < SDL_GetTicks())) {
			// zero out swipe state if it's been longer that 200ms since last
			// event recieved and it's shorter than the threshold
			state.last_swipe = 0;
			state.swipe_dx   = 0;
			state.swipe_dy   = 0;
		}
		Mouse::mouse_update = false;
		while (do_swipe || SDL_PollEvent(&event)) {
			int         gx, gy;
			SDL_Keycode simulate_key = SDLK_UNKNOWN;

			//
			if (do_swipe) {
				float ax = 0, ay = 0;
				ay = std::abs(state.swipe_dy);
				ax = std::abs(state.swipe_dx);
				CERR("Doing swipe ay: " << ay << " ax:" << ax);
				// More vertical motion or More Horizontal
				// perfectly diagonal motion will be treated as horizontal
				if (ay > ax) {
					// Swipe up the screen
					if (state.swipe_dy < -swipe_threshold) {
						simulate_key = SDLK_UP;
						resizeline(state.swipe_dy, +swipe_threshold, state.swipe_dx);
						// Swipe Down the screen
					} else if (state.swipe_dy > swipe_threshold) {
						simulate_key = SDLK_DOWN;
						resizeline(state.swipe_dy, -swipe_threshold, state.swipe_dx);
					}
				} else {
					// swipe right to left
					if (state.swipe_dx < -swipe_threshold) {
						simulate_key = SDLK_LEFT;
						resizeline(state.swipe_dx, +swipe_threshold, state.swipe_dy);
						// Swipe left to right
					} else if (state.swipe_dx > swipe_threshold) {
						simulate_key = SDLK_RIGHT;
						resizeline(state.swipe_dx, -swipe_threshold, state.swipe_dy);
					}
				}
			} else {
				CERR("event.type= " << event.type);
				switch (event.type) {
				case SDL_EVENT_QUIT: {
					CERR("SDL_EVENT_QUIT");
					ImageBufferPaintable screenshot;
					if (gwin->get_gump_man()->okay_to_quit(&screenshot)) {
						throw quit_exception();
					}
				} break;
				case SDL_EVENT_FINGER_DOWN: {
					CERR("SDL_EVENT_FINGER_DOWN");
					buttons_down.insert(button_down_finger);
					if ((!Mouse::use_touch_input) && (event.tfinger.fingerID != 0)) {
						Mouse::use_touch_input = true;
					}
					break;
				}
				case SDL_EVENT_FINGER_UP: {
					CERR("SDL_EVENT_FINGER_UP");
					// Get iterator for first instance of button_down_finger and
					// erase it from the collection
					auto it = buttons_down.find(button_down_finger);
					if (it != buttons_down.end()) {
						buttons_down.erase(it);
					}

				} break;
					// Finger swiping converts to cursor keys
				case SDL_EVENT_FINGER_MOTION: {
					gwin->get_win()->screen_to_game(event.button.x, event.button.y, gwin->get_fastmouse(), gx, gy);

					static int numFingers = 0;
					SDL_GetTouchFingers(event.tfinger.touchID, &numFingers);
					CERR("numFingers " << numFingers);

					// Will allow single finger swipes
					if (numFingers > 0) {
						// Hide on screen keyboard if we are swiping
						if (SDL_TextInputActive(window)) {
							SDL_StopTextInput(window);
						}
						// Accuulate the deltas onto
						// thevector
						state.swipe_dx += event.tfinger.dx;
						state.swipe_dy += event.tfinger.dy;
						// set last swipe value to now
						state.last_swipe = SDL_GetTicks();
					}
				} break;

				// Mousewheel scrolling with SDL2.
				case SDL_EVENT_MOUSE_WHEEL: {
					CERR("SDL_EVENT_MOUSE_WHEEL");
					if (event.wheel.y > 0) {
						simulate_key = SDLK_LEFT;
					} else if (event.wheel.y < 0) {
						simulate_key = SDLK_RIGHT;
					}
				} break;
				case SDL_EVENT_MOUSE_MOTION: {
					CERR("SDL_EVENT_MOUSE_MOTION");
					if (Mouse::use_touch_input && event.motion.which != EXSDL_TOUCH_MOUSEID) {
						Mouse::use_touch_input = false;
					}
					gwin->get_win()->screen_to_game(event.motion.x, event.motion.y, gwin->get_fastmouse(), gx, gy);

					Mouse::mouse()->move(gx, gy);
					Mouse::mouse_update = true;

				} break;
					// return;

				case SDL_EVENT_MOUSE_BUTTON_DOWN: {
					SDL_ConvertEventToRenderCoordinates(renderer, &event);
					gwin->get_win()->screen_to_game(event.button.x, event.button.y, gwin->get_fastmouse(), gx, gy);
					CERR("SDL_EVENT_MOUSE_BUTTON_DOWN( " << gx << " , " << gy << " )");
					buttons_down.insert(event.button.button);
					if (event.button.button == 1) {
						simulate_key = Hotspot::HitCheck(hotspots, gx, gy);
						// simulate_key = CheckHotspots(gx, gy);

						if (!simulate_key) {
							// Double click detection
							if (event.button.clicks >= 2) {
								simulate_key = SDLK_RETURN;
							}

							CERR("window size( " << gwin->get_width() << " , " << gwin->get_height() << " )");
							// Touch on the cheat screen will bring up the
							// keyboard but not if the tap was within a 20 pixel
							// border on the edge of the game screen)
							if (SDL_TextInputActive(window)) {
								SDL_StopTextInput(window);
							} else if (gx > 20 && gy > 20 && gx < (gwin->get_width() - 20) && gy < (gwin->get_height() - 20)) {
								TouchUI::startTextInput(window);
							}
						}
					}
				} break;
				case SDL_EVENT_MOUSE_BUTTON_UP: {
					buttons_down.erase(event.button.button);
				} break;

				case SDL_EVENT_KEY_DOWN: {
					buttons_down.insert(int(event.key.key));
				} break;

				case SDL_EVENT_KEY_UP: {
					buttons_down.erase(int(event.key.key));
				} break;

				default:
					break;
				}
			}

			if (simulate_key) {
				std::memset(&event, 0, sizeof(event));
				event.type    = SDL_EVENT_KEY_DOWN;
				event.key.key = simulate_key;
				CERR("simmulate key " << event.key.key);
				// Simulated keys automatically execute the command if possible
				if (state.GetMode() >= CP_HitKey && state.GetMode() <= CP_WrongShapeFile) {
					state.SetMode(CP_Command, true);
				}
			}
			if (event.type != SDL_EVENT_KEY_DOWN) {
				continue;
			}
			SDL_Keycode key_sym = event.key.key;
			SDL_Keymod  key_mod = event.key.mod;
			unicode             = 0;
			if (!Translate_keyboard(event, key_sym, unicode, true)) {
				continue;
			}

			if ((key_sym == SDLK_S) && (key_mod & SDL_KMOD_ALT) && (key_mod & SDL_KMOD_CTRL)) {
				make_screenshot(true);
				return SDLK_UNKNOWN;
			}

			return key_sym;
		}
		gwin->rotatecolours();
		Mouse::mouse()->show();    // Re-display mouse.
		if (gwin->show() || Mouse::mouse_update) {
			Mouse::mouse()->blit_dirty();
		}
		Mouse::mouse()->hide();    // Need to immediately turn off here to prevent
								   // flickering after repaint of whole screen
	}
	// Didn't get a key press before the timeout
	return SDLK_UNKNOWN;
}

bool CheatScreen::SharedInput() {
	SDL_Keycode           key_sym, unicode;
	std::vector<Hotspot*> hotspotsp;
	hotspotsp.reserve(hotspots.size());

	for (auto& hs : hotspots) {
		hotspotsp.push_back(&hs);
	}

	// didn't get a key press, just return
	if (!(key_sym = GetKey(hotspotsp, unicode))) {
		return false;
	}

	if (key_sym == SDLK_ESCAPE) {
		std::memset(state.input, 0, sizeof(state.input));
		// If current mode is needing to press a key return to command
		if (state.GetMode() >= CP_HitKey && state.GetMode() <= CP_WrongShapeFile) {
			state.command = 0;
			state.SetMode(CP_Command, true);
			return false;
		}
		// Escape will cancel current mode
		else if (state.GetMode() != CP_Command) {
			state.command = key_sym;
			state.SetMode(CP_Canceled, true);
			return false;
		}
	}

	if (state.GetMode() == CP_NorthSouth) {
		if (!state.input[0] && (key_sym == SDLK_N || key_sym == SDLK_S)) {
			state.input[0] = char(key_sym);
			state.activate = true;
		}
	} else if (state.GetMode() == CP_WestEast) {
		if (!state.input[0] && (key_sym == SDLK_W || key_sym == SDLK_E)) {
			state.input[0] = char(key_sym);
			state.activate = true;
		}
	} else if (state.GetMode() >= CP_HexXCoord && state.GetMode() <= CP_HexYCoord) {    // Want hex input
		// Activate (if possible)
		if (key_sym == SDLK_RETURN) {
			state.activate = true;
			// Begin New
			// increment/decrement
		} else if (key_sym == SDLK_LEFT || key_sym == SDLK_RIGHT) {
			char* end   = nullptr;
			long  value = std::strtol(state.input, &end, 16);
			if (state.val_max < state.val_min) {
				std::swap(state.val_max, state.val_min);
			}
			if (end == state.input + strlen(state.input)) {
				if (key_sym == SDLK_LEFT && value != state.val_min) {
					value = std::max(value - 1, state.val_min);
				} else if (key_sym == SDLK_RIGHT && value != state.val_max) {
					value = std::min(value + 1, state.val_max);
				}
				if (value < 0) {
					snprintf(state.input, std::size(state.input), "-%lx", -value);
				} else {
					snprintf(state.input, std::size(state.input), "%lx", value);
				}
			}
			// End New
		} else if ((key_sym == SDLK_MINUS) && !state.input[0]) {
			state.input[0] = '-';
		} else if (key_sym < 256 && std::isxdigit(key_sym)) {
			const size_t curlen = std::strlen(state.input);
			if (curlen < (std::size(state.input) - 1)) {
				state.input[curlen]     = char(std::tolower(key_sym));
				state.input[curlen + 1] = 0;
			}
		} else if (key_sym == SDLK_BACKSPACE) {
			const size_t curlen = std::strlen(state.input);
			if (curlen) {
				state.input[curlen - 1] = 0;
			}
		}
	} else if (state.GetMode() == CP_Name) {    // Want Text input
		if (key_sym == SDLK_RETURN) {
			state.activate = true;
		} else if ((unicode < 256 && std::isalnum(unicode)) || unicode == ' ') {
			const size_t curlen = std::strlen(state.input);
			if (curlen < (std::size(state.input) - 1)) {
				state.input[curlen]     = unicode;
				state.input[curlen + 1] = 0;
			}
		} else if (key_sym == SDLK_BACKSPACE) {
			const size_t curlen = std::strlen(state.input);
			if (curlen) {
				state.input[curlen - 1] = 0;
			}
		}
	} else if (state.GetMode() >= CP_ChooseNPC) {    // Need to grab
													 // numerical input
		// Browse shape
		if (state.GetMode() == CP_Shape && !state.input[0] && key_sym == SDLK_B) {
			cheat.shape_browser();
			state.input[0] = 'b';
			state.activate = true;
		}

		if (key_sym == SDLK_LEFT || key_sym == SDLK_RIGHT) {
			char* end   = nullptr;
			long  value = std::strtol(state.input, &end, 10);

			if (state.val_max < state.val_min) {
				std::swap(state.val_max, state.val_min);
			}
			if (end == state.input + strlen(state.input)) {
				if (key_sym == SDLK_LEFT && value != state.val_min) {
					value = std::max(value - 1, state.val_min);
				} else if (key_sym == SDLK_RIGHT && value != state.val_max) {
					value = std::min(value + 1, state.val_max);
				}
				snprintf(state.input, std::size(state.input) - 1, "%ld", value);
			}
		}
		// Activate (if possible)
		else if (key_sym == SDLK_RETURN) {
			state.activate = true;
		} else if ((key_sym == SDLK_MINUS) && !state.input[0]) {
			state.input[0] = '-';
		} else if (key_sym < 256 && std::isdigit(key_sym)) {
			const size_t curlen = std::strlen(state.input);
			if (curlen < (std::size(state.input) - 1)) {
				state.input[curlen]     = key_sym;
				state.input[curlen + 1] = 0;
			}
		} else if (key_sym == SDLK_BACKSPACE) {
			const auto curlen = std::strlen(state.input);
			if (curlen) {
				state.input[curlen - 1] = 0;
			}
		}
	} else {
		char c = key_sym;

		// Translate arrow key into the characters we use for arrows
		switch (key_sym) {
		case SDLK_UP: {
			c = '^';
		} break;

		case SDLK_DOWN: {
			c = 'V';
		} break;

		case SDLK_RIGHT: {
			c = '>';
		} break;

		case SDLK_LEFT: {
			c = '<';
		} break;

		default: {
		} break;
		}
		// Set input to the typed character so it is shown with the
		// prompt
		std::memset(state.input, 0, sizeof(state.input));
		state.input[0] = c;
		state.input[1] = 0;

		if (state.GetMode()) {    // Just want a key pressed
			state.SetMode(CP_Command, true);
			state.command = 0;
		} else {    // Need the key pressed
			state.command       = key_sym;
			state.highlighttime = SDL_GetTicks() + 1000;
			state.highlight     = state.command;
			return true;
		}
	}
	return false;
}

void CheatScreen::RunMenu(std::shared_ptr<Menu> menu) {
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int prompty = 81;
	const int promptx = 15;
#else
	const int prompty = maxy - 18;
	const int promptx = 0;
#endif

	std::shared_ptr<MenuCommand> message_command = std::make_shared<MenuCommand>();
	Uint32                       messagetime     = 0;
	const Uint32                 messagetimeout  = 1000;

	message_command->inputs.push_back(std::make_shared<InputHandlers::PressAKey>());
	std::stack<std::shared_ptr<MenuCommand>> input_stack;
	MenuCommand*                             last = nullptr;
	std::vector<Hotspot*>                    hotspots;
	input_stack.push(menu);

	Uint32       highlighted      = SDLK_UNKNOWN;
	Uint32       highlighttime    = 0;
	const Uint32 highlighttimeout = 500;
	SDL_Keycode  key_sym          = SDLK_UNKNOWN;
	SDL_Keycode  unicode          = SDLK_UNKNOWN;

	// While the input stack is ont enpty run it
	while (!input_stack.empty()) {
		auto current = input_stack.top();
		if (current.get() != last) {
			current->ResetPhase();
			last = current.get();
		}

		try {
			while (current && current->BeginPhase()) {
				auto ih = current->GetInputHandler();

				for (;;) {
					current->run();
					hotspots.clear();
					gwin->clear_screen();
					current->paint_display();
					current->GatherHotspots(hotspots);
					// 64 is big enough as the screen res is only big enough for 40x25 text
					char buf[64];

					ih->GetPromptMessage(buf, std::size(buf));
					int promptmsgwidth = font->paint_text_fixedwidth(ibuf, buf, promptx, prompty + 9, 8, fontcolor.colors);

					int cursor_offset = ih->PaintPrompt(promptx, prompty, key_sym);
					// Flash cursor with half second duty cycle
					if (SDL_GetTicks() % 1000 > 500) {
						font->paint_text_fixedwidth(
								ibuf, Strings::CURSOR, promptx + cursor_offset, prompty, 8,
								ih->input_full() ? fontcolor2.colors : fontcolor.colors);
					}
					ih->ArrangeHotspots(promptx + promptmsgwidth + 8, prompty, 2);
					ih->GatherHotspots(hotspots);

					if (highlighttime && highlighttime < SDL_GetTicks()) {
						highlighted   = SDLK_UNKNOWN;
						highlighttime = 0;
					}
					if (messagetime && messagetime < SDL_GetTicks()) {
						highlighted = SDLK_UNKNOWN;
						messagetime = 0;
						if (current == message_command) {
							current = nullptr;
							break;
						}
					}
					for (const auto hs : hotspots) {
						hs->Paint(highlighted, Mouse::mouse()->get_mousex(), Mouse::mouse()->get_mousey());
					}
					Mouse::mouse()->show();
					gwin->get_win()->show();
					Mouse::mouse()->hide();    // Must immediately hide to prevent flickering
					key_sym = GetKey(hotspots, unicode);

					if (key_sym != SDLK_UNKNOWN) {
						// Highlight the key
						highlighttime = SDL_GetTicks() + highlighttimeout;
						highlighted   = key_sym;

						// Escape always leaves current and doesn't get passed to input handler
						if (key_sym == SDLK_ESCAPE) {
							// If not Showing the Message Command throw the exception to return to the previousmenu
							if (current != message_command) {
								// Show Cancelled Message if not leaving a menu
								throw MenuCommandException{
										!dynamic_cast<Menu*>(current.get()) ? Strings::CANCELLED : std::string(), true};
							}
							current->cancelled();
							current = nullptr;
							break;
						} else if (ih->OnInput(key_sym)) {
							ih->Parse();
							break;
						}
					}
				}

				// Current could be gone here
				if (current) {
					current->EndPhase();
				}
			}
			auto command = current;

			// Activate the command now that all it's needed inputphases are done Replace the command with the result of Activate
			if (command) {
				command = command->Activate(key_sym);
			}

			// New Command so put it at the top of the stack
			if (command) {
				command->ResetPhase();

				command->below = input_stack.top();
				input_stack.push(command);
			}
			// If we aren't switching to a new MenuCommand, pop top of stack
			else {
				input_stack.pop();
			}
		} catch (MenuCommandException& ex) {
			if (ex.return_to_menu) {
				// pop till we reach a menu or nothing
				do {
					input_stack.top()->cancelled();
					input_stack.pop();
				} while (!input_stack.empty() && dynamic_cast<Menu*>(input_stack.top().get()) == nullptr);
			}
			// If the message is empty don't try to show it
			if (!ex.msg.empty()) {
				message_command->below = input_stack.top();
				message_command->inputs[0]->SetPromptMessage(std::move(ex.msg));
				input_stack.push(message_command);
				messagetime = SDL_GetTicks() + messagetimeout;
			}
		}
	}

	WaitButtonsUp();
}

void CheatScreen::SharedMenu() {
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx = 15;
	// const int offsety1 = 73;
	// const int offsety2 = 55;
	// const int offsetx1 = 160;
	// const int offsety4 = 36;
	const int offsety5 = 72;
#else
	const int offsetx = 0;
	// const int offsety1 = 0;
	// const int offsety2 = 0;
	// const int offsetx1 = 160;
	// const int offsety4 = maxy - 45;
	const int offsety5 = maxy - 36;
#endif    // eXit
	AddMenuItem(offsetx + 160, offsety5 + 9, SDLK_ESCAPE, Strings::Exit());
}

SDL_Keycode CheatScreen::Hotspot::HitCheck(const std::vector<Hotspot*>& hotspots, int mx, int my, int radius) {
	// Find the nearest hotspot
	SDL_Keycode nearest     = SDLK_UNKNOWN;
	int         nearestdist = INT_MAX;

	for (auto hs : hotspots) {
		if (hs) {
			for (size_t k = 0; k < std::size(hs->keycode); k++) {
				TileRect rect = hs->GetRect(k,false);
				int      dist = rect.distance(mx, my);
				if (rect && dist < nearestdist) {
					nearest     = hs->keycode[k];
					nearestdist = dist;
				}
			}
		}
	}
	// Only return it if it is within the radius
	if (nearestdist <= radius) {
		return FixUppercaseKeycode(nearest);
	}
	return SDLK_UNKNOWN;
}

void CheatScreen::PaintHotspots() {
	int mx = Mouse::mouse()->get_mousex();
	int my = Mouse::mouse()->get_mousey();
	for (const auto& hs : hotspots) {
		auto r = hs.ToTR();
		if (r) {
			if (r.has_point(mx, my)) {
				// Draw mouse hover
				ibuf->fill_translucent8(0, r.w, r.h, r.x, r.y, hovertable);
			}
			// Draw the box in bright yellow
			// ibuf->draw_box(
			//		hs.x - 2, hs.y - 2, hs.w + 4, hs.h + 4, 2, 255, 5);
		}
	}
}

//
// Normal
//

void CheatScreen::NormalLoop() {
	bool looping = true;

	ClearState clear(state);
	while (looping) {
		hotspots.clear();
		gwin->clear_screen();

		// First the display
		NormalDisplay();

		// Now the Menu Column
		NormalMenu();

		// Finally the Prompt...
		SharedPrompt();

		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			NormalActivate();
			state.activate = false;
			continue;
		}

		if (SharedInput()) {
			looping = NormalCheck();
		}
	}
	WaitButtonsUp();
}

void CheatScreen::NormalDisplay() {
	char buf[512];
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 108;
	const int offsety2 = 54;
	const int offsety3 = 0;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	const int offsety2 = 0;
	const int offsety3 = 45;
#endif
	const int        curmap = gwin->get_map()->get_num();
	const Tile_coord t      = gwin->get_main_actor()->get_tile();

	font->paint_text_fixedwidth(ibuf, "Advanced Option Cheat Screen", offsetx, offsety1, 8, fontcolor.colors);

	if (Game::get_game_type() == BLACK_GATE) {
		snprintf(buf, sizeof(buf), "Running \"Ultima VII: The Black Gate\"");
	} else if (Game::get_game_type() == SERPENT_ISLE) {
		snprintf(buf, sizeof(buf), "Running \"Ultima VII Part 2: Serpent Isle\"");
	} else {
		snprintf(buf, sizeof(buf), "Running Unknown Game Type %i", Game::get_game_type());
	}

	font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 18, 8, fontcolor.colors);

	strcpy(buf, "Exult Version " VERSION " Rev: ");
	auto rev    = VersionGetGitRevision(true);
	int  curlen = strlen(buf);
	rev.copy(buf + strlen(buf), rev.size());
	// Need to null terminate after copy
	buf[curlen + rev.size()] = 0;
	font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 27, 8, fontcolor.colors);

	font->paint_text_fixedwidth(ibuf, "Compiled " __DATE__ " " __TIME__, offsetx, offsety1 + 36, 8, fontcolor.colors);

	snprintf(
			buf, sizeof(buf), "Current time: %i:%02i %s  Day: %i", ((clock->get_hour() + 11) % 12) + 1, clock->get_minute(),
			clock->get_hour() < 12 ? "AM" : "PM", clock->get_day());
	font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety3, 8, fontcolor.colors);

	const int longi = ((t.tx - 0x3A5) / 10);
	const int lati  = ((t.ty - 0x46E) / 10);
	snprintf(
			buf, sizeof(buf), "Coordinates %d %s %d %s, Map #%d", abs(lati), (lati < 0 ? "North" : "South"), abs(longi),
			(longi < 0 ? "West" : "East"), curmap);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 63 - offsety2, 8, fontcolor.colors);

	snprintf(buf, sizeof(buf), "Coords in hex (%04x, %04x, %02x)", t.tx, t.ty, t.tz);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 72 - offsety2, 8, fontcolor.colors);

	snprintf(buf, sizeof(buf), "Coords in dec (%04i, %04i, %02i)", t.tx, t.ty, t.tz);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 81 - offsety2, 8, fontcolor.colors);
}

void CheatScreen::NormalMenu() {
	char buf[512];
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 73;
	const int offsety2 = 55;
	const int offsetx1 = 160;
	const int offsety4 = 36;
	// const int offsety5 = 72;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	const int offsety2 = 0;
	const int offsetx1 = 0;
	const int offsety4 = maxy - 45;
	// const int offsety5 = maxy - 36;
#endif

	// Left Column

	// Use
#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(CHEAT_SCREEN_TEST_MOBILE)
	// Paperdolls can be toggled in the gumps, no need here for small screens
	Shape_manager* sman = Shape_manager::get_instance();
	if (sman->can_use_paperdolls() && sman->are_paperdolls_enabled()) {
		snprintf(buf, sizeof(buf), "aperdolls..: Yes");
	} else {
		snprintf(buf, sizeof(buf), "aperdolls..:  No");
	}
	AddMenuItem(offsetx, maxy - 99, SDLK_P, buf);

#endif

	// GodMode
	snprintf(buf, sizeof(buf), "od Mode....: %3s", cheat.in_god_mode() ? "On" : "Off");
	AddMenuItem(offsetx, maxy - offsety1 - 90, SDLK_G, buf);

	// Archwizzard Mode
	snprintf(buf, sizeof(buf), "izard Mode.: %3s", cheat.in_wizard_mode() ? "On" : "Off");
	AddMenuItem(offsetx, maxy - offsety1 - 81, SDLK_W, buf);

	// Infravision
	snprintf(buf, sizeof(buf), "nfravision.: %3s", cheat.in_infravision() ? "On" : "Off");
	AddMenuItem(offsetx, maxy - offsety1 - 72, SDLK_I, buf);

	// Hackmover
	snprintf(buf, sizeof(buf), "ack Mover..: %3s", cheat.in_hack_mover() ? "Yes" : "No");
	AddMenuItem(offsetx, maxy - offsety1 - 63, SDLK_H, buf);

	// Eggs
	snprintf(buf, sizeof(buf), "ggs Visible: %3s", gwin->paint_eggs ? "Yes" : "No");
	AddMenuItem(offsetx, maxy - offsety1 - 54, SDLK_E, buf);

	// Set Time
	AddMenuItem(offsetx + offsetx1, offsety4, SDLK_S, "et Time");

	// Right Column

	// NPC Tool
	AddMenuItem(offsetx + 160, maxy - offsety2 - 99, SDLK_N, "PC Tool");

	// Global Flag Modify
	AddMenuItem(offsetx + 160, maxy - offsety2 - 90, SDLK_F, "lag Modifier");

	// Teleport
	AddMenuItem(offsetx + 160, maxy - offsety2 - 81, SDLK_T, "eleport");

#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(CHEAT_SCREEN_TEST_MOBILE)
	// for small screens taking the liberty of leaving that out
	// Time Rate
	snprintf(buf, sizeof(buf), " Time Rate:%4i", clock->get_time_rate());
	AddLeftRightMenuItem(offsetx + 160, offsety4, buf, clock->get_time_rate() > 1, clock->get_time_rate() < 20, true, false);
#endif

	SharedMenu();
}

void CheatScreen::NormalActivate() {
	const int      npc  = std::atoi(state.input);
	Shape_manager* sman = Shape_manager::get_instance();

	state.SetMode(CP_Command, false);

	switch (state.command) {
		// God Mode
	case SDLK_G:
		cheat.toggle_god();
		break;

		// Wizard Mode
	case SDLK_W:
		cheat.toggle_wizard();
		break;

		// Infravision
	case SDLK_I:
		cheat.toggle_infravision();
		pal.apply();
		break;

		// Eggs
	case SDLK_E:
		cheat.toggle_eggs();
		break;

		// Hack mover
	case SDLK_H:
		cheat.toggle_hack_mover();
		break;

		// Set Time
	case SDLK_S:
		state.SetMode(TimeSetLoop());
		break;

		// - Time Rate
	case '<':
		if (clock->get_time_rate() > 0) {
			clock->set_time_rate(clock->get_time_rate() - 1);
		}
		break;

		// + Time Rate
	case '>':
		if (clock->get_time_rate() < 20) {
			clock->set_time_rate(clock->get_time_rate() + 1);
		}
		break;

		// Teleport
	case SDLK_T:
		TeleportLoop();
		break;

		// NPC Tool
	case SDLK_N:
		if (npc < 0 || (npc >= 356 && npc <= 359)) {
			state.SetMode(CP_InvalidNPC, false);
		} else if (!state.input[0]) {
			NPCLoop(-1);
		} else {
			state.SetMode(NPCLoop(npc));
		}
		break;

		// Global Flag Editor
	case SDLK_F:
		if (npc < 0) {
			state.SetMode(CP_InvalidValue, false);
		} else if (static_cast<size_t>(npc) > Usecode_machine::last_gflag) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_Canceled, false);
		} else {
			state.SetMode(GlobalFlagLoop(npc));
		}
		break;

		// Paperdolls
	case SDLK_P:
		if ((Game::get_game_type() == BLACK_GATE || Game::get_game_type() == EXULT_DEVEL_GAME) && sman->can_use_paperdolls()) {
			sman->set_paperdoll_status(!sman->are_paperdolls_enabled());
			config->set("config/gameplay/bg_paperdolls", sman->are_paperdolls_enabled() ? "yes" : "no", true);
		}
		break;

	default:
		break;
	}

	std::memset(state.input, 0, sizeof(state.input));

	state.command = 0;
}

// Checks the state.input
bool CheatScreen::NormalCheck() {
	switch (state.command) {
		// Simple commands
	case SDLK_T:    // Teleport
	case SDLK_G:    // God Mode
	case SDLK_W:    // Wizard
	case SDLK_I:    // iNfravision
	case SDLK_S:    // Set Time
	case SDLK_E:    // Eggs
	case SDLK_H:    // Hack Mover
	case SDLK_C:    // Create Item
	case SDLK_P:    // Paperdolls
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		state.activate = true;
		break;

		// - Time
	case SDLK_LEFT:
		state.command = '<';
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		state.activate = true;
		break;

		// + Time
	case SDLK_RIGHT:
		state.command = '>';
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		state.activate = true;
		break;

		// NPC Tool
	case SDLK_N:
		state.SetMode(CP_ChooseNPC);
		state.val_min = 0;
		state.val_max = gwin->get_num_npcs() - 1;
		break;

		// Global Flag Editor
	case SDLK_F:
		state.SetMode(CP_GFlagNum);
		state.val_min = 0;
		state.val_max = Usecode_machine::last_gflag;
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

int CheatScreen::PaintArrow(int offsetx, int offsety, int type) {
	// Need to draw arrows with overlapping characters
	// up arrow
	if (type == '^') {
		// Use an i as the stem of the arrow
		font->paint_text_fixedwidth(ibuf, "i", offsetx, offsety, 8, fontcolor.colors);
		// Draw a black line to narrow the stem
		ibuf->draw_line8(0, offsetx + 4, offsety, offsetx + 4, offsety + 8);
		// Draw point of arrow
		font->paint_text_fixedwidth(ibuf, "^", offsetx, offsety, 8, fontcolor.colors);
	}    // down arrow
	else if (type == 'V') {
		// Use an l as the stem of the arrow
		font->paint_text_fixedwidth(ibuf, "l", offsetx, offsety, 8, fontcolor.colors);
		// Draw black lines to narrow the stem
		ibuf->draw_line8(0, offsetx + 2, offsety, offsetx + 2, offsety + 2);
		ibuf->draw_line8(0, offsetx + 4, offsety, offsetx + 4, offsety + 6);

		// Draw point of arrow
		font->paint_text_fixedwidth(ibuf, "m", offsetx, offsety + 2, 8, fontcolor.colors);

		// Paint black lines to make it pointy
		ibuf->draw_line8(0, offsetx + 0, offsety + 5, offsetx + 0, offsety + 8);
		ibuf->draw_line8(0, offsetx + 6, offsety + 5, offsetx + 6, offsety + 8);
		ibuf->draw_line8(0, offsetx + 1, offsety + 6, offsetx + 1, offsety + 8);
		ibuf->draw_line8(0, offsetx + 5, offsety + 6, offsetx + 5, offsety + 8);
	}    // left arrow
	else if (type == '<') {
		// Stem of arrow
		font->paint_text_fixedwidth(ibuf, "-", offsetx + 1, offsety, 8, fontcolor.colors);
		// Paint black line  to narrow the stem
		ibuf->draw_line8(0, offsetx + 0, offsety + 4, offsetx + 7, offsety + 4);
		// Point of  arrow
		font->paint_text_fixedwidth(ibuf, "<", offsetx, offsety, 8, fontcolor.colors);
		// Paint black line to make it pointier
		ibuf->draw_line8(0, offsetx + 1, offsety + 4, offsetx + 4, offsety + 7);
		ibuf->put_pixel8(0, offsetx + 5, offsety + 7);
	}    // Right Arrow
	else if (type == '>') {
		// Stem of arrow
		font->paint_text_fixedwidth(ibuf, "-", offsetx, offsety, 8, fontcolor.colors);
		// Paint black line to narrow the stem
		ibuf->draw_line8(0, offsetx + 0, offsety + 4, offsetx + 7, offsety + 4);
		// Point of arrow
		font->paint_text_fixedwidth(ibuf, ">", offsetx + 2, offsety, 8, fontcolor.colors);
		// Paint black line to make it pointier
		ibuf->draw_line8(0, offsetx + 7, offsety + 4, offsetx + 3, offsety + 7);
	} else {
		return 0;
	}

	return 8;
}

//
// TimeSet
//

CheatScreen::Cheat_Prompt CheatScreen::TimeSetLoop() {
	int        day  = 0;
	int        hour = 0;
	ClearState clear(state);
	state.SetMode(CP_Day);
	state.val_min = 0;
	state.val_max = INT_MAX;    // This seems unbounded
	while (true) {
		hotspots.clear();
		gwin->clear_screen();

		// First the display
		NormalDisplay();

		// Now the Menu Column
		NormalMenu();

		// Finally the Prompt...
		SharedPrompt();

		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			const int val = std::atoi(state.input);

			if (val < 0) {
				return CP_InvalidTime;
			} else if (state.GetMode() == CP_Day) {
				day = val;
				state.SetMode(CP_Hour);
				state.val_min = 0;
				state.val_max = 23;
			} else if (val > 59) {
				return CP_InvalidTime;
			} else if (state.GetMode() == CP_Minute) {
				clock->reset();
				clock->set_day(day);
				clock->set_hour(hour);
				clock->set_minute(val);
				break;
			} else if (val > 23) {
				return CP_InvalidTime;
			} else if (state.GetMode() == CP_Hour) {
				hour = val;
				state.SetMode(CP_Minute);
				state.val_min = 0;
				state.val_max = 59;
			}

			state.activate = false;
			state.input[0] = 0;
			state.input[1] = 0;
			state.input[2] = 0;
			state.input[3] = 0;
			state.command  = 0;
			continue;
		}

		SharedInput();
		if (state.GetMode() == CP_Canceled) {
			return CP_Canceled;
		}
	}

	return CP_ClockSet;
}

//
// Global Flags
//

void CheatScreen::load_global_flag_names() {
	global_flag_names_loaded = true;
	if (is_system_path_defined("<PATCH>") && U7exists(PATCH_GLOBAL_FLAGS)) {
		IFileDataSource flagsfile(PATCH_GLOBAL_FLAGS, true);
		if (flagsfile.good()) {
			Text_msg_file_reader reader(flagsfile);
			reader.get_global_section_strings(global_flag_names);
		}
	} else {
		const str_int_pair& resource = game->get_resource("files/global_flags");
		IExultDataSource    flagsfile(resource.str, resource.num);
		if (flagsfile.good()) {
			Text_msg_file_reader reader(flagsfile);
			reader.get_global_section_strings(global_flag_names);
		}
	}
}

CheatScreen::Cheat_Prompt CheatScreen::GlobalFlagLoop(int num) {
	bool looping = true;
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 83;
	// const int offsety2 = 72;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	// const int offsety2 = maxy - 36;
#endif

	int  i;
	char buf[64];

	Usecode_machine* usecode = Game_window::get_instance()->get_usecode();

	ClearState clear(state);
	while (looping) {
		hotspots.clear();
		gwin->clear_screen();

#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
		// on small screens we want lean and mean, so begone NormalDisplay
		font->paint_text_fixedwidth(ibuf, "Global Flags", 15, 0, 8, fontcolor.colors);
#else
		NormalDisplay();
#endif

		// First the info
		snprintf(buf, sizeof(buf), "Global Flag %i", num);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 99, 8, fontcolor.colors);

		// Load global flag names if not yet loaded
		if (!global_flag_names_loaded) {
			load_global_flag_names();
		}
		// Display the flag name if available
		if (num >= 0 && num < static_cast<int>(global_flag_names.size()) && !global_flag_names[num].empty()) {
			snprintf(buf, sizeof(buf), "%s", global_flag_names[num].c_str());
		} else {
			snprintf(buf, sizeof(buf), "(unnamed)");
		}
		font->paint_text_fixedwidth(ibuf, buf, offsetx + 130, maxy - offsety1 - 99, 8, fontcolor.colors);

		bool flagset = usecode->get_global_flag(num).value_or(false);
		snprintf(buf, sizeof(buf), "Flag is %s", flagset ? "SET" : "UNSET");
		font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 90, 8, fontcolor.colors);

		// Now the Menu Column
		if (!flagset) {
			AddMenuItem(offsetx + 130, maxy - offsety1 - 90, SDLK_S, "et Flag");
		} else {
			AddMenuItem(offsetx + 130, maxy - offsety1 - 90, SDLK_U, "nset Flag");
		}

		// Change Flag
		AddMenuItem(offsetx, maxy - offsety1 - 72, SDLK_UP, " Change Flag");
		AddLeftRightMenuItem(
				offsetx, maxy - offsety1 - 63, "Scroll Flags", num > 0, num < static_cast<int>(Usecode_machine::last_gflag), false,
				true);

		SharedMenu();

		// Finally the Prompt...
		SharedPrompt();

		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			state.SetMode(CP_Command, false);
			if (state.command == '<') {    // Decrement
				num = std::max(num - 1, 0);
			} else if (state.command == '>') {    // Increment
				num = std::min<size_t>(num + 1, Usecode_machine::last_gflag);
			} else if (state.command == '^') {    // Change Flag
				i = std::atoi(state.input);
				if (i < 0 || static_cast<size_t>(i) > Usecode_machine::last_gflag) {
					state.SetMode(CP_InvalidValue, false);
				} else if (state.input[0]) {
					num = i;
				}
			} else if (state.command == 's') {    // Set
				usecode->set_global_flag(num, true);
			} else if (state.command == 'u') {    // Unset
				usecode->set_global_flag(num, false);
			}
			std::memset(state.input, 0, sizeof(state.input));

			state.command  = 0;
			state.activate = false;
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
				state.SetMode(CP_GFlagNum);
				state.val_min = 0;
				state.val_max = Usecode_machine::last_gflag;
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

//
// Teleport screen
//

void CheatScreen::TeleportLoop() {
	bool looping = true;

	int prev = 0;

	ClearState clear(state);
	while (looping) {
		hotspots.clear();
		gwin->clear_screen();

		// First the display
		TeleportDisplay();

		// Now the Menu Column
		TeleportMenu();

		// Finally the Prompt...
		SharedPrompt();

		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			TeleportActivate(prev);
			state.activate = false;
			continue;
		}

		if (SharedInput()) {
			looping = TeleportCheck();
		}
	}
}

void CheatScreen::TeleportDisplay() {
	char             buf[512];
	const Tile_coord t       = gwin->get_main_actor()->get_tile();
	const int        curmap  = gwin->get_map()->get_num();
	const int        highest = Get_highest_map();
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 54;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
#endif

#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	font->paint_text_fixedwidth(ibuf, "Teleport Menu - Dangerous!", offsetx, 0, 8, fontcolor.colors);
#else
	font->paint_text_fixedwidth(ibuf, "Teleport Menu", offsetx, 0, 8, fontcolor.colors);
	font->paint_text_fixedwidth(ibuf, "Dangerous - use with care!", offsetx, 18, 8, fontcolor.colors);
#endif

	const int longi = ((t.tx - 0x3A5) / 10);
	const int lati  = ((t.ty - 0x46E) / 10);
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	snprintf(
			buf, sizeof(buf), "Coords %d %s %d %s, Map #%d of %d", abs(lati), (lati < 0 ? "North" : "South"), abs(longi),
			(longi < 0 ? "West" : "East"), curmap, highest);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 9, 8, fontcolor.colors);
#else
	snprintf(
			buf, sizeof(buf), "Coordinates   %d %s %d %s", abs(lati), (lati < 0 ? "North" : "South"), abs(longi),
			(longi < 0 ? "West" : "East"));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 63, 8, fontcolor.colors);
#endif

	snprintf(buf, sizeof(buf), "Coords in hex (%04x, %04x, %02x)", t.tx, t.ty, t.tz);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 72 - offsety1, 8, fontcolor.colors);

	snprintf(buf, sizeof(buf), "Coords in dec (%04i, %04i, %02i)", t.tx, t.ty, t.tz);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 81 - offsety1, 8, fontcolor.colors);

#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(CHEAT_SCREEN_TEST_MOBILE)
	snprintf(buf, sizeof(buf), "On Map #%d of %d", curmap, highest);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 90, 8, fontcolor.colors);
#endif
}

void CheatScreen::TeleportMenu() {
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 64;
	const int offsetx2 = 175;
	const int offsety2 = 63;
	// const int offsety3 = 72;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	const int offsetx2 = offsetx;
	const int offsety2 = maxy - 63;
	// const int offsety3 = maxy - 36;
#endif

	// Left Column
	// Geo
	AddMenuItem(offsetx, maxy - offsety1 - 99, SDLK_G, "eographic Coordinates");
	// Hex
	AddMenuItem(offsetx, maxy - offsety1 - 90, SDLK_H, "exadecimal Coordinates");

	// Dec
	AddMenuItem(offsetx, maxy - offsety1 - 81, SDLK_D, "ecimal Coordinates");

	// NPC
	AddMenuItem(offsetx, maxy - offsety1 - 72, SDLK_N, "PC Number");

	AddMenuItem(offsetx2, offsety2, SDLK_M, "ap Number");
	// Map

	SharedMenu();
}

void CheatScreen::TeleportActivate(int& prev) {
	int        i = std::atoi(state.input);
	static int lat;
	Tile_coord t       = gwin->get_main_actor()->get_tile();
	const int  highest = Get_highest_map();

	state.SetMode(CP_Command, false);
	switch (state.command) {
	case SDLK_G:    // North or South
		if (!state.input[0]) {
			state.SetMode(CP_NorthSouth);
			state.command = 'g';
		} else if (state.input[0] == 'n' || state.input[0] == 's') {
			prev = state.input[0];
			if (prev == 'n') {
				state.SetMode(CP_NLatitude);
				state.val_max = 0;
				state.val_min = 113;
			} else {
				state.SetMode(CP_SLatitude);
				state.val_max = 0;
				state.val_min = 193;
			}
			state.command = 'a';
		} else {
			state.SetMode(CP_InvalidValue, false);
		}
		break;

	case SDLK_A:    // latitude
		if (i < 0 || (prev == 'n' && i > 113) || (prev == 's' && i > 193)) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			if (prev == 'n') {
				state.SetMode(CP_NLatitude);
				state.val_max = 0;
				state.val_min = 113;
			} else {
				state.SetMode(CP_SLatitude);
				state.val_max = 0;
				state.val_min = 193;
			}
			state.command = 'a';
		} else {
			if (prev == 'n') {
				lat = ((i * -10) + 0x46E);
			} else {
				lat = ((i * 10) + 0x46E);
			}
			state.SetMode(CP_WestEast);
			state.command = 'b';
		}
		break;

	case SDLK_B:    // West or East
		if (!state.input[0]) {
			state.SetMode(CP_WestEast);
			state.command = 'b';
		} else if (state.input[0] == 'w' || state.input[0] == 'e') {
			prev = state.input[0];
			if (prev == 'w') {
				state.SetMode(CP_WLongitude);
				state.val_max = 0;
				state.val_min = 93;
			} else {
				state.SetMode(CP_ELongitude);
				state.val_max = 0;
				state.val_min = 213;
			}
			state.command = 'c';
		} else {
			state.SetMode(CP_InvalidValue, false);
		}
		break;

	case SDLK_C:    // longitude
		if (i < 0 || (prev == 'w' && i > 93) || (prev == 'e' && i > 213)) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			if (prev == 'w') {
				state.SetMode(CP_WLongitude);
				state.val_max = 0;
				state.val_min = 93;
			} else {
				state.SetMode(CP_ELongitude);
				state.val_max = 0;
				state.val_min = 213;
			}
			state.command = 'c';
		} else {
			if (prev == 'w') {
				t.tx = ((i * -10) + 0x3A5);
			} else {
				t.tx = ((i * 10) + 0x3A5);
			}
			t.ty = lat;
			t.tz = 0;
			gwin->teleport_party(t);
		}
		break;

	case SDLK_H:    // hex X coord
		i = strtol(state.input, nullptr, 16);
		if (i < 0 || i > c_num_tiles) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_HexXCoord);
			state.val_min = 0;
			state.val_max = c_num_tiles;
			state.command = 'h';
		} else {
			prev = i;
			state.SetMode(CP_HexYCoord);
			state.val_min = 0;
			state.val_max = c_num_tiles;
			state.val_min = 0;
			state.val_max = c_num_tiles;
			state.command = 'i';
		}
		state.val_max = 0;
		state.val_min = c_num_tiles;
		break;

	case SDLK_I:    // hex Y coord
		i = strtol(state.input, nullptr, 16);
		if (i < 0 || i > c_num_tiles) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_HexYCoord);
			state.val_min = 0;
			state.val_max = c_num_tiles;
			state.command = 'i';
		} else {
			t.tx = prev;
			t.ty = i;
			t.tz = 0;
			gwin->teleport_party(t);
		}
		break;

	case SDLK_D:    // dec X coord
		if (i < 0 || i > c_num_tiles) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_XCoord);
			state.command = 'd';
			state.val_min = 0;
			state.val_max = c_num_tiles;
		} else {
			prev = i;
			state.SetMode(CP_YCoord);
			state.command = 'e';
			state.val_min = 0;
			state.val_max = c_num_tiles;
		}
		break;

	case SDLK_E:    // dec Y coord
		if (i < 0 || i > c_num_tiles) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_YCoord);
			state.val_min = 0;
			state.val_max = c_num_tiles;
			state.command = 'e';
		} else {
			t.tx = prev;
			t.ty = i;
			t.tz = 0;
			gwin->teleport_party(t);
		}
		break;

	case SDLK_N:    // NPC
		if (i < 0 || (i >= 356 && i <= 359)) {
			state.SetMode(CP_InvalidNPC);
		} else {
			Actor* actor = gwin->get_npc(i);
			Game_window::get_instance()->teleport_party(actor->get_tile(), false, actor->get_map_num());
		}
		break;

	case SDLK_M:    // map
		if ((i < 0 || i > 255) || i > highest) {
			state.SetMode(CP_InvalidValue, false);
		} else {
			gwin->teleport_party(gwin->get_main_actor()->get_tile(), true, i);
		}
		break;

	default:
		break;
	}
	std::memset(state.input, 0, sizeof(state.input));
}

// Checks the state.input
bool CheatScreen::TeleportCheck() {
	ignore_unused_variable_warning(state.activate);
	switch (state.command) {
		// Simple commands
	case SDLK_G:    // geographic
		state.SetMode(CP_NorthSouth);
		return true;

	case SDLK_H:    // hex
		state.SetMode(CP_HexXCoord);
		state.val_min = 0;
		state.val_max = c_num_tiles;
		return true;

	case SDLK_D:    // dec teleport
		state.SetMode(CP_XCoord);
		state.val_min = 0;
		state.val_max = c_num_tiles;
		return true;

	case SDLK_N:    // NPC teleport
		state.SetMode(CP_ChooseNPC);
		break;

	case SDLK_M:    // NPC teleport
		state.SetMode(CP_EnterValue);
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
		state.command = 0;
		state.SetMode(CP_InvalidCom, false);
		break;
	}

	return true;
}

int CheatScreen::PaintKeyName(int offsetx, int offsety, SDL_Keycode keycode) {
	switch (keycode) {
	case SDLK_UP:
		return PaintArrow(offsetx, offsety, '^');
	case SDLK_DOWN:
		return PaintArrow(offsetx, offsety, 'V');
	case SDLK_LEFT:
		return PaintArrow(offsetx, offsety, '<');
	case SDLK_RIGHT:
		return PaintArrow(offsetx, offsety, '>');
	default:
		return font->paint_text_fixedwidth(ibuf, getKeyName(keycode), offsetx, offsety, 8, fontcolor.colors);
	}
}

int CheatScreen::AddMenuItem(int offsetx, int offsety, SDL_Keycode keycode, const char* label) {
	int keywidth = PaintKeyName(offsetx + 8, offsety, keycode);
	font->paint_text_fixedwidth(ibuf, "[", offsetx, offsety, 8, fontcolor.colors);
	font->paint_text_fixedwidth(ibuf, "]", offsetx + keywidth + 8, offsety, 8, fontcolor.colors);
	int labelstart = 16 + keywidth;
	int labelwidth = font->paint_text_fixedwidth(ibuf, label, offsetx + labelstart, offsety, 8, fontcolor.colors);
	hotspots.push_back(Hotspot(keycode, offsetx, offsety, 16 + keywidth, 8));
	if (state.highlight == keycode) {
		ibuf->fill_translucent8(0, 8 * (int(std::strlen(label)) + 2) + keywidth, 8, offsetx, offsety, highlighttable);
	}
	return labelstart + labelwidth;
}

CheatScreen::Hotspot::Hotspot(int x, int y, SDL_Keycode keycode1, std::string&& label, SDL_Keycode keycode2)
		: keycode{keycode1, keycode2}, namew{GetKeyNameWidth(keycode1), GetKeyNameWidth(keycode2)}, label(std::move(label)),
		  label_only(keycode1 == 0 && keycode2 == 0), x(x), y(y) {}

void CheatScreen::Hotspot::Paint(SDL_Keycode highlighted, int hoverx, int hovery) const {
	auto  font       = cscreen->font;
	auto& fontcolor  = cscreen->fontcolor;
	auto  ibuf       = cscreen->ibuf;
	int   nameswidth = 0;

	// Don't paint keys if in label only mode
	if (!label_only) {
		// No keycodes, paint nothing
		if (GetNumkeycodes() == 0) {
			return;
		}

		for (size_t k = 0; k < std::size(keycode); ++k) {
			if (keycode[k] && !hide[k]) {
				cscreen->PaintKeyName(x + 8 + nameswidth, y, keycode[k]);
			}
			nameswidth += namew[k] * 8;
		}

		font->paint_text_fixedwidth(ibuf, "[", x, y, 8, fontcolor.colors);
		font->paint_text_fixedwidth(ibuf, "]", x + nameswidth + 8, y, 8, fontcolor.colors);
	}

	int labelstart = (nameswidth ? 16 : 0) + nameswidth;
	int labelwidth = font->paint_text_fixedwidth(ibuf, get_label().c_str(), x + labelstart, y, 8, fontcolor.colors);
	for (size_t k = 0; k < std::size(keycode); ++k) {
		TileRect r = GetRect(k,false);

		if (keycode[k] && !hide[k]) {
			if (FixUppercaseKeycode(keycode[k]) == FixUppercaseKeycode(highlighted)) {
				ibuf->fill_translucent8(0, r.w, r.h, r.x, r.y, cscreen->highlighttable);
				// if (GetNumkeycodes() == 2) {
				ibuf->fill_translucent8(0, labelwidth, 8, x + labelstart, y, cscreen->highlighttable);
				//}
			}
			if (r.has_point(hoverx, hovery)) {
				ibuf->fill_translucent8(0, r.w, r.h, r.x, r.y, cscreen->hovertable);
				// if (GetNumkeycodes() == 2) {
				//ibuf->fill_translucent8(0, labelwidth, 8, x + labelstart, y, cscreen->hovertable);
				//}
			}
		}
	}
}


int CheatScreen::AddLeftRightMenuItem(
		int offsetx, int offsety, const char* label, bool left, bool right, bool leaveempty, bool fixedlabel) {
	// Change NPC

	int right_offset = (left || leaveempty) ? 8 : 0;
	int totalspace   = right_offset + ((right || leaveempty) ? 8 : 0);
	int xwidth       = right ? 8 : leaveempty ? 24 : 16;

	if (left) {
		PaintArrow(offsetx + 8, offsety, '<');
	}
	if (right) {
		PaintArrow(offsetx + 9 + right_offset, offsety, '>');
		hotspots.push_back(Hotspot(SDLK_RIGHT, offsetx + 9 + right_offset, offsety, 16, 8));
	}
	if (left) {
		hotspots.push_back(Hotspot(SDLK_LEFT, offsetx - 8, offsety, 16 + xwidth, 8));
	}

	font->paint_text_fixedwidth(ibuf, "[", offsetx, offsety, 8, fontcolor.colors);
	font->paint_text_fixedwidth(ibuf, "]", offsetx + totalspace + 8, offsety, 8, fontcolor.colors);
	int labelstart = (fixedlabel ? 32 : (totalspace + 16));
	int labelwidth = font->paint_text_fixedwidth(ibuf, label, offsetx + labelstart, offsety, 8, fontcolor.colors);

	if (state.highlight == SDLK_LEFT && left) {
		ibuf->fill_translucent8(0, 8 + xwidth, 8, offsetx, offsety, highlighttable);
	} else if (state.highlight == SDLK_RIGHT && right) {
		int extend = !left ? 16 : 0;
		ibuf->fill_translucent8(0, 16 + extend, 8, offsetx + 8 + right_offset - extend, offsety, highlighttable);
	}

	return labelstart + labelwidth;
}

void CheatScreen::EndFrame() {
	PaintHotspots();
	Mouse::mouse()->show();
	gwin->get_win()->show();
	Mouse::mouse()->hide();    // Must immediately hide to prevent flickering
}

void CheatScreen::WaitButtonsUp(bool silent) {
	Uint32     show_message = SDL_GetTicks() + 1000;
	ClearState clear(state);
	hotspots.clear();
	while (buttons_down.size()) {
		SharedInput();
		hotspots.clear();
		gwin->clear_screen();

		// exit if escape is pressed
		if (state.command == SDLK_ESCAPE) {
			// But first eat up events if there are any
			SharedInput();
			break;
		}

		if (!silent && show_message < SDL_GetTicks()) {
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
			const int offsetx_start = 15;

			// const int offsety1 = 73;
			// const int offsety2 = 55;
			const int offsetx1 = 160;
			// const int offsety4 = 36;
			const int offsety5 = 72;
#else
			const int offsetx_start = 0;
			// const int offsety1 = 0;
			// const int offsety2 = 0;
			const int offsetx1 = 160;
			// const int offsety4 = maxy - 45;
			const int offsety5 = maxy - 36;
#endif    // eXit

			int        offsetx       = offsetx_start;
			const char msg_waiting[] = "Waiting for up events: ";
			int        offsety       = 36;
			offsetx                  = offsetx1 - 4 * std::size(msg_waiting);
			offsetx += font->paint_text_fixedwidth(ibuf, msg_waiting, offsetx, offsety, 8, fontcolor.colors);

			bool first       = true;
			int  last_button = 0;
			for (int button : buttons_down) {
				char        buf[80]     = {0};
				const char* button_name = nullptr;

				// only each button once.. Standard guarantees that all keys
				// that compare equivalent are grouped together
				if (button == last_button) {
					continue;
				}
				last_button = button;

				switch (button) {
				case button_down_finger: {
					size_t num_fingers = buttons_down.count(button_down_finger);
					button_name        = buf;

					if (num_fingers > 1) {
						snprintf(buf, sizeof(buf), "%zu Fingers", num_fingers);
					} else {
						button_name = "Finger";
					}
				} break;

				case 1:
				case 2:
				case 3: {
					const char* button_names[] = {"Left Mouse Button", "Middle Mouse Button", "Right Mouse Button"};

					// Don't show mouse buttons if also waiting for finger
					// up
					if (buttons_down.find(button_down_finger) != buttons_down.end()) {
						continue;
					}

					button_name = button_names[button - 1];
				} break;

				default: {
					// It should be an SDL_Keycode
					const char* keyname = SDL_GetKeyName(SDL_Keycode(button));
					button_name         = buf;

					// Only display keyname if there is one and it is in ASCII
					if (keyname[0] && std::none_of(keyname, keyname + strlen(keyname), [](char c) {
							return static_cast<unsigned char>(c) >= 128;
						})) {
						snprintf(buf, sizeof(buf), "Key %s", keyname);
					} else {
						snprintf(buf, sizeof(buf), "Unknown Key #%i", button);
					}
				} break;
				}

				if (!first) {
					// Paint a comma at the end of the previous name
					offsetx += font->paint_text_fixedwidth(ibuf, ", ", offsetx, offsety, 8, fontcolor.colors);
				}
				first = false;

				// check if we have enough space on this line for the keyname
				// if not start a new line
				if (offsetx + strlen(button_name) * 8 > 312) {
					offsetx = offsetx_start;
					offsety += 9;
					// If we advance so many lines just break out now
					if (offsety >= (offsety5 + 9)) {
						break;
					}
				} else {
				}

				offsetx += font->paint_text_fixedwidth(ibuf, button_name, offsetx, offsety, 8, fontcolor.colors);
			}

			offsetx                = 0;
			const char msg_press[] = "Press";
			font->paint_text_fixedwidth(
					ibuf, msg_press, offsetx + 160 - 8 * std::size(msg_press), offsety5 + 9, 8, fontcolor.colors);

			AddMenuItem(offsetx + 160, offsety5 + 9, SDLK_ESCAPE, " to exit now");
		}

		EndFrame();
	}
}

CheatScreen::Menu::Menu(std::forward_list<std::pair<Hotspot, std::shared_ptr<MenuCommand>>>&& items) : items(std::move(items)) {
	// Set the hotspot for all our menu items
	for (auto& pair : this->items) {
		if (pair.second) {
			pair.second->hotspot = &pair.first;
			pair.second->owner   = this;
		}
	}

#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(CHEAT_SCREEN_TEST_MOBILE)
	const int offsetx = 15;
	// const int offsety1 = 73;
	// const int offsety2 = 55;
	// const int offsetx1 = 160;
	// const int offsety4 = 36;
	const int offsety5 = 72;
#else
	const int offsetx = 0;
	// const int offsety1 = 0;
	// const int offsety2 = 0;
	// const int offsetx1 = 160;
	// const int offsety4 = maxy - 45;
	const int offsety5 = cscreen->maxy - 36;
#endif    // eXit
		  // Add Escape hotspot
	this->items.emplace_front(Hotspot(offsetx + 160, offsety5 + 9, SDLK_ESCAPE, Strings::Exit()), nullptr);
	// Input handler to select Menu items
	inputs.emplace_back(std::make_unique<InputHandlers::KeyOnly>(Strings::ENTER_COMMAND));
}

std::shared_ptr<CheatScreen::MenuCommand> CheatScreen::Menu::Activate(SDL_Keycode keycode) {
	std::shared_ptr<MenuCommand> mc = nullptr;
	auto                         it = std::find_if(items.begin(), items.end(), [keycode](const auto& pair) {
        return pair.first.IsKeycode(keycode);
    });
	if (it != items.end()) {
		mc = it->second;
	} else {
		throw MenuCommandException{Strings::INVALID_COMMAND, false};
	}

	return mc;
}
