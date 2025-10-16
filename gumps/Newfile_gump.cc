/*
 *  Copyright (C) 2001-2025  The Exult Team
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

#include "Newfile_gump.h"

#include "Audio.h"
#include "Configuration.h"
#include "Gump_ToggleButton.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "Slider_widget.h"
#include "Text_button.h"
#include "Yesno_gump.h"
#include "actors.h"
#include "exult.h"
#include "game.h"
#include "gameclk.h"
#include "gamewin.h"
#include "items.h"
#include "listfiles.h"
#include "misc_buttons.h"
#include "miscinf.h"
#include "mouse.h"
#include "party.h"
#include "touchui.h"

#include <cctype>
#include <cstring>
#include <ctime>

using std::atoi;
using std::cout;
using std::endl;
using std::isdigit;
using std::localtime;
using std::qsort;
using std::string;
using std::strncpy;
using std::time;
using std::time_t;
using std::tm;

class Strings : public GumpStrings {
public:
	static auto Avatar() {
		return get_text_msg(0x660 - msg_file_start);
	}

	static auto Exp() {
		return get_text_msg(0x661 - msg_file_start);
	}

	static auto Hp() {
		return get_text_msg(0x662 - msg_file_start);
	}

	static auto Str() {
		return get_text_msg(0x663 - msg_file_start);
	}

	static auto Dxt() {
		return get_text_msg(0x664 - msg_file_start);
	}

	static auto Int() {
		return get_text_msg(0x665 - msg_file_start);
	}

	static auto Trn() {
		return get_text_msg(0x666 - msg_file_start);
	}

	static auto GameDay() {
		return get_text_msg(0x667 - msg_file_start);
	}

	static auto GameTime() {
		return get_text_msg(0x668 - msg_file_start);
	}

	static auto SaveCount() {
		return get_text_msg(0x669 - msg_file_start);
	}

	static auto Date() {
		return get_text_msg(0x66A - msg_file_start);
	}

	static auto Time() {
		return get_text_msg(0x66B - msg_file_start);
	}

	static auto File() {
		return get_text_msg(0x66C - msg_file_start);
	}

	static auto LOAD() {
		return get_text_msg(0x66D - msg_file_start);
	}

	static auto SAVE() {
		return get_text_msg(0x66E - msg_file_start);
	}
#ifdef DELETE
#	undef DELETE
#endif
	static auto DELETE() {
		return get_text_msg(0x66F - msg_file_start);
	}

	static auto month_Abbreviation(int month) {
		if (month < 0 || month > 11) {
			return "";
		}

		return get_text_msg(0x670 - msg_file_start + month);
	}

	static auto ordinal_numeral_suffix(int num) {
		if ((num % 10) == 1 && num != 11) {
			return get_text_msg(0x67C - msg_file_start);    // st
		} else if ((num % 10) == 2 && num != 12) {
			return get_text_msg(0x67D - msg_file_start);    // nd
		} else if ((num % 10) == 3 && num != 13) {
			return get_text_msg(0x67E - msg_file_start);    // rd
		}
		return get_text_msg(0x67F - msg_file_start);    // th
	}

	static auto No_Screenshot() {
		static std::string rw;
		if (rw.empty()) {
			rw = get_text_msg(0x6D2 - msg_file_start);

			// Replace all ~ with \n
			for (char& c : rw) {
				if (c == '~') {
					c = '\n';
				}
			}
		}
		return rw.c_str();
	}

	static auto QUIT() {
		return get_text_msg(0x6D3 - msg_file_start);
	}

	static auto GamedatDirectory() {
		return get_text_msg(0x6D4 - msg_file_start);
	}

	static auto NewQuickSave() {
		return get_text_msg(0x6D5 - msg_file_start);
	}

	static auto EmptySlot() {
		return get_text_msg(0x6D6 - msg_file_start);
	}

	static auto Unreadable_Savegame() {
		static std::string rw;
		if (rw.empty()) {
			rw = get_text_msg(0x6D0 - msg_file_start);

			// Replace all ~ with \n
			for (char& c : rw) {
				if (c == '~') {
					c = '\n';
				}
			}
		}
		return rw.c_str();
	}

	static auto NoInfo() {
		return get_text_msg(0x6D1 - msg_file_start);
	}
};

//
// Enable or disable a button.
//
void Newfile_gump::SetButtonEnabled(button_ids id, bool newenabled) {
	auto& source = newenabled ? disabled_buttons : buttons;
	auto& dest   = newenabled ? buttons : disabled_buttons;
	// Only move to dest if source exists
	// Don't want to overwrite an existing button in dest with a nullptr
	if (source[id]) {
		dest[id] = std::move(source[id]);
		source[id].reset();
	}
}

/*
 *  Create the load/save box.
 */

