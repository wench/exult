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

#ifndef CHEAT_SCREEN_H
#define CHEAT_SCREEN_H

#include "imagebuf.h"
#include "palette.h"
#include "rect.h"
#include "singles.h"
#include "span.h"

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

#include <any>
#include <array>
#include <climits>
#include <forward_list>
#include <functional>
#include <memory>
#include <stack>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

class Game_window;
class Image_buffer8;
class Font;
class Game_clock;
class Game_object;
class Actor;

// #define CHEAT_SCREEN_TEST_MOBILE 1

class CheatScreen : protected Game_singletons {
	static CheatScreen* cscreen;
	Actor*              grabbed = nullptr;

	std::vector<std::string> global_flag_names;
	bool                     global_flag_names_loaded = false;
	void                     load_global_flag_names();

public:
	CheatScreen() : highlighttable(), hovertable(), buttons_down() {}

	void show_screen();

	void SetGrabbedActor(Actor* g) {
		grabbed = g;
	}

	void ClearThisGrabbedActor(Actor* g) const {
		if (g == grabbed) {
			g = nullptr;
		}
	}

	void clear_buttons() {
		buttons_down.clear();
	}

	// When thrown Causes RunMenu to show the message if there is one and optionally leave the current MenuCommand returning to the
	// last Menu
	struct MenuCommandException {
		std::string msg = "";    // Message to display
		bool        return_to_menu
				= true;    // if true leave the current MenuCommand returning to previous menu, if false retry current input phase
	};

private:
	enum Cheat_Prompt {
		CP_Command = 0,

		CP_HitKey         = 1,
		CP_NotAvail       = 2,
		CP_InvalidNPC     = 3,
		CP_InvalidCom     = 4,
		CP_Canceled       = 5,
		CP_ClockSet       = 6,
		CP_InvalidTime    = 7,
		CP_InvalidShape   = 8,
		CP_InvalidValue   = 9,
		CP_Created        = 10,
		CP_ShapeSet       = 11,
		CP_ValueSet       = 12,
		CP_NameSet        = 13,
		CP_WrongShapeFile = 14,

		CP_ChooseNPC  = 16,
		CP_EnterValue = 17,
		CP_Minute     = 18,
		CP_Hour       = 19,
		CP_Day        = 20,
		CP_Shape      = 21,
		CP_Activity   = 22,
		CP_XCoord     = 23,
		CP_YCoord     = 24,
		CP_Lift       = 25,
		CP_GFlagNum   = 26,
		CP_NFlagNum   = 27,
		CP_TempNum    = 28,
		CP_NLatitude  = 29,
		CP_SLatitude  = 30,
		CP_WLongitude = 31,
		CP_ELongitude = 32,

		CP_Name       = 33,
		CP_NorthSouth = 34,
		CP_WestEast   = 35,

		CP_HexXCoord          = 37,
		CP_HexYCoord          = 38,
		CP_EnterValueNoCancel = 39,
		CP_CustomValue
	};

	// Get the 3 charcter key name for a character
	static const char* getKeyName(SDL_Keycode keycode);

	// Hotspot for Mouse/Touch input
	// Base TileRect can be changed after construction but keycode and label cannot be changed
	class Hotspot {
		SDL_Keycode keycode[2] = {0};
		int         namew[2]   = {0};    // Name Width of each keycode
		bool        hide[2]    = {false, false};

	public:
		const std::string label      = {};       // This is drawn to the right of above
		bool              label_only = false;    // Don't hit check keycodes. Only draw the label
		std::string       label_rw   = {};       // if this is set it is used instead of label
		int               x, y, w = 0, h = 0;

		const std::string& get_label() const {
			return label_rw.empty() ? label : label_rw;
		}

		// Paint the Hotspot
		void Paint(SDL_Keycode hilighted, int hoverx, int hovery) const;

		Hotspot(int x, int y, SDL_Keycode keycode, std::string&& label, SDL_Keycode keycode2 = SDLK_UNKNOWN);

		Hotspot(SDL_Keycode keycode, int x, int y, int w, int h) : x(x), y(y), w(w), h(h) {
			this->keycode[0] = keycode;
			this->keycode[1] = 1;
		}

