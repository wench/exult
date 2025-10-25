/*
Copyright (C) 2001-2022 The Exult Team

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

#ifndef GUMP_TOGGLEBUTTON_H
#define GUMP_TOGGLEBUTTON_H

#include "Gump_button.h"
#include "Text_button.h"

#include <string>
#include <vector>

template<typename base>
class Toggle_button : public base {
public:
	using MouseButton = Gump_Base::MouseButton;

	bool push(MouseButton button) override;
	void unpush(MouseButton button) override;
	bool activate(MouseButton button) override;

	int getselection() const final {
		return this->get_framenum();
	}

	virtual void toggle(int state) = 0;

protected:
	template <typename... Ts>
	Toggle_button(int numselections, Ts&&... args)
			: base(std::forward<Ts>(args)...),
					  numselections(numselections) {}
	const int numselections;
};

/*
 * A button that toggles shape when pushed
 */

class Gump_ToggleButton : public Toggle_button<Gump_button> {
public:
	Gump_ToggleButton(
			Gump* par, int px, int py, int shapenum, int selectionnum,
			int numsel, ShapeFile shfile = SF_EXULT_FLX);


	void    setselection(int newsel) override;
	void paint() override;
};

/*
 * A text button that toggles shape when pushed
 */

class Gump_ToggleTextButton : public Toggle_button<Text_button> {
public:
	Gump_ToggleTextButton(
			Gump_Base* par, const std::vector<std::string>& s, int selectionnum,
			int px, int py, int width, int height = 0);

	Gump_ToggleTextButton(
			Gump_Base* par, std::vector<std::string>&& s, int selectionnum,
			int px, int py, int width, int height = 0);

	void setselection(int newsel) override;


private:
	const std::vector<std::string> selections;
};

template <typename Parent>
class CallbackToggleButton : public Gump_ToggleButton {
public:
	using CallbackType  = void (Parent::*)(int state);
	using CallbackType2 = void (Parent::*)(Gump_widget*,int state);

	template <typename... Ts>
	CallbackToggleButton(Parent* par, CallbackType&& callback, Ts&&... args)
			: Gump_ToggleButton(par, std::forward<Ts>(args)...),
			  parent(par), on_toggle(std::forward<CallbackType>(callback)) {}

	template <typename... Ts>
	CallbackToggleButton(
			Parent* par, CallbackType2&& callback, Ts&&... args)
			: Gump_ToggleButton(par, std::forward<Ts>(args)...),
			  parent(par), on_toggle2(std::forward<CallbackType2>(callback)) {}

	void toggle(int state) override {
		if (on_toggle) {
			(parent->*on_toggle)(state);
		}
		if (on_toggle2) {
			(parent->*on_toggle2)(this,state);
		}
		parent->paint();
	}

private:
	Parent*       parent;
	CallbackType  on_toggle  = nullptr;
	CallbackType2 on_toggle2 = nullptr;
};

template <typename Parent>
class CallbackToggleTextButton : public Gump_ToggleTextButton {
public:
	using CallbackType  = void (Parent::*)(int state);
	using CallbackType2 = void (Parent::*)(Gump_widget*);

	template <typename... Ts>
	CallbackToggleTextButton(Parent* par, CallbackType&& callback, Ts&&... args)
			: Gump_ToggleTextButton(par, std::forward<Ts>(args)...),
			  parent(par), on_toggle(std::forward<CallbackType>(callback)) {}

	template <typename... Ts>
	CallbackToggleTextButton(
			Parent* par, CallbackType2&& callback, Ts&&... args)
			: Gump_ToggleTextButton(par, std::forward<Ts>(args)...),
			  parent(par), on_toggle2(std::forward<CallbackType2>(callback)) {}

	void toggle(int state) override {
		if (on_toggle) {
			(parent->*on_toggle)(state);
		}
		if (on_toggle2) {
			(parent->*on_toggle2)(this);
		}
		parent->paint();
	}

private:
	Parent*       parent;
	CallbackType  on_toggle  = nullptr;
	CallbackType2 on_toggle2 = nullptr;
};

template <typename Parent>
class SelfManagedCallbackToggleTextButton
		: public CallbackToggleTextButton<Parent> {
public:
	template <typename... Ts>
	SelfManagedCallbackToggleTextButton(Ts&&... args)
			: CallbackToggleTextButton<Parent>(std::forward<Ts>(args)...) {
		Gump_button::set_self_managed(true);
	}
};
#endif