Newfile_gump::Newfile_gump(bool restore_mode_, bool old_style_mode_)
		: Modal_gump(
				  nullptr, gwin->get_width() / 2 - 160,
				  gwin->get_height() / 2 - 100, -1),
		  restore_mode(restore_mode_), old_style_mode(old_style_mode_),
		  tinyfont(fontManager.get_font("TINY_BLACK_FONT")) {
	if (restore_mode) {
		old_style_mode = false;
	} else if (!old_style_mode) {
		// Only check for old style mode config setting if not in restore mode
		// and old_style_mode has not been forced by constructor parameter
		config->value(
				"config/disk/use_old_style_save_load", old_style_mode, false);
		config->set(
				"config/disk/use_old_style_save_load",
				old_style_mode ? "yes" : "no", true);
	}

	newname[0] = 0;

	gwin->get_tqueue()->pause(SDL_GetTicks());
	back = gwin->get_win()->create_buffer(
			gwin->get_width(), gwin->get_height());
	gwin->get_win()->get(back.get(), 0, 0);

	if (old_style_mode) {
		fieldcount = 10;
		// Old style mode has 1 pixel offset hence origin at 1,1
		SetProceduralBackground(TileRect(1, 1, 210, 156), -1, false);

		Audio* audio = Audio::get_ptr();
		// Toggle Buttons
		buttons[id_music]
				= std::make_unique<CallbackToggleButton<Newfile_gump>>(
						this, &Newfile_gump::toggle_audio_option,
						old_btn_cols[0], old_btn_rows[1],
						game->get_shape("gumps/musicbtn"),
						!audio ? 0 : audio->is_music_enabled(), 2,
						SF_GUMPS_VGA);
		buttons[id_speech]
				= std::make_unique<CallbackToggleButton<Newfile_gump>>(
						this, &Newfile_gump::toggle_audio_option,
						old_btn_cols[1], old_btn_rows[1],
						game->get_shape("gumps/speechbtn"),
						!audio ? 0 : audio->is_speech_enabled(), 2,
						SF_GUMPS_VGA);
		buttons[id_effects]
				= std::make_unique<CallbackToggleButton<Newfile_gump>>(
						this, &Newfile_gump::toggle_audio_option,
						old_btn_cols[2], old_btn_rows[1],
						game->get_shape("gumps/soundbtn"),
						!audio ? 0 : audio->are_effects_enabled(), 2,
						SF_GUMPS_VGA);

		// QUIT
		buttons[id_close] = std::make_unique<CallbackTextButton<Newfile_gump>>(
				this, &Newfile_gump::quit, Strings::QUIT(), old_btn_cols[2],
				old_btn_rows[0], 60);

		// Save
		buttons[id_save] = std::make_unique<CallbackTextButton<Newfile_gump>>(
				this, &Newfile_gump::save, Strings::SAVE(), old_btn_cols[1],
				old_btn_rows[0], 60);
		// Load
		buttons[id_load] = std::make_unique<CallbackTextButton<Newfile_gump>>(
				this, &Newfile_gump::load, Strings::LOAD(), old_btn_cols[0],
				old_btn_rows[0], 60);

		HorizontalArrangeWidgets(tcb::span(buttons.data() + id_load, 4), 9);

		HorizontalArrangeWidgets(tcb::span(buttons.data() + id_music, 3), 9);

	} else {
		fieldcount = 14;
		SetProceduralBackground(TileRect(0, 0, 320, 200), -1, false);

		// Cancel
		buttons[id_close] = std::make_unique<CallbackTextButton<Newfile_gump>>(
				this, &Newfile_gump::close, Strings::CANCEL(), btn_cols[3],
				btn_rows[0]);
		// Save
		buttons[id_save] = std::make_unique<CallbackTextButton<Newfile_gump>>(
				this, &Newfile_gump::save, Strings::SAVE(), btn_cols[0],
				btn_rows[0]);
		// Load
		buttons[id_load] = std::make_unique<CallbackTextButton<Newfile_gump>>(
				this, &Newfile_gump::load, Strings::LOAD(), btn_cols[1],
				btn_rows[0]);
		// Delete
		buttons[id_delete] = std::make_unique<CallbackTextButton<Newfile_gump>>(
				this, &Newfile_gump::delete_file, Strings::DELETE(),
				btn_cols[2], btn_rows[0]);

		HorizontalArrangeWidgets(
				tcb::span(buttons.data() + id_load, 4), 9, 217);
	}

	// Reposition the gump
	auto area = get_usable_area();
	x         = (gwin->get_width() + area.x - area.w) / 2;
	y         = (gwin->get_height() + area.y - area.h) / 2;

	scroll = std::make_unique<Scrollable_widget>(
			this, fieldx, fieldy, fieldw,
			fieldcount * (fieldh + fieldgap) - fieldgap, 0,
			old_style_mode ? Scrollable_widget::ScrollbarType::None
						   : Scrollable_widget::ScrollbarType::Always,
			false, 0xff, 1);
	scroll->add_child(std::make_shared<Slot_widget>(this));
	scroll->set_line_height(fieldh + fieldgap, false);

	LoadSaveGameDetails();
	SelectSlot(NoSlot);
	if (touchui != nullptr) {
		touchui->hideGameControls();
		touchui->hideButtonControls();
	}
}