		TileRect ToTR() const {
			return TileRect(x, y, w, h);
		}

		void setKeycode(SDL_Keycode code, unsigned index) {
			if (index >= std::size(keycode)) {
				return;
			}
			keycode[index] = code;
			namew[index]   = GetKeyNameWidth(code);
		}

		void setHidden(unsigned index, bool hidden) {
			if (index >= std::size(keycode)) {
				return;
			}
			hide[index] = hidden;
		}

		static int GetKeyNameWidth(SDL_Keycode keycode) {
			switch (keycode) {
				// Arrows are drawn 1 char wide
			case SDLK_UP:
			case SDLK_DOWN:
			case SDLK_LEFT:
			case SDLK_RIGHT:
				return 1;
				break;
			default:
				break;
			}
			return strlen(getKeyName(keycode));
		}

		TileRect GetRect(unsigned key, bool inclabel) const {
			if (key > 1 || !keycode[key] || hide[key]) {
				return {};
			}
			TileRect r(x, y, 0, 8);
			if (key == 0) {
				r.w = 8 + 8 * namew[0] + (namew[1] ? 0 : 8);
			} else {
				r.x += 8 + 8 * namew[0];
				r.w = 8 + 8 * namew[1];
			}

			// Include label if only 1 keycode
			if (inclabel && !keycode[1 - key]) {
				r.w += 8 * get_label().size();
			}

			return r;
		}

		unsigned GetNumkeycodes() const {
			unsigned num = 0;
			for (size_t i = 0; i < std::size(keycode); i++) {
				if (keycode[i] && !hide[i]) {
					num++;
				}
			}
			return num;
		}

		int getWidth() {
			if (label_only) {
				return get_label().size() * 8;
			}

			auto rect0 = GetRect(0, true);
			auto rect1 = GetRect(1, true);

			if (!rect0) {
				return rect1.w;
			} else if (!rect1) {
				return rect0.w;
			}

			if (rect1.intersects(rect0)) {
				return rect1.add(rect0).w;
			}

			return std::max(rect0.w, rect1.w);
		}

		SDL_Keycode HitCheck(int x, int y) const {
			if (label_only) {
				return SDLK_UNKNOWN;
			}

			if (keycode[0] && !hide[0] && GetRect(0, false).has_point(x, y)) {
				return FixUppercaseKeycode(keycode[0]);
			} else if (keycode[1] && !hide[1] && GetRect(1, false).has_point(x, y)) {
				return FixUppercaseKeycode(keycode[1]);
			}

			return SDLK_UNKNOWN;
		}

		static SDL_Keycode HitCheck(const std::vector<Hotspot*>& hotspots, int mx, int my, int radius = 4);

		bool IsKeycode(SDL_Keycode code, unsigned mask = 0xFFFFFFFF) const {
			if (!label_only) {
				code = FixUppercaseKeycode(code);
				for (unsigned i = 0; i < std::size(keycode); i++) {
					if (!hide[i] && (mask & 1 << i) && FixUppercaseKeycode(keycode[i]) == code) {
						return true;
					}
				}
			}
			return false;
		}

		// if the code is an uppercase letter make it lowercase
		static SDL_Keycode FixUppercaseKeycode(SDL_Keycode code) {
			return code >= 'A' && code <= 'Z' ? std::tolower(code) : code;
		}
	};

	std::vector<Hotspot> hotspots;

	class InputHandler {
	protected:
		// Actual text input
		char   input[17]     = {0};
		size_t curlen        = 0;        // the current lendth of the input string
		bool   empty_allowed = false;    // if false it is an error if the user doesn't input anything and presses enter
		bool   was_empty     = false;    // set if empty_allowed and there was no input

		std::string promptmsg;
		bool        cancellable = true;

	public:
		virtual ~InputHandler() {}

		// Hotspots are automatically placed to the right of the Prompt Text
		std::vector<Hotspot> hotspots;

		void SetPromptMessage(std::string&& promptmsg) {
			this->promptmsg = std::move(promptmsg);
		}

		virtual void GetPromptMessage(char* buf, size_t buf_size) {
			snprintf(buf, buf_size, "%s", promptmsg.c_str());
		}    // Handle Keypress. Returns teue if input finished

