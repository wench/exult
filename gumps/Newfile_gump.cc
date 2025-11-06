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
#include "Settings.h"
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

	static auto Settings() {
		return get_text_msg(0x6D7 - msg_file_start);
	}

	static auto SaveGames() {
		return get_text_msg(0x6D8 - msg_file_start);
	}

	static auto AutosaveCount_() {
		return get_text_msg(0x6D9 - msg_file_start);
	}

	static auto QuicksaveCount_() {
		return get_text_msg(0x6DA - msg_file_start);
	}

	static auto SortSavegamesBy_() {
		return get_text_msg(0x6DB - msg_file_start);
	}

	static auto GroupByType_() {
		return get_text_msg(0x6DC - msg_file_start);
	}

	static auto AutosavesWriteToGamedat_() {
		return get_text_msg(0x6DD - msg_file_start);
	}

	static auto RealTime() {
		return get_text_msg(0x6DE - msg_file_start);
	}
	static auto Name() {
		return get_text_msg(0x6DF - msg_file_start);
	}

};

//
// Enable or disable a button.
//
void Newfile_gump::SetWidgetEnabled(widget_ids id, bool newenabled) {
	auto& source = newenabled ? disabled_widgets : widgets;
	auto& dest   = newenabled ? widgets : disabled_widgets;
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
		old_style_mode = Settings::get().disk.use_old_style_save_load;
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
		widgets[id_music] = std::make_unique<
				SelfManaged<CallbackToggleButton<Newfile_gump>>>(
				this, &Newfile_gump::toggle_audio_option, old_btn_cols[0],
				old_btn_rows[1], game->get_shape("gumps/musicbtn"),
				!audio ? 0 : audio->is_music_enabled(), 2, SF_GUMPS_VGA);
		widgets[id_speech] = std::make_unique<
				SelfManaged<CallbackToggleButton<Newfile_gump>>>(
				this, &Newfile_gump::toggle_audio_option, old_btn_cols[1],
				old_btn_rows[1], game->get_shape("gumps/speechbtn"),
				!audio ? 0 : audio->is_speech_enabled(), 2, SF_GUMPS_VGA);
		widgets[id_effects] = std::make_unique<
				SelfManaged<CallbackToggleButton<Newfile_gump>>>(
				this, &Newfile_gump::toggle_audio_option, old_btn_cols[2],
				old_btn_rows[1], game->get_shape("gumps/soundbtn"),
				!audio ? 0 : audio->are_effects_enabled(), 2, SF_GUMPS_VGA);

		// QUIT
		widgets[id_close] = std::make_unique<
				SelfManaged<CallbackTextButton<Newfile_gump>>>(
				this, &Newfile_gump::quit, Strings::QUIT(), old_btn_cols[2],
				old_btn_rows[0], 60);

		// Save
		widgets[id_save] = std::make_unique<
				SelfManaged<CallbackTextButton<Newfile_gump>>>(
				this, &Newfile_gump::save, Strings::SAVE(), old_btn_cols[1],
				old_btn_rows[0], 60);
		// Load
		widgets[id_load] = std::make_unique<
				SelfManaged<CallbackTextButton<Newfile_gump>>>(
				this, &Newfile_gump::load, Strings::LOAD(), old_btn_cols[0],
				old_btn_rows[0], 60);

		HorizontalArrangeWidgets(
				tcb::span(
						widgets.data() + id_normal_start,
						id_settings_start - id_normal_start),
				9);

		HorizontalArrangeWidgets(
				tcb::span(
						widgets.data() + id_old_style_start,
						id_old_style_last + 1 - id_old_style_start),
				9);

	} else {
		fieldcount = 14;
		SetProceduralBackground(TileRect(0, 0, 320, 200), -1, false);

		// Cancel
		widgets[id_close] = std::make_unique<
				SelfManaged<CallbackTextButton<Newfile_gump>>>(
				this, &Newfile_gump::close, Strings::CANCEL(), btn_cols[3],
				btn_rows[0]);
		// Save
		widgets[id_save] = std::make_unique<
				SelfManaged<CallbackTextButton<Newfile_gump>>>(
				this, &Newfile_gump::save, Strings::SAVE(), btn_cols[0],
				btn_rows[0]);
		// Load
		widgets[id_load] = std::make_unique<
				SelfManaged<CallbackTextButton<Newfile_gump>>>(
				this, &Newfile_gump::load, Strings::LOAD(), btn_cols[1],
				btn_rows[0]);
		// Delete
		widgets[id_delete] = std::make_unique<
				SelfManaged<CallbackTextButton<Newfile_gump>>>(
				this, &Newfile_gump::delete_file, Strings::DELETE(),
				btn_cols[2], btn_rows[0]);

		widgets[id_change_mode] = std::make_unique<
				SelfManaged<CallbackToggleTextButton<Newfile_gump>>>(
				this, &Newfile_gump::toggle_settings,
				std::vector<std::string>{
						Strings::Settings(), Strings::SaveGames()},
				0, btn_cols[3], btn_rows[0], 0, 0);

		HorizontalArrangeWidgets(
				tcb::span(
						widgets.data() + id_normal_start,
						id_settings_start - id_normal_start),
				9, 217);

		// Apply button uses same location as save button
		widgets[id_apply] = std::make_unique<
				SelfManaged<CallbackTextButton<Newfile_gump>>>(
				this, &Newfile_gump::apply_settings, Strings::APPLY(), 1,
				btn_rows[0]);

		// Revert button uses same location as load button
		widgets[id_revert] = std::make_unique<
				SelfManaged<CallbackTextButton<Newfile_gump>>>(
				this, &Newfile_gump::revert_settings, Strings::REVERT(), 2,
				btn_rows[0]);

		HorizontalArrangeWidgets(
				tcb::span(widgets.data() + id_apply, 2), 9,
				widgets[id_close]->get_x());

		// Settings widgets
		int yindex = 0;

		int amaxval = Settings::get().disk.autosave_count;
		if (amaxval < 100) {
			amaxval = 100;
		} else  {
			amaxval = std::max(1000,amaxval);
		}
		int qmaxval = Settings::get().disk.quicksave_count;
		if (qmaxval < 100) {
			qmaxval = 100;
		} else {
			qmaxval = std::max(1000, qmaxval);
		}

		int num_width = std::max(
				font->get_text_width(std::to_string(amaxval).c_str()),
				font->get_text_width(std::to_string(qmaxval).c_str()));

		widgets[id_slider_autocount] = std::make_unique<Slider_widget>(
				this, get_button_pos_for_label(Strings::AutosaveCount_()),
				yForRow(yindex++)-12, std::nullopt, std::nullopt, std::nullopt,
				Settings::get().disk.autosave_count.get_min(), amaxval, 1, Settings::get().disk.autosave_count, 64, font,
				num_width, true, false);

		
		widgets[id_slider_quickcount] = std::make_unique<Slider_widget>(
				this, get_button_pos_for_label(Strings::QuicksaveCount_()),
				yForRow(yindex++)-12, std::nullopt, std::nullopt, std::nullopt,
				Settings::get().disk.quicksave_count.get_min(),
				qmaxval, 1, Settings::get().disk.quicksave_count, 64, font,
				num_width, true, false);

		widgets[id_button_sortby] = std::make_unique<SelfManaged<Gump_ToggleTextButton>>(
						this,
						std::vector<std::string>{
								Strings::RealTime(), Strings::Name(),
								Strings::GameTime()},
				Settings::get().disk.savegame_sort_by,
				get_button_pos_for_label(Strings::SortSavegamesBy_()),
				yForRow(yindex++)-2,64,0);

		widgets[id_button_groupbytype] = std::make_unique<SelfManaged<Gump_ToggleTextButton>>(
			this,std::vector<std::string>{Strings::No(), Strings::Yes()},
				Settings::get().disk.savegame_group_by_type,
				get_button_pos_for_label(Strings::GroupByType_()),
						yForRow(yindex++)-2,64,0);

		widgets[id_button_autosaves_write_to_gamedat] = std::make_unique<SelfManaged<Gump_ToggleTextButton>>(
			this,std::vector<std::string>{Strings::No(), Strings::Yes()},
				Settings::get().disk.autosaves_write_to_gamedat,
				get_button_pos_for_label(Strings::AutosavesWriteToGamedat_()),
						yForRow(yindex++)-2,64,0);

		RightAlignWidgets(tcb::span(widgets.data() + id_slider_autocount, 5));
		//RightAlignWidgets(tcb::span(widgets.data() + id_button_sortbyname, 3),num_width+4);
	}

	// Reposition the gump
	auto area = get_usable_area();
	x         = (gwin->get_width() + area.x - area.w) / 2;
	y         = (gwin->get_height() + area.y - area.h) / 2;

	auto scroll = std::make_unique<Scrollable_widget>(
			this, fieldx, fieldy, fieldw,
			fieldcount * (fieldh + fieldgap) - fieldgap, 0,
			old_style_mode ? Scrollable_widget::ScrollbarType::None
						   : Scrollable_widget::ScrollbarType::Always,
			false, 0xff, 1);
	scroll->add_child(std::make_shared<Slot_widget>(this));
	scroll->set_line_height(fieldh + fieldgap, false);
	widgets[id_scroll] = std::move(scroll);

	LoadSaveGameDetails(false);
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
		gamedat->Extractgame(
				(*games)[selected_slot - SavegameSlots].filename().c_str(),
				!restore_mode);
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

	size_t save_num = selected_slot - SavegameSlots;

	// Already a game in this slot? If so ask to delete
	if (selected_slot >= SavegameSlots && games && games->size() > save_num
		&& !(*games)[save_num].savename.empty() && (*games)[save_num].details) {
		if (!Yesno_gump::ask("Okay to write over existing saved game?")) {
			return;
		}
	}

	const SaveInfo* info = nullptr;
	// Use actual savegame num if overwriting existing game
	if (games && games->size() > size_t(save_num)) {
		save_num = (*games)[save_num].num;
		info     = &((*games)[save_num]);
	}
	// if not old stylemode and saving to empty slot, use unspecified num
	else if (!old_style_mode) {
		save_num = -1;
	}

	// Now write to savegame file
	if (selected_slot >= SavegameSlots && selected_slot <= LastSlot()) {
		if (!info || info->filename().empty()) {
			gamedat->Savegame(newname);
		} else {
			gamedat->Savegame(info->filename().c_str(), newname);
		}
	} else if (selected_slot == EmptySlot) {
		gamedat->Savegame(newname);
	} else if (selected_slot == QuicksaveSlot) {
		gamedat->Quicksave();
	}

	cout << "Saved game #" << selected_slot << " successfully." << endl;

	// Reset everything
	FreeSaveGameDetails();
	LoadSaveGameDetails(false);
	gwin->set_all_dirty();
	gwin->got_bad_feeling(4);
	SelectSlot(NoSlot);
}