/*
 *  Delete the load/save box.
 */

Newfile_gump::~Newfile_gump() {
	gwin->get_tqueue()->resume(SDL_GetTicks());
	FreeSaveGameDetails();
	if (touchui != nullptr) {
		touchui->showButtonControls();
		if (!gumpman->gump_mode()
			|| (!gumpman->modal_gump_mode()
				&& gumpman->gumps_dont_pause_game())) {
			touchui->showGameControls();
		}
	}
}

/*
 *  'Load' clicked.
 */

void Newfile_gump::load() {
	// Shouldn't ever happen.
	if (selected_slot < GamedatSlot) {
		return;
	}

	// Aborts if unsuccessful.
	if (selected_slot >= SavegameSlots && selected_slot <= LastSlot()) {
		gwin->restore_gamedat((*games)[selected_slot - SavegameSlots].num);
	}

	// Read Gamedat if not in restore mode
	if (!restore_mode) {
		gwin->read();
	}

	// Set Done
	done     = true;
	restored = true;

	// Reset Selection
	SelectSlot(NoSlot);
}

/*
 *  'Save' clicked.
 */

void Newfile_gump::save() {
	// Shouldn't ever happen.
	if ((!strlen(newname) && selected_slot != QuicksaveSlot)
		|| selected_slot < FirstSlot() || restore_mode) {
		return;
	}

	// Already a game in this slot? If so ask to delete
	if (selected_slot >= SavegameSlots) {
		if (!Yesno_gump::ask("Okay to write over existing saved game?")) {
			return;
		}
	}

	// Write to gamedat
	gwin->write();

	// Now write to savegame file
	if (selected_slot >= SavegameSlots && selected_slot <= LastSlot()) {
		gwin->save_gamedat(
				(*games)[selected_slot - SavegameSlots].num, newname);
	} else if (selected_slot == EmptySlot) {
		gwin->save_gamedat(newname, SaveInfo::REGULAR);
	} else if (selected_slot == QuicksaveSlot) {
		gwin->save_gamedat("", SaveInfo::QUICKSAVE);
	}

	cout << "Saved game #" << selected_slot << " successfully." << endl;

	// Reset everything
	FreeSaveGameDetails();
	LoadSaveGameDetails();
	gwin->set_all_dirty();
	gwin->got_bad_feeling(4);
	SelectSlot(NoSlot);
}

/*
 *  'Delete' clicked.
 */

void Newfile_gump::delete_file() {
	// Shouldn't ever happen.
	if (selected_slot <= SavegameSlots || selected_slot > LastSlot()) {
		return;
	}

	// Ask to delete
	if (!Yesno_gump::ask("Okay to delete saved game?")) {
		return;
	}

	U7remove((*games)[selected_slot].filename().c_str());
	filename    = nullptr;
	is_readable = false;

	cout << "Deleted Save game #" << selected_slot << " ("
		 << (*games)[selected_slot].filename() << ") successfully." << endl;

	// Reset everything
	FreeSaveGameDetails();
	LoadSaveGameDetails();
	gwin->set_all_dirty();
	SelectSlot(NoSlot);
}