		virtual bool OnInput(SDL_Keycode key_sym) = 0;
		// Parse the input, throws InputException on error
		virtual void Parse();

		// Arranges the hotspots
		virtual void ArrangeHotspots(int x, int y, unsigned lines = 1);
		// Paint The Prompt and Input, return pixel width drawn
		virtual int PaintPrompt(int x, int y, SDL_Keycode lastkey);

		virtual bool input_full() {
			return curlen >= std::size(input) - 1;
		}

		// Get raw input buffer and its size
		const char* getInputRAW(size_t& len) {
			len = curlen;
			return input;
		}

		// Get input as null terminated string
		const char* getInput() {
			// Null terminate it just to be sure
			input[curlen] = 0;
			return input;
		}

		virtual void clear() {
			curlen = 0;
			memset(input, 0, sizeof(input));
			was_empty = false;
		}

		InputHandler(bool empty_allowed, std::string&& promptmsg) : empty_allowed(empty_allowed), promptmsg(std::move(promptmsg)) {}

		InputHandler(bool empty_allowed, std::string&& promptmsg, std::vector<Hotspot>&& hotspots)
				: empty_allowed(empty_allowed), promptmsg(std::move(promptmsg)), hotspots(std::move(hotspots)) {}

		void GatherHotspots(std::vector<Hotspot*>& gathered) {
			gathered.reserve(gathered.size() + hotspots.size());
			for (auto& hs : hotspots) {
				gathered.push_back(&hs);
			}
		}
	};

	class InputHandlers {
	public:
		class KeyOnly : public InputHandler {
		public:
			SDL_Keycode key_sym = SDLK_UNKNOWN;

			struct {
				std::function<void(InputHandler*)> Parsed = {};
			} events;

			KeyOnly(std::string&& promptmsg) : InputHandler(true, std::move(promptmsg)), events() {}

			KeyOnly(std::string&& promptmsg, std::vector<Hotspot>&& hotspots)
					: InputHandler(true, std::move(promptmsg), std::move(hotspots)) {}

			void GetPromptMessage(char* buf, size_t buf_size) override {
				snprintf(buf, buf_size, "%s", promptmsg.c_str());
			}

			// On Input just records the key
			bool OnInput(SDL_Keycode key_sym) override {
				this->key_sym = Hotspot::FixUppercaseKeycode(key_sym);
				return true;
			}

			bool check_key(SDL_Keycode key_sym) {
				return this->key_sym == Hotspot::FixUppercaseKeycode(key_sym);
			}

			void Parse() override;

			void clear() override {
				InputHandler::clear();
				key_sym = 0;
			}
		};

		class PressAKey : public KeyOnly {
		public:
			PressAKey(std::string&& promptmsg = {}) : KeyOnly(std::move(promptmsg)) {}

			virtual void GetPromptMessage(char* buf, size_t buf_size) override {
				snprintf(buf, buf_size, "%s%sPress a Key", promptmsg.c_str(), promptmsg.empty() ? "" : " ");
			}
		};

		class String : public InputHandler {
			std::string invalidmsg;

		public:
			String(bool empty_allowed, std::string&& promptmsg, std::string&& invalidmsg)
					: InputHandler(empty_allowed, std::move(promptmsg)), invalidmsg(std::move(invalidmsg)) {}

			bool OnInput(SDL_Keycode key_sym) override;
			void Parse() override;
		};

		class Integer : public InputHandler {
		protected:
			bool        hexonly;
			int         val_min, val_max;
			std::string invalidmsg;

		public:
			int value = 0;

			void GetPromptMessage(char* buf, size_t buf_size) override;

			bool OnInput(SDL_Keycode key_sym) override;
			void Parse() override;

			void clear() override {
				InputHandler::clear();
				value = val_min;
			}

			void setMax(int new_max) {
				val_max = new_max;
			}

			Integer(bool empty_allowed, int min, int max, bool hex);
			Integer(bool empty_allowed, int min, int max, bool hex, std::string&& promptmsg);
			Integer(bool empty_allowed, int min, int max, bool hex, std::string&& promptmsg, std::string&& invalidmsg);
		};

