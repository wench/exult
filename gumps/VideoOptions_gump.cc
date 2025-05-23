/*
 *  Copyright (C) 2001-2024  The Exult Team
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

#include "Configuration.h"
#include "Gump_ToggleButton.h"
#include "Gump_button.h"
#include "Text_button.h"
#include "VideoOptions_gump.h"
#include "Yesno_gump.h"
#include "exult.h"
#include "exult_flx.h"
#include "font.h"
#include "gameclk.h"
#include "gamewin.h"
#include "mouse.h"
#include "palette.h"

#include <cstring>
#include <iostream>
#include <iterator>
#include <set>
#include <string>

using std::string;

VideoOptions_gump* VideoOptions_gump::video_options_gump = nullptr;

static const int rowy[]
		= {5, 17, 29, 41, 53, 65, 77, 89, 101, 113, 130, 139, 156};
static const int   colx[]     = {35, 50, 119, 127, 131, 153};
static const char* applytext  = "APPLY";
static const char* canceltext = "CANCEL";
static const char* helptext   = "HELP";

static inline uint32 make_resolution(uint16 width, uint16 height) {
	return (uint32(width) << 16) | uint32(height);
}

static inline uint16 get_width(uint32 resolution) {
	return resolution >> 16;
}

static inline uint16 get_height(uint32 resolution) {
	return resolution & 0xffffu;
}

static string resolutionstring(int w, int h) {
	char buf[100];
	snprintf(buf, sizeof(buf), "%ix%i", w, h);
	return buf;
}

static string resolutionstring(uint32 resolution) {
	if (resolution == 0) {
		return "Auto";
	}
	return resolutionstring(get_width(resolution), get_height(resolution));
}

using VideoOptions_button = CallbackTextButton<VideoOptions_gump>;
using VideoTextToggle     = CallbackToggleTextButton<VideoOptions_gump>;

void VideoOptions_gump::close() {
	// save_settings();

	// have to repaint everything in case resolution changed
	gwin->set_all_dirty();
	done = true;
}

void VideoOptions_gump::cancel() {
	done = true;
}

void VideoOptions_gump::help() {
	SDL_OpenURL("https://exult.info/docs.php#video_gump");
}

void VideoOptions_gump::rebuild_buttons() {
	// skip apply, fullscreen, and share settings
	for (int i = id_resolution; i < id_count; i++) {
		buttons[i].reset();
	}

	// the text arrays are freed by the destructors of the buttons

	std::vector<std::string> scalers;
	scalers.reserve(Image_window::NumScalers);
	for (int i = 0; i < Image_window::SDLScaler; i++) {
		scalers.emplace_back(Image_window::get_name_for_scaler(i));
	}
	buttons[id_scaler] = std::make_unique<VideoTextToggle>(
			this, &VideoOptions_gump::toggle_scaler, std::move(scalers), scaler,
			colx[2], rowy[3], 74);

	std::vector<std::string> game_restext;
	game_restext.reserve(game_resolutions.size());
	for (auto res : game_resolutions) {
		game_restext.emplace_back(resolutionstring(res));
	}
	int  selected_game_resolution;
	auto it = std::find(
			game_resolutions.cbegin(), game_resolutions.cend(),
			game_resolution);
	if (it == game_resolutions.cend()) {
		// Should never happen... but anyway.
		selected_game_resolution = 0;
	} else {
		selected_game_resolution = it - game_resolutions.cbegin();
	}

	buttons[id_game_resolution] = std::make_unique<VideoTextToggle>(
			this, &VideoOptions_gump::toggle_game_resolution,
			std::move(game_restext), selected_game_resolution, colx[2], rowy[6],
			74);

	std::vector<std::string> fill_scaler_text = {"Point", "Bilinear"};
	{
		const char* renderer_name = SDL_GetRendererName(
				SDL_GetRenderer(gwin->get_win()->get_screen_window()));
		if (renderer_name) {
			fill_scaler_text.emplace_back(renderer_name);
		}
	}
	buttons[id_fill_scaler] = std::make_unique<VideoTextToggle>(
			this, &VideoOptions_gump::toggle_fill_scaler,
			std::move(fill_scaler_text), fill_scaler, colx[2], rowy[7], 74);

	int sel_fill_mode;
	has_ac = false;

	if (fill_mode == Image_window::Fill) {
		sel_fill_mode = 0;
	} else if (fill_mode == Image_window::Fit) {
		sel_fill_mode = 1;
	} else if (fill_mode == Image_window::AspectCorrectFit) {
		sel_fill_mode = 1;
		has_ac        = true;
	} else if (fill_mode == Image_window::Centre) {
		sel_fill_mode = 2;
	} else if (fill_mode == Image_window::AspectCorrectCentre) {
		sel_fill_mode = 2;
		has_ac        = true;
	} else {
		sel_fill_mode = 3;
	}
	std::vector<std::string> fill_mode_text = {"Fill", "Fit", "Centre"};
	if (startup_fill_mode > Image_window::AspectCorrectCentre) {
		fill_mode_text.emplace_back("Custom");
	}

	buttons[id_fill_mode] = std::make_unique<VideoTextToggle>(
			this, &VideoOptions_gump::toggle_fill_mode,
			std::move(fill_mode_text), sel_fill_mode, colx[2], rowy[8], 74);

	rebuild_dynamic_buttons();
}

void VideoOptions_gump::rebuild_dynamic_buttons() {
	buttons[id_resolution].reset();
	buttons[id_scaling].reset();
	buttons[id_has_ac].reset();

	const auto&  resolutionsref = fullscreen ? resolutions : win_resolutions;
	const uint32 current_res    = make_resolution(
            gwin->get_win()->get_display_width(),
            gwin->get_win()->get_display_height());

	std::vector<std::string> restext;
	restext.reserve(resolutionsref.size());
	for (auto res : resolutionsref) {
		restext.emplace_back(resolutionstring(res));
	}
	auto it = std::find(
			resolutionsref.cbegin(), resolutionsref.cend(), current_res);
	int selected_res = 0;
	if (it == resolutionsref.cend()) {
		// Just in case
		selected_res = 0;
	} else {
		selected_res = it - resolutionsref.cbegin();
	}

	resolution = resolutionsref[selected_res];

	buttons[id_resolution] = std::make_unique<VideoTextToggle>(
			this, &VideoOptions_gump::toggle_resolution, std::move(restext),
			selected_res, colx[2], rowy[1], 74);

	const int max_scales = scaling > 8 && scaling <= 16 ? scaling : 8;
	const int num_scales = (scaler == Image_window::point
							|| scaler == Image_window::interlaced
							|| scaler == Image_window::bilinear
							|| scaler == Image_window::SDLScaler)
								   ? max_scales
								   : 1;
	if (num_scales > 1) {
		// the text arrays is freed by the destructor of the button
		std::vector<std::string> scalingtext;
		for (int i = 0; i < num_scales; i++) {
			scalingtext.push_back('x' + std::to_string(i + 1));
		}
		buttons[id_scaling] = std::make_unique<VideoTextToggle>(
				this, &VideoOptions_gump::toggle_scaling,
				std::move(scalingtext), scaling, colx[2], rowy[4], 74);
	} else if (scaler == Image_window::Hq3x || scaler == Image_window::_3xBR) {
		scaling = 2;
	} else if (scaler == Image_window::Hq4x || scaler == Image_window::_4xBR) {
		scaling = 3;
	} else {
		scaling = 1;
	}

	if (fill_mode == Image_window::Fit
		|| fill_mode == Image_window::AspectCorrectFit
		|| fill_mode == Image_window::Centre
		|| fill_mode == Image_window::AspectCorrectCentre) {
		std::vector<std::string> ac_text = {"Disabled", "Enabled"};
		buttons[id_has_ac]               = std::make_unique<VideoTextToggle>(
                this, &VideoOptions_gump::toggle_aspect_correction,
                std::move(ac_text), has_ac ? 1 : 0, colx[4], rowy[9], 62);
	}
}

void VideoOptions_gump::load_settings(bool Fullscreen) {
	fullscreen = Fullscreen;
	setup_video(fullscreen, MENU_INIT);

	if (resolutions.empty()) {
		fullscreen = gwin->get_win()->is_fullscreen() ? 1 : 0;
		{
			std::set<uint32> Resolutions;
			std::transform(
					Image_window::Resolutions.cbegin(),
					Image_window::Resolutions.cend(),
					std::inserter(Resolutions, Resolutions.end()),
					[](const auto elem) {
						return elem.first;
					});
			auto it = std::find(
					Resolutions.cbegin(), Resolutions.cend(), resolution);
			if (it == Resolutions.cend()) {
				Resolutions.insert(resolution);
			}

			resolutions.reserve(Resolutions.size());
			for (const auto elem : Resolutions) {
				resolutions.push_back(elem);
			}
		}
		{
			// Add in useful window resolutions
			// Ratios          8:5 Game exact,    4:3 Top and Bottom bands
			std::set<uint32> Resolutions{
					//  320 Set : x1, x9/8, x5/4, x3/2, x8/5, x9/5, x15/8, (x2)
					make_resolution(320, 180), make_resolution(320, 200),
					make_resolution(320, 240), make_resolution(360, 225),
					make_resolution(360, 270), make_resolution(400, 225),
					make_resolution(400, 250), make_resolution(400, 300),
					make_resolution(480, 270), make_resolution(480, 300),
					make_resolution(480, 360), make_resolution(512, 288),
					make_resolution(512, 320), make_resolution(512, 384),
					make_resolution(576, 324), make_resolution(576, 360),
					make_resolution(576, 432), make_resolution(600, 375),
					make_resolution(600, 450),
					//  640 Set : x1, x9/8, x5/4, x3/2, x8/5, x9/5, x15/8, (x2)
					make_resolution(640, 360), make_resolution(640, 400),
					make_resolution(640, 480), make_resolution(720, 405),
					make_resolution(720, 450), make_resolution(720, 540),
					make_resolution(800, 450), make_resolution(800, 500),
					make_resolution(800, 600), make_resolution(960, 540),
					make_resolution(960, 600), make_resolution(960, 720),
					make_resolution(1024, 576), make_resolution(1024, 640),
					make_resolution(1024, 768), make_resolution(1152, 648),
					make_resolution(1152, 720), make_resolution(1152, 864),
					make_resolution(1200, 675), make_resolution(1200, 750),
					make_resolution(1200, 900),
					// 1280 Set : x1, x9/8, x5/4, x3/2, x8/5, x9/5, x15/8, (x2)
					make_resolution(1280, 720), make_resolution(1280, 800),
					make_resolution(1280, 960), make_resolution(1440, 810),
					make_resolution(1440, 900), make_resolution(1440, 1080),
					make_resolution(1600, 900), make_resolution(1600, 1000),
					make_resolution(1600, 1200), make_resolution(1920, 1080),
					make_resolution(1920, 1200), make_resolution(1920, 1440),
					make_resolution(2048, 1152), make_resolution(2048, 1280),
					make_resolution(2048, 1536), make_resolution(2304, 1296),
					make_resolution(2304, 1440), make_resolution(2304, 1728),
					make_resolution(2400, 1350), make_resolution(2400, 1500),
					make_resolution(2400, 1800),
					// 2560 Set : x1, x9/8, x5/4, x3/2, x8/5, x9/5, x15/8, (x2)
					make_resolution(2560, 1440), make_resolution(2560, 1600),
					make_resolution(2560, 1920), make_resolution(2880, 1620),
					make_resolution(2880, 1800), make_resolution(2880, 2160),
					make_resolution(3200, 1800), make_resolution(3200, 2000),
					make_resolution(3200, 2400), make_resolution(3840, 2160),
					make_resolution(3840, 2400), make_resolution(3840, 2880),
					make_resolution(4096, 2304), make_resolution(4096, 2560),
					make_resolution(4096, 3072), make_resolution(4608, 2592),
					make_resolution(4608, 2880), make_resolution(4608, 3456),
					make_resolution(4800, 2700), make_resolution(4800, 3000),
					make_resolution(4800, 3600),
					// 5120 Set : x1, x9/8, x5/4, x3/2, x8/5, x9/5, x15/8, (x2)
					make_resolution(5120, 2880), make_resolution(5120, 3200),
					make_resolution(5120, 3840), make_resolution(5760, 3240),
					make_resolution(5760, 3600), make_resolution(5760, 4320),
					make_resolution(6400, 3600), make_resolution(6400, 4000),
					make_resolution(6400, 4800), make_resolution(7680, 4320),
					make_resolution(7680, 4800), make_resolution(7680, 5760),
					make_resolution(8192, 4608), make_resolution(8192, 5120),
					make_resolution(8192, 6144), make_resolution(9216, 5184),
					make_resolution(9216, 5760), make_resolution(9216, 6912),
					make_resolution(9600, 5400), make_resolution(9600, 6000),
					make_resolution(9600, 7200)};
			auto it = std::find(
					Resolutions.cbegin(), Resolutions.cend(), resolution);
			if (it == Resolutions.cend()) {
				Resolutions.insert(resolution);
			}

			win_resolutions.reserve(Resolutions.size());
			for (const auto elem : Resolutions) {
				if (Image_window::VideoModeOK(
							get_width(elem), get_height(elem), false)) {
					win_resolutions.push_back(elem);
				}
			}
		}
	}

	if (startup_fill_mode == 0) {
		startup_fill_mode = fill_mode;
	}
	has_ac = false;

	if (game_resolutions.empty()) {
		game_resolutions.reserve(5);
		game_resolutions.push_back(0);    // Auto
		game_resolutions.push_back(make_resolution(320, 200));
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID)
		game_resolutions.push_back(make_resolution(400, 250));
		game_resolutions.push_back(make_resolution(480, 300));
#endif
		auto it = std::find(
				game_resolutions.cbegin(), game_resolutions.cend(),
				game_resolution);
		if (it == game_resolutions.cend()) {
			game_resolutions.push_back(game_resolution);
		}
	}

	gclock->reset_palette();

	o_resolution      = resolution;
	o_scaling         = scaling;
	o_scaler          = scaler;
	o_fill_scaler     = fill_scaler;
	o_fill_mode       = fill_mode;
	o_game_resolution = game_resolution;
}

VideoOptions_gump::VideoOptions_gump()
		: Modal_gump(nullptr, -1),
		  startup_fill_mode(static_cast<Image_window::FillMode>(0)) {
	SetProceduralBackground(TileRect(29, 2, 166, 166), -1);
	video_options_gump = this;

	const std::vector<std::string> enabledtext = {"Disabled", "Enabled"};

	fullscreen = gwin->get_win()->is_fullscreen();
#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID)
	buttons[id_fullscreen] = std::make_unique<VideoTextToggle>(
			this, &VideoOptions_gump::toggle_fullscreen, enabledtext,
			fullscreen, colx[2], rowy[0], 74);
#endif
	config->value("config/video/share_video_settings", share_settings, false);

	std::vector<std::string> yesNO = {"No", "Yes"};
#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID)
	buttons[id_share_settings] = std::make_unique<VideoTextToggle>(
			this, &VideoOptions_gump::toggle_share_settings, std::move(yesNO),
			share_settings, colx[5], rowy[11], 40);
#endif
	o_share_settings = share_settings;

	// Apply
	buttons[id_apply] = std::make_unique<VideoOptions_button>(
			this, &VideoOptions_gump::save_settings, applytext, colx[0] - 2,
			rowy[12], 50);
	// Cancel
	buttons[id_cancel] = std::make_unique<VideoOptions_button>(
			this, &VideoOptions_gump::cancel, canceltext, colx[5] - 10,
			rowy[12], 50);
	// Help
	buttons[id_help] = std::make_unique<VideoOptions_button>(
			this, &VideoOptions_gump::help, helptext, colx[2] - 31, rowy[12],
			50);
	load_settings(fullscreen);

	rebuild_buttons();
}

void VideoOptions_gump::save_settings() {
	int resx = get_width(resolution);
	int resy = get_height(resolution);
	int gw   = get_width(game_resolution);
	int gh   = get_height(game_resolution);

	int tgw = gw;
	int tgh = gh;
	int tw;
	int th;
	Image_window::get_draw_dims(
			resx, resy, scaling + 1, fill_mode, tgw, tgh, tw, th);
	if (tw / (scaling + 1) < 320 || th / (scaling + 1) < 200) {
		if (!Yesno_gump::ask(
					"Scaled size less than 320x200.\nExult may be "
					"unusable.\nApply anyway?",
					nullptr, "TINY_BLACK_FONT")) {
			return;
		}
	}
	gwin->resized(
			resx, resy, fullscreen != 0, gw, gh, scaling + 1, scaler, fill_mode,
			fill_scaler == 2   ? Image_window::SDLScaler
			: fill_scaler == 1 ? Image_window::bilinear
							   : Image_window::point);
	gclock->reset_palette();
	set_pos();
	gwin->set_all_dirty();

	if (!Countdown_gump::ask("Settings applied.\nKeep?", 20)) {
		resx = get_width(o_resolution);
		resy = get_height(o_resolution);
		gw   = get_width(o_game_resolution);
		gh   = get_height(o_game_resolution);
		bool o_fullscreen;
		config->value("config/video/fullscreen", o_fullscreen, true);
		if (fullscreen != o_fullscreen) {    // use old settings from the config
			setup_video(o_fullscreen, TOGGLE_FULLSCREEN);
		} else {
			gwin->resized(
					resx, resy, o_fullscreen, gw, gh, o_scaling + 1, o_scaler,
					o_fill_mode,
					o_fill_scaler == 2   ? Image_window::SDLScaler
					: o_fill_scaler == 1 ? Image_window::bilinear
										 : Image_window::point);
		}
		gclock->reset_palette();
		set_pos();
		gwin->set_all_dirty();
	} else {
		config->set(
				"config/video/fullscreen", fullscreen ? "yes" : "no", false);
		config->set(
				"config/video/share_video_settings",
				share_settings ? "yes" : "no", false);
		setup_video(
				fullscreen != 0, SET_CONFIG, resx, resy, gw, gh, scaling + 1,
				scaler, fill_mode,
				fill_scaler == 2   ? Image_window::SDLScaler
				: fill_scaler == 1 ? Image_window::bilinear
								   : Image_window::point);
		config->write_back();
		o_resolution      = resolution;
		o_scaling         = scaling;
		o_scaler          = scaler;
		o_game_resolution = game_resolution;
		o_fill_mode       = fill_mode;
		o_fill_scaler     = fill_scaler;
		o_share_settings  = share_settings;
	}
}

void VideoOptions_gump::paint() {
	Modal_gump::paint();
	for (auto& btn : buttons) {
		if (btn != nullptr) {
			btn->paint();
		}
	}

	std::shared_ptr<Font> font = fontManager.get_font("SMALL_BLACK_FONT");
	Image_window8*        iwin = gwin->get_win();
#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID)
	font->paint_text(
			iwin->get_ib8(), "Full Screen:", x + colx[0], y + rowy[0] + 1);
	if (fullscreen) {
		font->paint_text(
				iwin->get_ib8(), "Display Mode:", x + colx[0], y + rowy[1] + 1);
	} else {
		font->paint_text(
				iwin->get_ib8(), "Window Size:", x + colx[0], y + rowy[1] + 1);
	}
#else
	font->paint_text(
			iwin->get_ib8(), "Resolution:", x + colx[0], y + rowy[1] + 1);
#endif
	font->paint_text(iwin->get_ib8(), "Scaler:", x + colx[0], y + rowy[3] + 1);
	if (buttons[id_scaling] != nullptr) {
		font->paint_text(
				iwin->get_ib8(), "Scaling:", x + colx[0], y + rowy[4] + 1);
	}
	font->paint_text(
			iwin->get_ib8(), "Game Area:", x + colx[0], y + rowy[6] + 1);
	font->paint_text(
			iwin->get_ib8(), "Fill Quality:", x + colx[0], y + rowy[7] + 1);
	font->paint_text(
			iwin->get_ib8(), "Fill Mode:", x + colx[0], y + rowy[8] + 1);
	if (buttons[id_has_ac] != nullptr) {
		font->paint_text(
				iwin->get_ib8(), "AR Correction:", x + colx[0],
				y + rowy[9] + 1);
	}
#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID)
	font->paint_text(
			iwin->get_ib8(), "Same settings for window", x + colx[0],
			y + rowy[10] + 1);
	font->paint_text(
			iwin->get_ib8(), "and fullscreen:", x + colx[0], y + rowy[11] + 1);
#endif
	gwin->set_painted();
}

bool VideoOptions_gump::mouse_down(int mx, int my, MouseButton button) {
	// Only left and right buttons
	if (button !=

				MouseButton::Left
		&& button != MouseButton::Right) {
		return Modal_gump::mouse_down(mx, my, button);
	}

	// We'll eat the mouse down if we've already got a button down
	if (pushed) {
		return true;
	}

	// First try checkmark
	pushed = Gump::on_button(mx, my);

	// Try buttons at bottom.
	if (!pushed) {
		for (auto& btn : buttons) {
			if (btn != nullptr && btn->on_button(mx, my)) {
				pushed = btn.get();
				break;
			}
		}
	}

	if (pushed && !pushed->push(button)) {    // On a button?
		pushed = nullptr;
	}

	return pushed != nullptr || Modal_gump::mouse_down(mx, my, button);
}

bool VideoOptions_gump::mouse_up(int mx, int my, MouseButton button) {
	// Not Pushing a button?
	if (!pushed) {
		return Modal_gump::mouse_up(mx, my, button);
	}

	if (pushed->get_pushed() != button) {
		return button == MouseButton::Left;
	}

	bool res = false;
	pushed->unpush(button);
	if (pushed->on_button(mx, my)) {
		res = pushed->activate(button);
	}
	pushed = nullptr;
	return res || Modal_gump::mouse_up(mx, my, button);
}