void Newfile_gump::Slot_widget::paint() {
	int sx = 0, sy = 0;

	local_to_screen(sx, sy);
	Image_window8* iwin = gwin->get_win();
	Image_buffer8* ibuf = iwin->get_ib8();

	auto     clipsave = ibuf->SaveClip();
	TileRect newclip  = clipsave.Rect().intersect(get_rect());
	ibuf->set_clip(newclip.x, newclip.y, newclip.w, newclip.h);

	// Start at the first field that would be visible
	for (int i = std::max(0, (newclip.y - sy) / (fieldh + fieldgap));
		 i < nfg->NumSlots(); i++) {
		const int actual_slot = nfg->FirstSlot() + i;
		const int fx          = sx;
		const int fy          = sy + i * (fieldh + fieldgap);
		// if field y is beyond end of clip break out as no more fields will be
		// visible
		if (fy > (newclip.y + newclip.h)) {
			break;
		}

		TileRect fieldrect(fx, fy, fieldw, fieldh);

		// If field not visible, skip it
		if (!newclip.intersects(fieldrect)) {
			continue;
		}
		// Always paint the field background
		ibuf->draw_beveled_box(
				fx, fy, fieldw, fieldh, 1, 137, 144, 144, 140, 140, 142);

		const char* text;

		if (actual_slot == QuicksaveSlot) {
			text = Strings::NewQuickSave();
		} else if (actual_slot == GamedatSlot) {
			text = Strings::GamedatDirectory();
		}
		// The Slot being drawn is selected for saving
		else if (actual_slot == nfg->selected_slot && !nfg->buttons[id_load]) {
			text = nfg->newname;
		} else if (actual_slot == EmptySlot) {
			if (nfg->restore_mode) {
				text = "";
			} else {
				text = Strings::EmptySlot();
			}
			// the slot being drawn is a savegame
		} else if (
				nfg->games && actual_slot >= SavegameSlots
				&& actual_slot <= nfg->LastSlot()) {
			text = (*nfg->games)[actual_slot - SavegameSlots].savename.c_str();
		} else {
			text = "";
		}

		sman->paint_text(2, text, fx + textx, fy + texty);

		// Being Edited? If so paint cursor
		if (nfg->selected_slot == actual_slot && nfg->cursor != -1) {
			gwin->get_win()->fill8(
					0, 1, sman->get_text_height(2),
					fx + textx + sman->get_text_width(2, text, nfg->cursor),
					fy + texty);
		}

		// If selected, show selected icon
		if (nfg->selected_slot == actual_slot) {
			int ix = iconx + fx;
			int iy = icony + fy;

			for (int l = 0; l < 4; l++) {
				ibuf->draw_line8(142, ix + l, iy + l, ix + l, iy + 6 - l);
			}

			ibuf->draw_line8(146, ix + 1, iy + 7, ix + 4, iy + 4);
			ibuf->draw_line8(146, ix + 1, iy + 6, ix + 3, iy + 4);
		}
	}
}

/*
 *  Paint on screen.
 */

