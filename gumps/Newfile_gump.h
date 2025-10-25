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

#ifndef NEWFILE_GUMP_H
#define NEWFILE_GUMP_H

#include "Modal_gump.h"
#include "SaveInfo.h"

#include <array>
#include <memory>

class Shape_file;
class Image_buffer;

/*
 *  The file save/load box:
 */
class Newfile_gump : public Modal_gump {
public:
protected:
	enum button_ids {
		id_first = 0,
		id_load  = id_first,
		id_save,
		id_delete,
		id_close,
		id_page_up,
		id_line_up,
		id_line_down,
		id_page_down,
		id_count
	};

	std::array<std::unique_ptr<Gump_button>, id_count> buttons;

	static const short btn_cols[5];    // x-coord of each button.
	static const short btn_rows[5];    // y-coord of each button.

	// Text field info
	static const short fieldx;        // Start Y of each field
	static const short fieldy;        // Start X of first
	static const short fieldw;        // Width of each field
	static const short fieldh;        // Height of each field
	static const short fieldgap;      // Gap between fields
	static const short fieldcount;    // Number of fields
	static const short textx;         // X Offset in field
	static const short texty;         // Y Offset in field
	static const short textw;         // Maximum allowable width of text
	static const short iconx;         // X Offset in field
	static const short icony;         // Y Offset in field

	// Scrollbar and Slider Info
	static const short scrollx;    // X Offset
	static const short scrolly;    // Y Offset
	static const short scrollh;    // Height of Scroll Bar
	static const short sliderw;    // Width of Slider
	static const short sliderh;    // Height of Slider

	// Side Text
	static const short infox;    // X Offset for info
	static const short infoy;    // Y Offset for info
	static const short infow;    // Width of info box
	static const short infoh;    // Height of info box

	unsigned char restored = 0;    // Set to 1 if we restored a game.

	std::unique_ptr<Image_buffer> back;

	std::vector<SaveInfo> games;    // The list of savegames
	int first_free = 0;             // The number of the first free savegame

	std::unique_ptr<Shape_file> cur_shot;    // Screenshot for current game
	std::unique_ptr<SaveGame_Details> cur_details;    // Details of current game
	std::unique_ptr<SaveGame_Party[]> cur_party;      // Party of current game

	// Gamedat is being used as a 'quicksave'
	int last_selected = -4;    // keeping track of the selected line for iOS
	std::unique_ptr<Shape_file>       gd_shot;       // Screenshot in Gamedat
	std::unique_ptr<SaveGame_Details> gd_details;    // Details in Gamedat
	std::unique_ptr<SaveGame_Party[]> gd_party;      // Party in Gamedat

	Shape_file*       screenshot  = nullptr;    // The picture to be drawn
	SaveGame_Details* details     = nullptr;    // The game details to show
	SaveGame_Party*   party       = nullptr;    // The party to show
	bool              is_readable = false;      // Is the save game readable
	const char* filename = nullptr;    // Filename of the savegame, if exists

	int  list_position = -2;    // The position in the savegame list (top game)
	int  selected = -3;    // The savegame that has been selected (num in list)
	int  cursor   = 0;     // The position of the cursor
	int  slide_start = -1;                  // Pixel (v) where a slide started
	char newname[MAX_SAVEGAME_NAME_LEN];    // The new name for the game

	// Run in reduced functionality restore mode. Can only load and delete
	// games. Loading games only Restores Gamedat
	bool restore_mode;

	int BackspacePressed();
	int DeletePressed();
	int MoveCursor(int count);
	int AddCharacter(char c);

	void LoadSaveGameDetails();    // Loads (and sorts) all the savegame details
	void FreeSaveGameDetails();    // Frees all the savegame details

	void PaintSaveName(int line);

public:
	// Construct a Newfile_gump. Optionally using reduced functionality restore
	// mode
	Newfile_gump(bool restore_mode = false);
	~Newfile_gump() override;

	void load();           // 'Load' was clicked.
	void save();           // 'Save' was clicked.
	void delete_file();    // 'Delete' was clicked.

	void scroll_line(int dir);    // Scroll Line Button Pressed
	void scroll_page(int dir);    // Scroll Page Button Pressed.

	void line_up() {
		scroll_line(-1);
	}

	void line_down() {
		scroll_line(1);
	}

	void page_up() {
		scroll_page(-1);
	}

	void page_down() {
		scroll_page(1);
	}

	int restored_game() {    // 1 if user restored.
		return restored;
	}

	// Paint it and its contents.
	void paint() override;

	void close() override {
		done = true;
	}

	// Handle events:
	bool text_input(const char* text) override;    // Character typed.
	bool mouse_down(int mx, int my, MouseButton button) override;
	bool mouse_up(int mx, int my, MouseButton button) override;
	bool mouse_drag(int mx, int my) override;
	bool key_down(SDL_Keycode chr, SDL_Keycode unicode)
			override;    // Character typed.

	bool mousewheel_up(int mx, int my) override;
	bool mousewheel_down(int mx, int my) override;
};

#endif    // NEWFILE_GUMP_H