		class Shape : public Integer {
		public:
			int  shapenum     = -1;
			int  framenum     = -1;
			bool wantframenum = false;
			Shape(bool empty_allowed, bool wantframenum = false);
			bool OnInput(SDL_Keycode key_sym) override;
			void Parse() override;

			void clear() override;
		};

		// GameObject inherits from Integer so npc number can be entered for an object
		class GameObject : public Integer {
		public:
			Game_object* object = nullptr;
			GameObject(bool empty_allowed);
			GameObject(bool empty_allowed, std::string&& promptmsg);
			GameObject(bool empty_allowed, std::string&& promptmsg, std::string&& invalidmsg);
			bool OnInput(SDL_Keycode key_sym) override;
			void Parse() override;

			void clear() override {
				Integer::clear();
				object = nullptr;
			}
		};

		class NPC : public GameObject {
		public:
			NPC(bool empty_allowed);
			NPC(bool empty_allowed, std::string&& promptmsg);
			Actor* actor = nullptr;
			void   Parse() override;

			void clear() override {
				GameObject::clear();
				actor = nullptr;
			}
		};
	};

	struct Menu;

	class MenuCommand : public std::enable_shared_from_this<MenuCommand> {
		bool input_active = false;

	public:
		size_t                                     phase = 0;
		std::vector<std::shared_ptr<InputHandler>> inputs;
		// Back reference to the MenuCommand below us in the stack
		std::shared_ptr<MenuCommand> below;
		std::any                     data;
		Menu*                        owner = nullptr;

		Hotspot* hotspot = nullptr;

		MenuCommand() {}

		virtual ~MenuCommand() {}    // Funtcion Callbacks for Events

		struct Events {
			std::function<std::shared_ptr<MenuCommand>(MenuCommand*, SDL_Keycode keycode)> Activate      = {};
			std::function<bool(MenuCommand*)>                                              paint_display = {};
			std::function<void(MenuCommand*)>                                              run           = {};
			std::function<void(MenuCommand*)> cancelled = {};    // Called if input was cancelled (escape pressed) by user
		} events;

		// Virtual methods for input handlers
		virtual std::shared_ptr<MenuCommand> Activate(SDL_Keycode keycode) {
			if (events.Activate) {
				return events.Activate(this, keycode);
			}
			return {};
		}

		virtual void paint_display() {
			// let event handler paint the display first and if it doesn't forward to below
			if (!events.paint_display || !events.paint_display(this)) {
				below->paint_display();
			}
		}

		virtual void cancelled() {
			if (events.cancelled) {
				events.cancelled(this);
			}
		}

		InputHandler* GetInputHandler() const {
			if (phase >= inputs.size()) {
				return {};
			}
			return inputs[phase].get();
		}

		virtual void GatherHotspots(std::vector<Hotspot*>& gathered) {
			if (below) {
				below->GatherHotspots(gathered);
			}
		}

		virtual void run() {
			if (events.run) {
				events.run(this);
			}
		}

		// Returns true if we are top of the stack, active and receiving input
		bool isInputActive() {
			return input_active;
		}

		// Returns false if no more phases
		virtual bool BeginPhase() {
			if (phase >= inputs.size()) {
				input_active = false;
				return false;
			}
			inputs[phase]->clear();
			input_active = true;
			return true;
		}

		virtual void EndPhase() {
			++phase;
			if (phase >= inputs.size()) {
				input_active = false;
			}
		}

		void ResetPhase() {
			phase = 0;
			// This is set false until BeginPhase is called
			input_active = false;
		}

		// Returns true if the command is ready to be activated, that is there are no more phases
		bool Ready() {
			return phase >= inputs.size();
		}

		std::shared_ptr<Menu> GetMyMenu() {
			if (owner) {
				return std::static_pointer_cast<Menu>(owner->shared_from_this());
			}
			if (below) {
				auto menu = std::dynamic_pointer_cast<Menu>(below);
				if (menu) {
					return menu;
				}
				return below->GetMyMenu();
			}
			return {};
		}

		// Helper Methods to get and set data. This is used by menus to keep track of the current object (actor, flag num) being
		// edited

		template <typename T>
		void setData(T value) {
			data = std::make_any<T>(value);
		}