void Newfile_gump::paint() {
	Modal_gump::paint();

	Image_window8* iwin = gwin->get_win();
	Image_buffer8* ibuf = iwin->get_ib8();

	auto r = buttons[id_close]->get_rect();
	// draw slider and button backgrounds
	if (!old_style_mode) {
		ibuf->draw_box(x + 212, y + 3, 7, 25, 0, 145, 142);
		ibuf->draw_box(x + 212, y + 28, 7, 129, 0, 143, 142);
		ibuf->draw_box(x + 212, y + 157, 7, 38, 0, 145, 142);

		int bbglimit = r.x + r.w - x;
		if (bbglimit > 219) {
			bbglimit = get_usable_area().w - 3;
		} else {
			bbglimit = 219;
		}

		ibuf->draw_box(x + 3, y + 188, bbglimit - 3, 7, 0, 145, 142);
	} else {
		ibuf->draw_box(x + 3, y + 134, get_usable_area().w - 4, 7, 0, 145, 142);
	}

	// Paint scroll widget, that paints the fields
	scroll->paint();

	// Paint Buttons
	for (const auto& btn : buttons) {
		if (btn) {
			btn->paint();
		}
	}

	// If in old style mode, don't paint the savegame details
	if (old_style_mode) {
		return;
	}

	// Paint the savegame details

	if (screenshot) {
		sman->paint_shape(x + 222, y + 2, screenshot->get_frame(0));
	} else {
		// Paint No Screenshot background
		ibuf->draw_box(x + 222, y + 2, 96, 60, 0, 143, 142);

		auto msg = Strings::No_Screenshot();
		int  tw, th;
		font->get_text_box_dims(msg, tw, th);

		font->paint_text_box(
				ibuf, msg, x + 270 - tw / 2, y + 30 - th / 2, tw, th, 0, false,
				true);
	}
	// Draw details background
	ibuf->draw_beveled_box(
			x + 222, y + 63, 96, 68, 1, 137, 144, 145, 140, 139, 142);

	if (details && party && party->size()) {
		for (int i = party->size() - 1; i >= 0; --i) {
			int  shape_num  = (*party)[i].shape;
			auto shape_file = ShapeFile((*party)[i].shape_file);

			// Need to ensure that the avatar's shape actually exists
			if (i == 0 && !sman->have_si_shapes()
				&& Shapeinfo_lookup::IsSkinImported(shape_num)
				&& shape_file == SF_SHAPES_VGA) {
				// Female if odd, male if even
				if (shape_num % 2) {
					shape_num = Shapeinfo_lookup::GetFemaleAvShape();
				} else {
					shape_num = Shapeinfo_lookup::GetMaleAvShape();
				}
			}
			ShapeID shape(shape_num, 16, shape_file);
			shape.paint_shape(x + 249 + (i & 3) * 23, y + 157 + i / 4 * 27);
		}

		char info[320];

		snprintf(
				info, std::size(info),
				"%s: %s\n"
				"%s: %i  %s: %i\n"
				"%s: %i  %s %i\n"
				"%s: %i  %s: %i\n"
				"\n"
				"%s: %i\n"
				"%s: %02i:%02i\n"
				"\n"
				"%s: %i\n"
				"%s: %i%s %s %04i\n"
				"%s: %02i:%02i",

				Strings::Avatar(), party->front().name, Strings::Exp(),
				party->front().exp, Strings::Hp(), party->front().health,
				Strings::Str(), party->front().str, Strings::Dxt(),
				party->front().dext, Strings::Int(), party->front().intel,
				Strings::Trn(), party->front().training, Strings::GameDay(),
				details->game_day, Strings::GameTime(), details->game_hour,
				details->game_minute, Strings::SaveCount(), details->save_count,
				Strings::Date(), details->real_day,
				Strings::ordinal_numeral_suffix(details->real_day),
				Strings::month_Abbreviation(details->real_month - 1),
				details->real_year, Strings::Time(), details->real_hour,
				details->real_minute);
		info[std::size(info) - 1] = 0;

		if (filename) {
			size_t cursize = strlen(info);

			if (cursize < std::size(info) - 2) {
				auto             svfilename = get_filename_from_path(filename);
				std::string_view svfile     = Strings::File();
				info[cursize++]             = '\n';

				cursize += svfile.copy(
						info + cursize,
						std::min(std::size(info) - cursize, svfile.size()));

				info[cursize++] = ':';

				if (cursize < std::size(info)) {
					cursize += svfilename.copy(
							info + cursize, std::min(
													std::size(info) - cursize,
													svfilename.size()));
				}
				info[std::min(cursize, std::size(info) - 1)] = 0;
			}
		}

		// Draw text lines manually  so we can control line spacing
		int   th   = tinyfont->get_text_height();
		int   ty   = y + infoy;
		int   tx   = x + infox;
		char* text = info;
		while (text && *text) {
			char* eol = std::strchr(text, '\n');
			if (eol) {
				*eol = 0;
			}

			tinyfont->draw_text(ibuf, tx, ty, text);

			//
			// Emptylines with only a linebreak are only 2 pixels high
			while (eol && *++eol == '\n') {
				ty += 2;
			}

			text = eol;
			ty += th;
		}
	} else {
		if (filename) {
			char info[64] = {0};

			size_t cursize = 0;

			if (cursize < std::size(info) - 2) {
				auto             svfilename = get_filename_from_path(filename);
				std::string_view svfile     = Strings::File();
				info[cursize++]             = '\n';

				cursize += svfile.copy(
						info + cursize,
						std::min(std::size(info) - cursize, svfile.size()));

				info[cursize++] = ':';

				if (cursize < std::size(info)) {
					cursize += svfilename.copy(
							info + cursize, std::min(
													std::size(info) - cursize,
													svfilename.size()));
				}
				info[std::min(cursize, std::size(info) - 1)] = 0;
			}
			tinyfont->paint_text_box(
					ibuf, info, x + infox, y + infoy, infow, infoh,0,false,false);
		}

		if (!is_readable && !(restore_mode && selected_slot == NoSlot)) {
			font->paint_text_box(
					ibuf, Strings::Unreadable_Savegame(), x + infox,

					y + infoy + (infoh - 18) / 2, infow, infoh, 0, false, true);
		} else {
			tinyfont->paint_text_box(
					ibuf, Strings::NoInfo(), x + infox,

					y + infoy + (infoh - 18) / 2, infow, infoh, 0, false, true);
		}
	}
}

void Newfile_gump::quit() {
	if (gumpman->okay_to_quit()) {
		done = true;
	}
}

/*
 *  Handle mouse-down events.
 */

bool Newfile_gump::mouse_down(
		int mx, int my, MouseButton button    // Position in window.
) {

	pushed = Gump::on_button(mx, my);
	// Try buttons at bottom.
	if (!pushed) {
		for (const auto& btn : buttons) {
			if (btn && btn->on_button(mx, my)) {
				pushed = btn.get();
				break;
			}
		}
	}

	if (pushed) {    // On a button?
		if (!pushed->push(button)) {
			pushed = nullptr;
		}
		return true;
	}

	gwin->set_all_dirty();    // Repaint.
	return scroll->mouse_down(mx, my, button)
		   || Modal_gump::mouse_down(mx, my, button);
	// See if on text field.
}

bool Newfile_gump::Slot_widget::mouse_down(int mx, int my, MouseButton button) {
	if (button != MouseButton::Left) {
		return false;
	}
	screen_to_local(mx, my);

	if (mx < 0 || mx >= get_width()) {
		return false;
	}
	if (my < 0 || my >= get_height()) {
		return false;
	}
	// Which field was hit?
	int hit = my / (fieldh + fieldgap);
	int fy  = hit * (fieldh + fieldgap);
	// Make sure it was an actual hit
	if (my >= fy && my < (fy + fieldh)) {
#ifdef DEBUG
		cout << "Hit a save game field" << endl;
#endif
		nfg->SelectSlot(hit + nfg->FirstSlot());
		return true;
	}

	return false;
}

