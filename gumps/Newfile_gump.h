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

#ifndef NEWFILE_GUMP_H
#define NEWFILE_GUMP_H

#include "Modal_gump.h"
#include "Scrollable_widget.h"
#include "gamedat.h"

#include <array>
#include <functional>
#include <memory>

class Shape_file;
class Image_buffer;
class PageTurnEffect;

/*
 *  The file save/load box:
 */
class Newfile_gump : public Modal_gump {
public:

	using SaveInfo = GameDat::SaveInfo;
	using SaveGame_Details = GameDat::SaveGame_Details;
	using SaveGame_Party   = GameDat::SaveGame_Party;

protected:
	enum widget_ids {
		id_first = 0,
		id_scroll       = id_first,
		id_load,
		id_save,
		id_delete,
		id_close,
		id_change_mode,
		
		
		// Settings widgets start here
		id_apply,
		id_revert,
		id_slider_autocount,
		id_slider_quickcount,
		id_button_sortby,
		id_button_groupbytype,
		id_button_autosaves_write_to_gamedat,

		// Old style mode buttons start here
		id_music,
		id_speech,
		id_effects,

		id_count,

		id_normal_start = id_scroll,
		id_normal_last = id_delete,
		id_settings_start = id_apply,
		id_settings_last    = id_button_autosaves_write_to_gamedat,
		id_old_style_start = id_music,
		id_old_style_last   = id_effects,

	};

	std::array<std::unique_ptr<Gump_widget>, id_count> widgets;
	std::array<std::unique_ptr<Gump_widget>, id_count> disabled_widgets;

	// Enable or disable a button
	void SetWidgetEnabled(widget_ids id, bool newenabled);
	// Button Coords
	constexpr static std::array<short, 1> btn_rows = {
			186,
	};
	constexpr static std::array<short, 4> btn_cols = {2, 46, 88, 150};
	constexpr static std::array<short, 2> old_btn_rows{132, 155};
	constexpr static std::array<short, 3> old_btn_cols{95, 164, 233};

	// Text field info
	constexpr static short fieldx   = 2;      // Start Y of each field
	constexpr static short fieldy   = 2;      // Start X of first
	constexpr static short fieldw   = 208;    // Width of each field
	constexpr static short fieldh   = 12;     // Height of each field
	constexpr static short fieldgap = 1;      // Gap between fields
	constexpr static short textx    = 12;     // X Offset in field
	constexpr static short texty    = 2;      // Y Offset in field
	constexpr static short textw
			= fieldw - 17;    // Maximum allowable width of text (pixels)
	constexpr static short iconx = 2;    // X Offset in field
	constexpr static short icony = 2;    // Y Offset in field

	constexpr static short scrollx = 2, scrolly = 2;

	// Info box info
	constexpr static short infox = 224;
	constexpr static short infoy = 65;
	constexpr static short infow = 92;
	constexpr static short infoh = 79;

	bool restored = false;    // Set true if we restored a game.

	enum SaveSlots {
		InvalidSlot   = -5,
		NoSlot        = -4,
		EmptySlot     = -3,
		QuicksaveSlot = -2,
		GamedatSlot   = -1,
		SavegameSlots = 0
	};

	std::unique_ptr<Image_buffer> back;

	const std::vector<SaveInfo>* games
			= nullptr;                  // The list of savegames being shown
	std::vector<SaveInfo> old_games;    // Backing storage for the  list of
										// savegames in old_style_mode

	std::unique_ptr<Shape_file> cur_shot;       // Screenshot for current game
	SaveGame_Details            cur_details;    // Details of current game
	std::vector<SaveGame_Party> cur_party;      // Party of current game

	// Gamedat is being used as a 'quicksave'
	int last_selected
			= InvalidSlot;    // keeping track of the selected line for iOS
	std::unique_ptr<Shape_file> gd_shot;       // Screenshot in Gamedat
	SaveGame_Details            gd_details;    // Details in Gamedat
	std::vector<SaveGame_Party> gd_party;      // Party in Gamedat

	const Shape_file*       screenshot = nullptr;    // The picture to be drawn
	const SaveGame_Details* details    = nullptr;    // The game details to show
	const std::vector<SaveGame_Party>* party = nullptr;    // The party to show
	bool        is_readable = false;      // Is the save game readable
	const char* filename    = nullptr;    // Filename of the savegame, if exists