		template <typename T>
		T getDataOrDefault(T defaultValue = {}) const {
			if (!data.has_value() || data.type() != typeid(T)) {
				return defaultValue;
			}

			return std::any_cast<T>(data);
		}

		template <typename T>
		static T getDataOrDefault(MenuCommand* self, T defaultValue = {}) {
			return self ? self->getDataOrDefault<T>(defaultValue) : defaultValue;
		}

		template <typename T>
		static T getDataOrDefault(std::shared_ptr<MenuCommand> self, T defaultValue = {}) {
			return self ? self->getDataOrDefault<T>(defaultValue) : defaultValue;
		}
	};

	struct LeftRightIntegerCommand : public MenuCommand {
		int  min, max, currentval;
		bool update_label;

		LeftRightIntegerCommand(int min, int max, int initial, bool update_label = true)
				: MenuCommand(), min(std::min(min, max)), max(std::max(min, max)), currentval(initial), update_label(update_label) {
		}

		std::shared_ptr<MenuCommand> Activate(SDL_Keycode keycode) override {
			// first hotspot key is decrement
			if (hotspot->IsKeycode(keycode, 1 << 0) && currentval > min) {
				currentval--;
				if (currentval > max) {
					currentval = max;
				}
			}
			// second key is increment
			if (hotspot->IsKeycode(keycode, 1 << 1) && currentval < max) {
				currentval++;
				if (currentval < min) {
					currentval = min;
				}
			}

			return MenuCommand::Activate(keycode);
		}

		void run() override {
			MenuCommand::run();
			// Set each key hidden based on comparing the currentvalue to min and max
			hotspot->setHidden(0, currentval <= min);
			hotspot->setHidden(1, currentval >= max);
			if (update_label) {
				char valstring[16];
				snprintf(valstring, std::size(valstring), "%4i", currentval);
				hotspot->label_rw = (hotspot->label + valstring);
			}
		}
	};

	struct ToggleCommand : public MenuCommand {
		bool                       state;
		std::array<const char*, 2> state_string;

		ToggleCommand(bool state, std::array<const char*, 2>&& state_string = {"N", "Y"})
				: MenuCommand(), state(state), state_string(std::move(state_string)) {}

		std::shared_ptr<MenuCommand> Activate(SDL_Keycode keycode) override {
			state = !state;

			return MenuCommand::Activate(keycode);
		}

		void run() override {
			MenuCommand::run();
			// Set the hotspot label based on the currentstate

			if (hotspot) {
				hotspot->label_rw = hotspot->label + state_string[int(state)];
			}
		}
	};

	// A menu is a menu command and can be directly used as a menu item to create multi level menus
	struct Menu : public MenuCommand {
		std::forward_list<std::pair<Hotspot, std::shared_ptr<MenuCommand>>> items;

	public:
		Menu(std::forward_list<std::pair<Hotspot, std::shared_ptr<MenuCommand>>>&& items);

		std::shared_ptr<MenuCommand> Activate(SDL_Keycode keycode) override;

		void GatherHotspots(std::vector<Hotspot*>& gathered) override {
			for (auto& it : items) {
				gathered.push_back(&it.first);
				// it.second->GatherHotspots(gathered);
			}
		}

		void run() override {
			if (isInputActive()) {
				// Forward to our children if we are recieving input
				for (const auto& it : items) {
					if (it.second) {
						it.second->run();
					}
				}
			}
		}
	};

	SDL_Keycode GetKey(const std::vector<Hotspot*>& hotspots, SDL_Keycode& unicode);

	void RunMenu(std::shared_ptr<Menu> menu);

	struct {
		Uint32 last = 0;
		// Accumulated swipe deltas. We treat these as a vector
		float dx = 0;
		float dy = 0;

	} swipe;

	struct {
		Uint32      highlight     = 0;
		Uint32      highlighttime = 0;
		char        input[17]     = {0};
		int         command       = 0;
		bool        activate      = false;
		const char* custom_prompt = nullptr;
		int         saved_value   = 0;
		long        val_min       = LONG_MIN;
		long        val_max       = LONG_MAX;
		long        value;
		Uint32      last_swipe = 0;
		// Accumulated swipe deltas. We treat these as a vector
		float swipe_dx = 0;
		float swipe_dy = 0;