/*
 *  'Delete' clicked.
 */

void Newfile_gump::delete_file() {
	// Shouldn't ever happen.
	if (selected_slot < SavegameSlots || selected_slot > LastSlot()) {
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
	LoadSaveGameDetails(true);
	gwin->set_all_dirty();
	SelectSlot(NoSlot);
}

void Newfile_gump::toggle_settings(int state) {
	show_settings = (state != 0);

	if (!show_settings)
	{
		auto selected_before = selected_slot;
		FreeSaveGameDetails();
		GameDat::get()->ResortSaveInfos();
		LoadSaveGameDetails(false);
		SelectSlot(selected_before);
	}

	transition_start_time = SDL_GetTicks();
}

void Newfile_gump::apply_settings() {
auto &settings = Settings::get().disk;
	settings.autosave_count = widgets[id_slider_autocount]->getselection();
	settings.quicksave_count = widgets[id_slider_quickcount]->getselection();
	settings.savegame_sort_by = widgets[id_button_sortby]->getselection();
	settings.savegame_group_by_type
			= widgets[id_button_groupbytype]->getselection() != 0;
	settings.autosaves_write_to_gamedat = widgets[id_button_autosaves_write_to_gamedat]->getselection() != 0;

	
	
	settings.save_dirty(true);
}

void Newfile_gump::revert_settings() {
	auto& settings = Settings::get().disk;
	widgets[id_slider_autocount]->setselection(settings.autosave_count);
	widgets[id_slider_quickcount]->setselection(settings.quicksave_count);
	widgets[id_button_sortby]->setselection(
			settings.savegame_sort_by);
	widgets[id_button_groupbytype]->setselection( 
			settings.savegame_group_by_type);
	widgets[id_button_autosaves_write_to_gamedat]->setselection( 
			settings.autosaves_write_to_gamedat);

}

bool Newfile_gump::run() {
	bool need_repaint = Modal_gump::run() || transition_start_time != 0;

	if (transition_start_time != 0
		&& SDL_GetTicks() > transition_start_time + transition_duration) {
		transition_start_time = 0;
	}

	for (auto& btn : widgets) {
		if (btn) {
			need_repaint = btn->run() || need_repaint;
		}
	}

	return need_repaint;
}

void Newfile_gump::Slot_widget::paint() {
	int sx = 0, sy = 0;

	local_to_screen(sx, sy);
	auto ibuf = Shape_frame::get_to_render();

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
		else if (actual_slot == nfg->selected_slot && !nfg->widgets[id_load]) {
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
				&& actual_slot <= nfg->LastSlot()
				&& nfg->games->size() > size_t(actual_slot - SavegameSlots)) {
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
	auto ibuf = Shape_frame::get_to_render();

	if (old_style_mode || !show_settings || transition_start_time) {
		paint_normal();
	}
	if (old_style_mode || !(show_settings || transition_start_time)) {
		return;
	}
	
	//
	// create a barn door wipe using clipping rect to transition between normal
	// and settings
	//
	// The clipping rect controls how much of the settings to show
	//
	auto     clipsave = ibuf->SaveClip();
	TileRect newclip  = get_rect();

	if (transition_start_time) {
		Uint32 elapsed = std::min(
				SDL_GetTicks() - transition_start_time, transition_duration);
		int total_width = newclip.w;
		if (show_settings) {
			// Expanding
			newclip.w = (elapsed * total_width) / transition_duration;
		} else {
			// Contracting
			newclip.w = total_width
						- (elapsed * total_width) / transition_duration;
		}
		newclip.x = newclip.x + (total_width - newclip.w) / 2;
	}

	newclip = clipsave.Rect().intersect(newclip);
	ibuf->set_clip(newclip.x, newclip.y, newclip.w, newclip.h);
	paint_settings();
}

void Newfile_gump::paint_normal() {
	Modal_gump::paint();

	Image_window8* iwin = gwin->get_win();
	Image_buffer8* ibuf = iwin->get_ib8();

	// draw button backgrounds
	if (!old_style_mode) {
		ibuf->draw_box(x + 212, y + 3, 7, 25, 0, 145, 142);
		ibuf->draw_box(x + 212, y + 28, 7, 129, 0, 143, 142);
		ibuf->draw_box(x + 212, y + 157, 7, 38, 0, 145, 142);

		ibuf->draw_box(x , y + 188, get_usable_area().w , 7, 0, 145, 142);
	} else {
		ibuf->draw_box(x , y + 134, get_usable_area().w , 7, 0, 145, 142);
	}

	// Ensure change mode button always says Settings if savegames shown
	if (widgets[id_change_mode]) {
		widgets[id_change_mode]->setselection(0);
	}

	// Paint widgets after scroll background but first need to restore clip to
	// draw without transition
	for (int i = id_first; i < id_count; i++) {
		auto& btn = widgets[i];

		if ((i >= id_settings_start && i <= id_settings_last)
			|| (!old_style_mode && i >= id_old_style_start
				&& i <= id_old_style_last)) {
			// Skip settings buttons in normal paint
			continue;
		}

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
			shape.paint_shape(x + 249 + (i & 3) * 23, y + 157 + i / 4 * 27,true);
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
					ibuf, info, x + infox, y + infoy, infow, infoh, 0, false,
					false);
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

void Newfile_gump::paint_settings() {
	auto ibuf = Shape_frame::get_to_render();

	Modal_gump::paint();
	TileRect usable = get_usable_area();
	ibuf->draw_box(x , y + 188, usable.w , 7, 0, 145, 142);


	// Ensure change mode button always says SaveGames in if settings shown
	widgets[id_change_mode]->setselection(1);

	int sx = 0, sy = 0, y_index = 0;
	local_to_screen(sx, sy);



	// Paint all buttons
	for (int i = id_first; i < id_count; i++) {
		auto& widget = widgets[i];

		if ((i >= id_normal_start && i <= id_normal_last)
			|| (i >= id_old_style_start && i <= id_old_style_last)) {
			continue;
		}

		if (widget) {
			auto rect = widget->get_rect();
			//Slider_widget* slider = dynamic_cast<Slider_widget*>(widget.get());
			//if (slider) {
				
					//ibuf->draw_box(
						//	rect.x + 12, rect.y + 2, 64,
							//Slider_widget::Diamond::get_height_static(), 0, 143,
							//142);
				

			//}
			// Zebra striping
			if (i >= id_slider_autocount
				&& i <= id_button_autosaves_write_to_gamedat) {

				ibuf->draw_box(
						sx + usable.x , rect.y + 2, usable.w , rect.h - 4,
						0, 143, 142);
			}
			widget->paint();

		}
	}

	font->paint_text(
			ibuf, Strings::AutosaveCount_(), sx + label_margin,
			sy + yForRow(y_index));
	font->paint_text(
			ibuf, Strings::QuicksaveCount_(), sx + label_margin,
			sy + yForRow(++y_index));
	font->paint_text(
			ibuf, Strings::SortSavegamesBy_(), sx + label_margin,
			sy + yForRow(++y_index));
	font->paint_text(
			ibuf, Strings::GroupByType_(), sx + label_margin,
			sy + yForRow(++y_index));
	font->paint_text(
			ibuf, Strings::AutosavesWriteToGamedat_(),
			sx + label_margin, sy + yForRow(++y_index));
}

void Newfile_gump::quit() {
	if (gumpman->okay_to_quit()) {
		done = true;
	}
}

inline bool Newfile_gump::forward_input(
		std::function<bool(Gump_widget*)> func) {
	for (const auto& widget : widgets) {
		auto found = widget ? widget->Input_first() : nullptr;
		if (found && func(found)) {
			return true;
		}
	}

	bool do_normal   = !transition_start_time && !show_settings;
	bool do_settings = !transition_start_time && show_settings;

	for (int i = id_first; i < id_count; i++) {
		const auto& widget = widgets[i];
		// Skip widgets not in current mode
		if ((!do_normal && i >= id_normal_start && i <= id_normal_last)
			|| (!do_settings && i >= id_settings_start && i <= id_settings_last)
			|| (!old_style_mode && i >= id_old_style_start
				&& i <= id_old_style_last)) {
			continue;
		}
		if (widget && func(widget.get())) {
			return true;
		}
	}
	return false;
}

/*
 *  Handle mouse-down events.
 */

bool Newfile_gump::mouse_down(
		int mx, int my, MouseButton button    // Position in window.
) {
	if (forward_input([mx, my, button](Gump_widget* widget) {
			return widget->mouse_down(mx, my, button);
		})) {
		return true;
	}

	gwin->set_all_dirty();    // Request Repaint.

	return Modal_gump::mouse_down(mx, my, button);
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
	bool result = Modal_gump::mouse_up(mx, my, button)
				  || forward_input([mx, my, button](Gump_widget* widget) {
						 return widget->mouse_up(mx, my, button);
					 });

	if (touchui != nullptr
		&& ((selected_slot == EmptySlot && last_selected != InvalidSlot)
			|| (selected_slot >= 0 && selected_slot == last_selected))) {
		touchui->promptForName(newname);
		result |= true;
	}
	// reset so the prompt doesn't pop up on closing
	last_selected = InvalidSlot;

	return result;
}

bool Newfile_gump::mousewheel_up(int mx, int my) {
	return Modal_gump::mousewheel_up(mx, my)
		   || forward_input([mx, my](Gump_widget* widget) {
				  return widget->mousewheel_up(mx, my);
			  });
}

bool Newfile_gump::mousewheel_down(int mx, int my) {
	return Modal_gump::mousewheel_down(mx, my)
		   || forward_input([mx, my](Gump_widget* widget) {
				  return widget->mousewheel_down(mx, my);
			  });
}

/*
 *  Mouse was dragged with left button down.
 */

bool Newfile_gump::mouse_drag(
		int mx, int my    // Where mouse is.
) {
	return Modal_gump::mouse_drag(mx, my)
		   || forward_input([mx, my](Gump_widget* widget) {
				  return widget->mouse_drag(mx, my);
			  });
}

bool Newfile_gump::text_input(const char* text) {
	if (forward_input([text](Gump_widget* widget) {
			return widget->text_input(text);
		})) {
		return true;
	}
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
	SetWidgetEnabled(id_save, *newname != 0);

	// Remove Load and Delete Button
	SetWidgetEnabled(id_load, false);
	SetWidgetEnabled(id_delete, false);

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
	if (transition_start_time) {
		return false;
	}

	if (forward_input([chr, unicode](Gump_widget* widget) {
			return widget->key_down(chr, unicode);
		})) {
		return true;
	}

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
	case SDLK_RETURN: {
		auto load_button = dynamic_cast<Gump_button*>(widgets[id_load].get());
		auto save_button = dynamic_cast<Gump_button*>(widgets[id_save].get());
		// If only 'Save', do it.
		if (!load_button && save_button) {
			if (save_button->push(MouseButton::Left)) {
				gwin->show(true);
				save_button->unpush(MouseButton::Left);
				gwin->show(true);
				save_button->activate(MouseButton::Left);
			}
		}    // If only 'Load', do it.
		if (load_button && !save_button) {
			if (load_button->push(MouseButton::Left)) {
				gwin->show(true);
				load_button->unpush(MouseButton::Left);
				gwin->show(true);
				load_button->activate(MouseButton::Left);
			}
		}
		update_details = true;
		break;
	}

	case SDLK_BACKSPACE:
		if (BackspacePressed()) {
			// Can't restore/delete now.
			SetWidgetEnabled(id_load, false);
			SetWidgetEnabled(id_delete, false);
			// If no chars cant save either
			SetWidgetEnabled(id_save, *newname != 0);
			update_details = true;
		}
		break;

	case SDLK_DELETE:
		if (DeletePressed()) {
			// Can't restore/delete now.
			SetWidgetEnabled(id_load, false);
			SetWidgetEnabled(id_delete, false);
			// If no chars cant save either
			SetWidgetEnabled(id_save, *newname != 0);

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
				SetWidgetEnabled(id_load, false);
				SetWidgetEnabled(id_delete, false);
				// If no chars cant save either
				SetWidgetEnabled(id_save, *newname != 0);

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
	text[cursor] = c;
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
	selected_slot         = slot;
	bool   want_load      = true;
	bool   want_delete    = true;
	bool   want_save      = true;
	size_t savegame_index = selected_slot - SavegameSlots;

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
	} else if (
			selected_slot >= SavegameSlots && selected_slot < NumSlots()
			&& games && games->size() > savegame_index) {
		screenshot = (*games)[savegame_index].screenshot.get();
		details    = &((*games)[savegame_index].details);
		party      = &((*games)[savegame_index].party);
		strcpy(newname, (*games)[savegame_index].savename.c_str());
		cursor      = static_cast<int>(strlen(newname));
		is_readable = want_load = (*games)[savegame_index].readable;
		filename                = (*games)[savegame_index].filename().c_str();
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

	SetWidgetEnabled(id_load, want_load);
	SetWidgetEnabled(id_save, want_save);
	SetWidgetEnabled(id_delete, want_delete);

	gwin->set_all_dirty();    // Repaint.
}

void Newfile_gump::LoadSaveGameDetails(bool force) {
	// Gamedat Details
	if (!gd_shot || !gd_details || gd_party.empty()) {
		// Only if any are missing
		gamedat->get_saveinfo(gd_shot, gd_details, gd_party, false);
	}

	if (!restore_mode) {
		// Only if any are missing
		if (!cur_shot || !cur_details || cur_party.empty()) {
			gamedat->get_saveinfo(cur_shot, cur_details, cur_party, true);
		}
	}
	if (!old_style_mode) {
		games = gamedat->GetSaveGameInfos(force);
	} else {
		// Fill old_games slots
		old_games.clear();
		old_games.resize(fieldcount);
		for (const auto& savegame : *gamedat->GetSaveGameInfos(force)) {
			if (savegame.num >= 0 && savegame.num < fieldcount) {
				old_games[savegame.num] = SaveInfo(savegame, {});
			}
		}

		// Fill in any missing old_games slots
		for (int i = 0; i < fieldcount; ++i) {
			if (old_games[i].num != i) {
				old_games[i] = SaveInfo(
						gamedat->get_save_filename(i, SaveInfo::Type::REGULAR));
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
	if (widgets[id_scroll]) {
		widgets[id_scroll]->run();
	}
}

void Newfile_gump::FreeSaveGameDetails() {
	//cur_shot.reset();
	//cur_details = SaveGame_Details();
	//cur_party.clear();

	//gd_shot.reset();
	//gd_details = SaveGame_Details();
	//gd_party.clear();


	filename = nullptr;
	details  = nullptr;
	party    = nullptr;
	screenshot = nullptr;

	// The SaveInfo struct will delete everything that it's got allocated
	// So we don't need to worry about that
	games = nullptr;
}

void Newfile_gump::toggle_audio_option(Gump_widget* btn, int state) {
	auto audio = Audio::get_ptr();
	if (btn == widgets[id_music].get()) {    // Music?
		if (audio) {
			audio->set_music_enabled(state);
			if (!state) {    // Stop what's playing.
				audio->stop_music();
			}
		}
		const string s = state ? "yes" : "no";
		// Write option out.
		config->set("config/audio/midi/enabled", s, true);
	} else if (btn == widgets[id_speech].get()) {    // Speech?
		if (audio) {
			audio->set_speech_enabled(state);
			if (!state) {
				audio->stop_speech();
			}
		}
		const string s = state ? "yes" : "no";
		// Write option out.
		config->set("config/audio/speech/enabled", s, true);
	} else if (btn == widgets[id_effects].get()) {    // Sound effects?
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