/*
 *  Handle mouse-up events.
 */

bool Newfile_gump::mouse_up(
		int mx, int my, MouseButton button    // Position in window.
) {


	bool result = scroll->mouse_up(mx, my, button);

	if (pushed) {    // Pushing a button?
		pushed->unpush(button);
		if (pushed->on_button(mx, my)) {
			pushed->activate(button);
		}
		pushed = nullptr;
		result |= true;
	}
	if (touchui != nullptr
		&& ((selected_slot == EmptySlot && last_selected != InvalidSlot)
			|| (selected_slot >= 0 && selected_slot == last_selected))) {
		touchui->promptForName(newname);
		result |= true;
	}
	// reset so the prompt doesn't pop up on closing
	last_selected = InvalidSlot;

	return result || Modal_gump::mouse_up(mx, my, button);
}

bool Newfile_gump::mousewheel_up(int mx, int my) {
	return scroll->mousewheel_up(mx, my)
		   || Modal_gump::mousewheel_up(mx, my);
}


bool Newfile_gump::mousewheel_down(int mx, int my) {
	return scroll->mousewheel_down(mx, my)
		   || Modal_gump::mousewheel_down(mx, my);
}

/*
 *  Mouse was dragged with left button down.
 */

bool Newfile_gump::mouse_drag(
		int mx, int my    // Where mouse is.
) {
	return scroll->mouse_drag(mx, my) || Modal_gump::mouse_drag(mx, my);
}

bool Newfile_gump::text_input(const char* text) {
	if (restore_mode) {
		return true;
	}

	if (cursor == -1 || strlen(text) >= MAX_SAVEGAME_NAME_LEN - 1) {
		return true;
	}
	// if (strcmp(text, newname) == 0) {    // Not changed
	// return true;
	//}

	strcpy(newname, text);
	cursor = static_cast<int>(strlen(text));

	// Show save button if newname is not empty
	SetButtonEnabled(id_save, *newname != 0);

	// Remove Load and Delete Button
	SetButtonEnabled(id_load, false);
	SetButtonEnabled(id_delete, false);

	screenshot = cur_shot.get();
	details    = &cur_details;
	party      = &cur_party;

	gwin->set_all_dirty();
	return true;
}

/*
 *  Handle character that was typed.
 */

bool Newfile_gump::key_down(SDL_Keycode chr, SDL_Keycode unicode) {
	bool update_details = false;
	int  repaint        = false;

	// Are we selected on some text?
	if (selected_slot == NoSlot) {
		return false;
	}

	if (restore_mode && chr != SDLK_RETURN) {
		return false;
	}

	switch (chr) {
	case SDLK_RETURN:    // If only 'Save', do it.
		if (!buttons[id_load] && buttons[id_save]) {
			if (buttons[id_save]->push(MouseButton::Left)) {
				gwin->show(true);
				buttons[id_save]->unpush(MouseButton::Left);
				gwin->show(true);
				buttons[id_save]->activate(MouseButton::Left);
			}
		}    // If only 'Load', do it.
		if (buttons[id_load] && !buttons[id_save]) {
			if (buttons[id_load]->push(MouseButton::Left)) {
				gwin->show(true);
				buttons[id_load]->unpush(MouseButton::Left);
				gwin->show(true);
				buttons[id_load]->activate(MouseButton::Left);
			}
		}
		update_details = true;
		break;

	case SDLK_BACKSPACE:
		if (BackspacePressed()) {
			// Can't restore/delete now.
			SetButtonEnabled(id_load, false);
			SetButtonEnabled(id_delete, false);
			// If no chars cant save either
			SetButtonEnabled(id_save, *newname != 0);
			update_details = true;
		}
		break;

	case SDLK_DELETE:
		if (DeletePressed()) {
			// Can't restore/delete now.
			SetButtonEnabled(id_load, false);
			SetButtonEnabled(id_delete, false);
			// If no chars cant save either
			SetButtonEnabled(id_save, *newname != 0);

			repaint        = true;
			update_details = true;
		}
		break;

	case SDLK_LEFT:
		repaint = MoveCursor(-1);
		break;

	case SDLK_RIGHT:
		repaint = MoveCursor(1);
		break;

	case SDLK_HOME:
		repaint = MoveCursor(-MAX_SAVEGAME_NAME_LEN);
		break;

	case SDLK_END:
		repaint = MoveCursor(MAX_SAVEGAME_NAME_LEN);
		break;

	default:
		if (unicode < ' ') {
			return Modal_gump::key_down(
					chr, unicode);    // Ignore other special chars and let
									  // parent class handle them
		}

		if (unicode < 256 && isascii(unicode)) {
			if (AddCharacter(unicode)) {
				// Can't restore/delete now.
				SetButtonEnabled(id_load, false);
				SetButtonEnabled(id_delete, false);
				// If no chars cant save either
				SetButtonEnabled(id_save, *newname != 0);

				repaint        = true;
				update_details = true;
			}
		}
		break;
	}

	// This sets the game details to the cur set
	if (update_details) {
		screenshot = cur_shot.get();
		details    = &cur_details;
		party      = &cur_party;
		repaint    = true;
	}
	if (repaint) {
		gwin->set_all_dirty();
	}
	return true;
}