	private:
		Cheat_Prompt mode = CP_Command;

	public:
		Cheat_Prompt GetMode() {
			return mode;
		}

		void SetMode(Cheat_Prompt newmode, bool clearinput = true) {
			// Clear the input if changing to or from a text/value input mode
			if (clearinput) {
				input[0] = 0;
			}
			mode = newmode;
		}
	} state;

	template <class T>
	struct ClearState {
		T* ptr = nullptr;

		ClearState() = delete;

		ClearState(T& obj, bool now = true, bool on_destruct = true) : ptr(&obj) {
			if (now) {
				*ptr = T();
			}
			if (!on_destruct) {
				ptr = nullptr;
			}
		}

		~ClearState() {
			if (ptr) {
				*ptr = T();
			}
		}
	};

	Image_buffer8*        ibuf  = nullptr;
	std::shared_ptr<Font> font  = nullptr;
	Game_clock*           clock = nullptr;
	int                   maxx = 0, maxy = 0;
	int                   centerx = 0, centery = 0;
	Palette               pal;
	Xform_palette         highlighttable;
	Xform_palette         hovertable;
	Xform_palette         fontcolor;
	Xform_palette         fontcolor2;

	// Turn off clang-format so it doesn't wrap the long comments
	// clang-format off

	// Constants used for touch input
	const float  swipe_threshold     = 0.075f;	// Threshold for Swipes to be converted into key inputs.

	// clang-format on

	void                  SharedPrompt();
	bool                  SharedInput();
	void                  SharedMenu();
	void                  PaintHotspots();
	std::shared_ptr<Menu> RootMenu();

	void ActivityDisplay();

	// Paint an arrow using the font, type is one of '^' 'v' '<' '>'
	int PaintArrow(int offsetx, int offsety, int type);

	int PaintKeyName(int offsetx, int offsety, SDL_Keycode key_sym);

	std::shared_ptr<Menu> GlobalFlagMenu(unsigned num);

	std::shared_ptr<Menu> NPCMenu(Actor* actor);

	void         FlagLoop(Actor* actor);
	void         FlagMenu(Actor* actor);
	void         FlagActivate(Actor* actor);
	bool         FlagCheck(Actor* actor);
	Cheat_Prompt AdvancedFlagLoop(int flagnum, Actor* actor);

	std::shared_ptr<Menu> BusinessMenu(Actor* actor);

	void StatLoop(Actor* actor);
	void StatMenu(Actor* actor);
	void StatActivate(Actor* actor);
	bool StatCheck(Actor* actor);
	void PalEffectLoop(Actor*);
	void PalEffectMenu(Actor* actor);
	void PalEffectActivate(Actor* actor);
	bool PalEffectCheck(Actor* actor);

	std::shared_ptr<Menu> TeleportMenu();

	//! @brief Add a menu item with a hotspot for the Specified key
	//! @param offsetx X coord for the menu item
	//! @param offsety Y coord for the menu item
	//! @param keycode Keycode used to activate the menuitem
	//! @param label Label of the Menu item
	//! @return Width in pixels of the menu item
	int AddMenuItem(int offsetx, int offsety, SDL_Keycode keycode, const char* label);

	//! @brief Add a menuitem for left and right cursor keys
	//! @param offsetx X coord for the menu item
	//! @param offsety  Y coord for the menu item
	//! @param label Label of the Menu item
	//! @param left Flag for if the left arrow should be shown
	//! @param right Flag for if the right arrow should be shown
	//! @param leaveempty Flag to indicate that blank space should be used
	//! inplace of missing arrows
	//! @param fixedlabel Flag to indicate the label should be drawn at a fixed
	//! position if arrows are mising and !leaveempty
	//! @return Width in pixels of the menu item
	int AddLeftRightMenuItem(
			int offsetx, int offsety, const char* label, bool left, bool right, bool leaveempty, bool fixedlabel = false);

	int  highest_map = INT_MIN;
	int  Get_highest_map();
	void EndFrame();

	//
	static inline const int      button_down_finger = -1;
	std::unordered_multiset<int> buttons_down;
	void                         WaitButtonsUp(bool silent = false);
};

#endif