	int fieldcount;    // Number of slots being shown by the gump: 14 in normal
					   // or restore mode, 10 in olde_style_mode
	int selected_slot
			= NoSlot;    // The savegame slot that has been selected
	int  cursor = -1;    // The position of the cursor ( -1 is no cursor)
	char newname[MAX_SAVEGAME_NAME_LEN];    // The new name for the game

	// Run in reduced functionality restore mode. Can only load and delete
	// games. Loading games only Restores Gamedat
	// Can not be combined with old_style_mode. Restore mode takes Presedence
	bool restore_mode;

	// Old style mode replicates the functionality of the old File_gump and
	// how it works in the original games There are 10 save slots only. Each
	// slot can be edited. There is no scrolling or deleting. There  are
	// Quit, Sound, Speech and Music Buttons.
	// Cannot be combined with restore_mode

	const uint64_t                  transition_duration = 500;
	bool                            show_settings       = false;
	std::unique_ptr<PageTurnEffect> page_turn_effect;

	bool                  old_style_mode;
	std::shared_ptr<Font> tinyfont;

	int  BackspacePressed();
	int  DeletePressed();
	int  MoveCursor(int count);
	int  AddCharacter(char c);
	void SelectSlot(int slot);    // Select a given slot

	void LoadSaveGameDetails();    // Loads (and sorts) all the savegame details
	void FreeSaveGameDetails();    // Frees all the savegame details

public:
	// Construct a Newfile_gump. Optionally using reduced functionality restore
	// mode or old style mode. Can not use both modes at once. restore_mode
	// takes Presedence
	Newfile_gump(bool restore_mode = false, bool old_style_mode = false);
	~Newfile_gump() override;

	// Handle one of the toggles.
	void toggle_audio_option(Gump_widget* btn, int state);
	void load();           // 'Load' was clicked.
	void save();           // 'Save' was clicked.
	void delete_file();    // 'Delete' was clicked.
	void toggle_settings(int state);    // 'Settings' was clicked.
	void apply_settings();              // 'Apply' was clicked.
	void revert_settings();             // 'Revert' was clicked.
	bool run() override;    // Run the gump.

	// Get the first slot
	sint64 FirstSlot() const {
		if (restore_mode) {
			return GamedatSlot;
		}
		if (!old_style_mode) {
			return EmptySlot;
		} else {
			return SavegameSlots;
		}
	}

	// Get the last slot
	sint64 LastSlot() const {
		return std::max<sint64>(
				fieldcount - (SavegameSlots - FirstSlot()),
				SavegameSlots + (games ? games->size()  : 0));
	}

	// Get the total nuber of slots
	sint64 NumSlots() const {
		return LastSlot()  - FirstSlot();
	}

	bool restored_game() {    // true if user restored.
		return restored;
	}

	// Paint it and its contents.
	void paint() override;

	void paint_normal();
	void paint_settings();
	void close() override {
		done = true;
	}

	void quit();

	// Handle events:
	bool text_input(const char* text) override;    // Character typed.
	bool mouse_down(int mx, int my, MouseButton button) override;
	bool mouse_up(int mx, int my, MouseButton button) override;
	bool mouse_drag(int mx, int my) override;
	bool key_down(SDL_Keycode chr, SDL_Keycode unicode)
			override;    // Character typed.


	bool mousewheel_up(int mx, int my) override;
	bool mousewheel_down(int mx, int my) override;

private:
	bool forward_input(std::function<bool(Gump_widget*)>);

	class Slot_widget : public Gump_widget {
		Newfile_gump* nfg;

	public:
		Slot_widget(Newfile_gump* nfg)
				: Gump_widget(nfg, 0, 0, 0, -1, SF_OTHER), nfg(nfg) {}

		bool mouse_down(int mx, int my, MouseButton button) override;

		TileRect get_rect() const override {
			TileRect ret(
					0, 0, fieldw,
					(fieldh + fieldgap) * nfg->NumSlots() - fieldgap);
			local_to_screen(ret.x, ret.y);
			return ret;
		}

		void paint() override;
	};


};

#endif    // NEWFILE_GUMP_H