int Newfile_gump::BackspacePressed() {
	if (cursor == -1 || cursor == 0) {
		return 0;
	}
	cursor--;
	return DeletePressed();
}

int Newfile_gump::DeletePressed() {
	if (cursor == -1 || cursor == static_cast<int>(strlen(newname))) {
		return 0;
	}
	for (unsigned i = cursor; i < strlen(newname); i++) {
		newname[i] = newname[i + 1];
	}

	return 1;
}

int Newfile_gump::MoveCursor(int count) {
	if (cursor == -1) {
		return 0;
	}

	cursor += count;
	if (cursor < 0) {
		cursor = 0;
	}
	if (cursor > static_cast<int>(strlen(newname))) {
		cursor = strlen(newname);
	}

	return 1;
}

int Newfile_gump::AddCharacter(char c) {
	if (cursor == -1 || cursor >= MAX_SAVEGAME_NAME_LEN - 1) {
		return 0;
	}

	char text[MAX_SAVEGAME_NAME_LEN];

	strncpy(text, newname, cursor);
	text[cursor]     = c;
	strncpy(text + cursor + 1, newname + cursor,
			MAX_SAVEGAME_NAME_LEN - cursor - 1);
	text[MAX_SAVEGAME_NAME_LEN - 1] = 0;

	// Now check the width of the text
	if (font->get_text_width(text) >= textw) {
		return 0;
	}

	cursor++;
	strcpy(newname, text);
	return 1;
}

void Newfile_gump::SelectSlot(int slot) {
	// Out of range slots are treated as NoSlot
	if (slot < NoSlot || slot > LastSlot()) {
		slot = NoSlot;
	}
	selected_slot    = slot;
	bool want_load   = true;
	bool want_delete = true;
	bool want_save   = true;

	if (selected_slot == EmptySlot) {
		want_load   = false;
		want_delete = false;
		want_save   = false;
		screenshot  = cur_shot.get();
		details     = &cur_details;
		party       = &cur_party;
		newname[0]  = 0;
		cursor      = 0;
		is_readable = true;
		filename    = nullptr;
	} else if (selected_slot == GamedatSlot) {
		want_delete = false;
		screenshot  = gd_shot.get();
		details     = &gd_details;
		party       = &gd_party;
		strcpy(newname, Strings::GamedatDirectory());
		cursor      = -1;    // No cursor
		is_readable = true;
		filename    = nullptr;
	} else if (selected_slot == QuicksaveSlot) {
		want_delete = false;
		want_load   = false;
		screenshot  = cur_shot.get();
		details     = &cur_details;
		party       = &cur_party;
		strcpy(newname, Strings::NewQuickSave());
		cursor      = -1;    // No cursor
		is_readable = true;
		filename    = nullptr;
	} else if (selected_slot >= SavegameSlots && selected_slot < NumSlots()) {
		screenshot = (*games)[selected_slot].screenshot.get();
		details    = &((*games)[selected_slot].details);
		party      = &((*games)[selected_slot].party);
		strcpy(newname, (*games)[selected_slot].savename.c_str());
		cursor      = static_cast<int>(strlen(newname));
		is_readable = want_load = (*games)[selected_slot].readable;
		filename                = (*games)[selected_slot].filename().c_str();
	} else {
		// No slot Selected
		want_load   = false;
		want_delete = false;
		want_save   = false;
		// Show details of current game
		screenshot  = cur_shot.get();
		details     = &cur_details;
		party       = &cur_party;
		newname[0]  = 0;
		cursor      = 0;
		is_readable = false;
		filename    = nullptr;
	}

	if (restore_mode) {
		// No cursor in restore mode because there is no saving so names can't
		// be changed
		cursor    = -1;
		want_save = false;
	}

	SetButtonEnabled(id_load, want_load);
	SetButtonEnabled(id_save, want_save);
	SetButtonEnabled(id_delete, want_delete);

	gwin->set_all_dirty();    // Repaint.
}

