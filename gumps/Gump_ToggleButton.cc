/*
Copyright (C) 2001-2024 The Exult Team

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

#include "Gump_ToggleButton.h"

#include "gamewin.h"


void Gump_ToggleButton::setselection(int newsel) {
	set_frame(newsel);
	gwin->add_dirty(get_rect());
}


void Gump_ToggleTextButton::setselection(int selectionnum) {
	gwin->add_dirty(get_rect());
	set_frame(selectionnum);
	text = selections[get_framenum()];
	init();
	gwin->add_dirty(get_rect());
}

void Gump_ToggleButton::paint() {
	// Paint using gump_widget instead of gump_button.
	Gump_widget::paint();
}


template <typename base>
bool Toggle_button<base>::activate(MouseButton button) {
	int       delta;
	if (button == MouseButton::Left) {
		delta = 1;
	} else if (button == MouseButton::Right) {
		delta = numselections - 1;
	} else {
		return false;
	}

	this->setselection((this->get_framenum() + delta) % numselections);
	toggle(this->get_framenum());
	base::gwin->add_dirty(this->get_rect());
	return true;
}

template <typename base>
bool Toggle_button<base>::push(MouseButton button) 
{
	if (button == MouseButton::Left || button == MouseButton::Right) {
		this->set_pushed(button);
		base::gwin->add_dirty(this->get_rect());
		return true;
	}
	return false;
}

template <typename base>
void Toggle_button<base>::unpush(MouseButton button) {
	if (button == MouseButton::Left || button == MouseButton::Right) {
		this->set_pushed(false);
		base::gwin->add_dirty(this->get_rect());
	}
}

//
// Constructors
//
// Constructors are last in the file so they correctly instatiate the templated virtual functions 
//

 Gump_ToggleButton::Gump_ToggleButton(
		Gump* par, int px, int py, int shapenum, int selectionnum, int numsel,
		ShapeFile shfile)
		: Toggle_button(numsel,par, shapenum, px, py, shfile)  {
	set_frame(selectionnum);
}

 Gump_ToggleTextButton::Gump_ToggleTextButton(
		Gump_Base* par, const std::vector<std::string>& s, int selectionnum,
		int px, int py, int width, int height)
		: Toggle_button(s.size(), par, "", px, py, width, height), selections(s) {
	set_frame(selectionnum);

	// call init for all of the strings to ensure the widget is wide enough
	// for all of them
	for (auto& selection : selections) {
		text = selection;
		init();
	}

		// Set the text to the actual default selection
	if (selectionnum >= 0 && size_t(selectionnum) < selections.size()) {
		text = selections[selectionnum];
	} else {
		// If selection is out of range show no text
		text.clear();
	}
	init();
 }

 Gump_ToggleTextButton::Gump_ToggleTextButton(
		 Gump_Base* par, std::vector<std::string>&& s, int selectionnum, int px,
		 int py, int width, int height)
		 : Toggle_button(s.size(), par, "", px, py, width, height),
		   selections(std::move(s)) {
	 set_frame(selectionnum);
	 // call init for all of the strings to ensure the widget is wide enough
	 // for all of them
	 for (auto& selection : selections) {
		 text = selection;
		 init();
	 }
	 // Set the text to the actual default selection
	 if (selectionnum >= 0 && size_t(selectionnum) < selections.size()) {
		 text = selections[selectionnum];
	 } else {
		 // If selection is out of range show no text
		 text.clear();
	 }
	 init();
 }