void Newfile_gump::LoadSaveGameDetails() {
	// Gamedat Details
	gwin->get_saveinfo(gd_shot, gd_details, gd_party);

	if (!restore_mode) {
		// Current screenshot
		cur_shot = gwin->create_mini_screenshot();

		// Current Details
		cur_details = SaveGame_Details();

		gwin->get_win()->put(back.get(), 0, 0);

		if (gd_details) {
			cur_details.save_count = gd_details.save_count;
		} else {
			cur_details.save_count = 0;
		}

		cur_details.game_day
				= static_cast<short>(gclock->get_total_hours() / 24);
		cur_details.game_hour   = gclock->get_hour();
		cur_details.game_minute = gclock->get_minute();

		const time_t t        = time(nullptr);
		tm*          timeinfo = localtime(&t);

		cur_details.real_day    = timeinfo->tm_mday;
		cur_details.real_hour   = timeinfo->tm_hour;
		cur_details.real_minute = timeinfo->tm_min;
		cur_details.real_month  = timeinfo->tm_mon + 1;
		cur_details.real_year   = timeinfo->tm_year + 1900;
		cur_details.real_second = timeinfo->tm_sec;
		cur_details.good        = true;
		// Current Party
		cur_party.clear();
		cur_party.reserve(partyman->get_count() + 1);

		for (auto npc : partyman->IterateWithMainActor) {
			auto&       current = cur_party.emplace_back();
			std::string namestr = npc->get_npc_name();
			std::strncpy(
					current.name, namestr.c_str(), std::size(current.name) - 1);
			current.name[std::size(current.name) - 1] = 0;
			current.shape                             = npc->get_shapenum();
			current.shape_file                        = npc->get_shapefile();

			current.dext     = npc->get_property(Actor::dexterity);
			current.str      = npc->get_property(Actor::strength);
			current.intel    = npc->get_property(Actor::intelligence);
			current.health   = npc->get_property(Actor::health);
			current.combat   = npc->get_property(Actor::combat);
			current.mana     = npc->get_property(Actor::mana);
			current.magic    = npc->get_property(Actor::magic);
			current.training = npc->get_property(Actor::training);
			current.exp      = npc->get_property(Actor::exp);
			current.food     = npc->get_property(Actor::food_level);
			current.flags    = npc->get_flags();
			current.flags2   = npc->get_flags2();
		}

		party      = &cur_party;
		screenshot = cur_shot.get();
		details    = &cur_details;
	}
	if (!old_style_mode) {
		games = &gwin->GetSaveGameInfos();
	} else {
		// Fill old_games slots
		old_games.clear();
		old_games.resize(fieldcount);
		for (const auto& savegame : gwin->GetSaveGameInfos()) {
			if (savegame.num >= 0 && savegame.num < fieldcount) {
				old_games[savegame.num] = SaveInfo(savegame, {});
			}
		}

		// Fill in any missing old_games slots
		for (int i = 0; i < fieldcount; ++i) {
			if (old_games[i].num != i) {
				old_games[i] = SaveInfo(
						gwin->get_save_filename(i, SaveInfo::REGULAR));
			}
		}

		games = &old_games;
	}

	// We'll now output the info if debugging
#ifdef DEBUG
	cout << "Listing " << (games ? games->size() : 0) << " Save games" << endl;
	for (size_t i = 0; i < (games ? games->size() : 0); i++) {
		cout << i << " = " << (*games)[i].num << " : " << (*games)[i].filename()
			 << " : " << (*games)[i].savename << endl;
	}
#endif
	scroll->run();
}

void Newfile_gump::FreeSaveGameDetails() {
	cur_shot.reset();
	cur_details = SaveGame_Details();
	cur_party.clear();

	gd_shot.reset();
	gd_details = SaveGame_Details();
	gd_party.clear();

	filename = nullptr;
	details  = nullptr;
	party    = nullptr;

	// The SaveInfo struct will delete everything that it's got allocated
	// So we don't need to worry about that
	games = nullptr;
}

void Newfile_gump::toggle_audio_option(Gump_widget* btn, int state) {
	auto audio = Audio::get_ptr();
	if (btn == buttons[id_music].get()) {    // Music?
		if (audio) {
			audio->set_music_enabled(state);
			if (!state) {    // Stop what's playing.
				audio->stop_music();
			}
		}
		const string s = state ? "yes" : "no";
		// Write option out.
		config->set("config/audio/midi/enabled", s, true);
	} else if (btn == buttons[id_speech].get()) {    // Speech?
		if (audio) {
			audio->set_speech_enabled(state);
			if (!state) {
				audio->stop_speech();
			}
		}
		const string s = state ? "yes" : "no";
		// Write option out.
		config->set("config/audio/speech/enabled", s, true);
	} else if (btn == buttons[id_effects].get()) {    // Sound effects?
		if (audio) {
			audio->set_effects_enabled(state);
			if (!state) {    // Off?  Stop what's playing.
				audio->stop_sound_effects();
			}
		}
		const string s = state ? "yes" : "no";
		// Write option out.
		config->set("config/audio/effects/enabled", s, true);
	}
}
