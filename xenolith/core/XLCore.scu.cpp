/**
Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#include "XLDefine.h"

#include "XLInputDispatcher.cc"
#include "XLEvent.cc"
#include "XLEventHeader.cc"
#include "XLEventHandler.cc"
#include "XLDirector.cc"
#include "XLResourceCache.cc"
#include "XLVertexArray.cc"
#include "XLScheduler.cc"
#include "XLTextInputManager.cc"

#include "base/XLApplication.cc"
#include "base/XLDeferredManager.cc"
#include "base/XLLog.cc"

namespace stappler::xenolith {

const ColorMode ColorMode::SolidColor = ColorMode();
const ColorMode ColorMode::IntensityChannel(gl::ComponentMapping::R, gl::ComponentMapping::One);
const ColorMode ColorMode::AlphaChannel(gl::ComponentMapping::One, gl::ComponentMapping::R);

StringView getInputKeyCodeName(InputKeyCode code) {
	switch (code) {
	case InputKeyCode::KP_DECIMAL: return StringView("KP_DECIMAL"); break;
	case InputKeyCode::KP_DIVIDE: return StringView("KP_DIVIDE"); break;
	case InputKeyCode::KP_MULTIPLY: return StringView("KP_MULTIPLY"); break;
	case InputKeyCode::KP_SUBTRACT: return StringView("KP_SUBTRACT"); break;
	case InputKeyCode::KP_ADD: return StringView("KP_ADD"); break;
	case InputKeyCode::KP_ENTER: return StringView("KP_ENTER"); break;
	case InputKeyCode::KP_EQUAL: return StringView("KP_EQUAL"); break;

	case InputKeyCode::BACKSPACE: return StringView("BACKSPACE"); break;
	case InputKeyCode::TAB: return StringView("TAB"); break;
	case InputKeyCode::ENTER: return StringView("ENTER"); break;

	case InputKeyCode::RIGHT: return StringView("RIGHT"); break;
	case InputKeyCode::LEFT: return StringView("LEFT"); break;
	case InputKeyCode::DOWN: return StringView("DOWN"); break;
	case InputKeyCode::UP: return StringView("UP"); break;
	case InputKeyCode::PAGE_UP: return StringView("PAGE_UP"); break;
	case InputKeyCode::PAGE_DOWN: return StringView("PAGE_DOWN"); break;
	case InputKeyCode::HOME: return StringView("HOME"); break;
	case InputKeyCode::END: return StringView("END"); break;
	case InputKeyCode::LEFT_SHIFT: return StringView("LEFT_SHIFT"); break;
	case InputKeyCode::LEFT_CONTROL: return StringView("LEFT_CONTROL"); break;
	case InputKeyCode::LEFT_ALT: return StringView("LEFT_ALT"); break;
	case InputKeyCode::LEFT_SUPER: return StringView("LEFT_SUPER"); break;
	case InputKeyCode::RIGHT_SHIFT: return StringView("RIGHT_SHIFT"); break;
	case InputKeyCode::RIGHT_CONTROL: return StringView("RIGHT_CONTROL"); break;
	case InputKeyCode::RIGHT_ALT: return StringView("RIGHT_ALT"); break;
	case InputKeyCode::RIGHT_SUPER: return StringView("RIGHT_SUPER"); break;

	case InputKeyCode::ESCAPE: return StringView("ESCAPE"); break;

	case InputKeyCode::INSERT: return StringView("INSERT"); break;
	case InputKeyCode::CAPS_LOCK: return StringView("CAPS_LOCK"); break;
	case InputKeyCode::SCROLL_LOCK: return StringView("SCROLL_LOCK"); break;
	case InputKeyCode::NUM_LOCK: return StringView("NUM_LOCK"); break;

	case InputKeyCode::SPACE: return StringView("SPACE"); break;

	case InputKeyCode::KP_0: return StringView("KP_0"); break;
	case InputKeyCode::KP_1: return StringView("KP_1"); break;
	case InputKeyCode::KP_2: return StringView("KP_2"); break;
	case InputKeyCode::KP_3: return StringView("KP_3"); break;
	case InputKeyCode::KP_4: return StringView("KP_4"); break;
	case InputKeyCode::KP_5: return StringView("KP_5"); break;
	case InputKeyCode::KP_6: return StringView("KP_6"); break;
	case InputKeyCode::KP_7: return StringView("KP_7"); break;
	case InputKeyCode::KP_8: return StringView("KP_8"); break;
	case InputKeyCode::KP_9: return StringView("KP_9"); break;

	case InputKeyCode::APOSTROPHE: return StringView("APOSTROPHE"); break;
	case InputKeyCode::COMMA: return StringView("COMMA"); break;
	case InputKeyCode::MINUS: return StringView("MINUS"); break;
	case InputKeyCode::PERIOD: return StringView("PERIOD"); break;
	case InputKeyCode::SLASH: return StringView("SLASH"); break;
	case InputKeyCode::_0: return StringView("0"); break;
	case InputKeyCode::_1: return StringView("1"); break;
	case InputKeyCode::_2: return StringView("2"); break;
	case InputKeyCode::_3: return StringView("3"); break;
	case InputKeyCode::_4: return StringView("4"); break;
	case InputKeyCode::_5: return StringView("5"); break;
	case InputKeyCode::_6: return StringView("6"); break;
	case InputKeyCode::_7: return StringView("7"); break;
	case InputKeyCode::_8: return StringView("8"); break;
	case InputKeyCode::_9: return StringView("9"); break;
	case InputKeyCode::SEMICOLON: return StringView("SEMICOLON"); break;
	case InputKeyCode::EQUAL: return StringView("EQUAL"); break;

	case InputKeyCode::WORLD_1: return StringView("WORLD_1"); break;
	case InputKeyCode::WORLD_2: return StringView("WORLD_2"); break;

	case InputKeyCode::A: return StringView("A"); break;
	case InputKeyCode::B: return StringView("B"); break;
	case InputKeyCode::C: return StringView("C"); break;
	case InputKeyCode::D: return StringView("D"); break;
	case InputKeyCode::E: return StringView("E"); break;
	case InputKeyCode::F: return StringView("F"); break;
	case InputKeyCode::G: return StringView("G"); break;
	case InputKeyCode::H: return StringView("H"); break;
	case InputKeyCode::I: return StringView("I"); break;
	case InputKeyCode::J: return StringView("J"); break;
	case InputKeyCode::K: return StringView("K"); break;
	case InputKeyCode::L: return StringView("L"); break;
	case InputKeyCode::M: return StringView("M"); break;
	case InputKeyCode::N: return StringView("N"); break;
	case InputKeyCode::O: return StringView("O"); break;
	case InputKeyCode::P: return StringView("P"); break;
	case InputKeyCode::Q: return StringView("Q"); break;
	case InputKeyCode::R: return StringView("R"); break;
	case InputKeyCode::S: return StringView("S"); break;
	case InputKeyCode::T: return StringView("T"); break;
	case InputKeyCode::U: return StringView("U"); break;
	case InputKeyCode::V: return StringView("V"); break;
	case InputKeyCode::W: return StringView("W"); break;
	case InputKeyCode::X: return StringView("X"); break;
	case InputKeyCode::Y: return StringView("Y"); break;
	case InputKeyCode::Z: return StringView("Z"); break;
	case InputKeyCode::LEFT_BRACKET: return StringView("LEFT_BRACKET"); break;
	case InputKeyCode::BACKSLASH: return StringView("BACKSLASH"); break;
	case InputKeyCode::RIGHT_BRACKET: return StringView("RIGHT_BRACKET"); break;
	case InputKeyCode::GRAVE_ACCENT: return StringView("GRAVE_ACCENT"); break;

		/* Function keys */
	case InputKeyCode::F1: return StringView("F1"); break;
	case InputKeyCode::F2: return StringView("F2"); break;
	case InputKeyCode::F3: return StringView("F3"); break;
	case InputKeyCode::F4: return StringView("F4"); break;
	case InputKeyCode::F5: return StringView("F5"); break;
	case InputKeyCode::F6: return StringView("F6"); break;
	case InputKeyCode::F7: return StringView("F7"); break;
	case InputKeyCode::F8: return StringView("F8"); break;
	case InputKeyCode::F9: return StringView("F9"); break;
	case InputKeyCode::F10: return StringView("F10"); break;
	case InputKeyCode::F11: return StringView("F11"); break;
	case InputKeyCode::F12: return StringView("F12"); break;
	case InputKeyCode::F13: return StringView("F13"); break;
	case InputKeyCode::F14: return StringView("F14"); break;
	case InputKeyCode::F15: return StringView("F15"); break;
	case InputKeyCode::F16: return StringView("F16"); break;
	case InputKeyCode::F17: return StringView("F17"); break;
	case InputKeyCode::F18: return StringView("F18"); break;
	case InputKeyCode::F19: return StringView("F19"); break;
	case InputKeyCode::F20: return StringView("F20"); break;
	case InputKeyCode::F21: return StringView("F21"); break;
	case InputKeyCode::F22: return StringView("F22"); break;
	case InputKeyCode::F23: return StringView("F23"); break;
	case InputKeyCode::F24: return StringView("F24"); break;
	case InputKeyCode::F25: return StringView("F25"); break;

	case InputKeyCode::MENU: return StringView("MENU"); break;
	case InputKeyCode::PRINT_SCREEN: return StringView("PRINT_SCREEN"); break;
	case InputKeyCode::PAUSE: return StringView("PAUSE"); break;
	case InputKeyCode::DELETE: return StringView("DELETE"); break;
	default: break;
	}
	return StringView();
}

StringView getInputKeyCodeKeyName(InputKeyCode code) {
	switch (code) {
	case InputKeyCode::KP_DECIMAL: return StringView("KPDL"); break;
	case InputKeyCode::KP_DIVIDE: return StringView("KPDV"); break;
	case InputKeyCode::KP_MULTIPLY: return StringView("KPMU"); break;
	case InputKeyCode::KP_SUBTRACT: return StringView("KPSU"); break;
	case InputKeyCode::KP_ADD: return StringView("KPAD"); break;
	case InputKeyCode::KP_ENTER: return StringView("KPEN"); break;
	case InputKeyCode::KP_EQUAL: return StringView("KPEQ"); break;

	case InputKeyCode::BACKSPACE: return StringView("BKSP"); break;
	case InputKeyCode::TAB: return StringView("TAB"); break;
	case InputKeyCode::ENTER: return StringView("RTRN"); break;

	case InputKeyCode::RIGHT: return StringView("RGHT"); break;
	case InputKeyCode::LEFT: return StringView("LEFT"); break;
	case InputKeyCode::DOWN: return StringView("DOWN"); break;
	case InputKeyCode::UP: return StringView("UP"); break;
	case InputKeyCode::PAGE_UP: return StringView("PGUP"); break;
	case InputKeyCode::PAGE_DOWN: return StringView("PGDN"); break;
	case InputKeyCode::HOME: return StringView("HOME"); break;
	case InputKeyCode::END: return StringView("END"); break;
	case InputKeyCode::LEFT_SHIFT: return StringView("LFSH"); break;
	case InputKeyCode::LEFT_CONTROL: return StringView("LCTL"); break;
	case InputKeyCode::LEFT_ALT: return StringView("LALT"); break;
	case InputKeyCode::LEFT_SUPER: return StringView("LWIN"); break;
	case InputKeyCode::RIGHT_SHIFT: return StringView("RTSH"); break;
	case InputKeyCode::RIGHT_CONTROL: return StringView("RCTL"); break;
	case InputKeyCode::RIGHT_ALT: return StringView("RALT"); break;
	case InputKeyCode::RIGHT_SUPER: return StringView("RWIN"); break;

	case InputKeyCode::ESCAPE: return StringView("ESC"); break;

	case InputKeyCode::INSERT: return StringView("INS"); break;
	case InputKeyCode::CAPS_LOCK: return StringView("CAPS"); break;
	case InputKeyCode::SCROLL_LOCK: return StringView("SCLK"); break;
	case InputKeyCode::NUM_LOCK: return StringView("NMLK"); break;

	case InputKeyCode::SPACE: return StringView("SPCE"); break;

	case InputKeyCode::KP_0: return StringView("KP0"); break;
	case InputKeyCode::KP_1: return StringView("KP1"); break;
	case InputKeyCode::KP_2: return StringView("KP2"); break;
	case InputKeyCode::KP_3: return StringView("KP3"); break;
	case InputKeyCode::KP_4: return StringView("KP4"); break;
	case InputKeyCode::KP_5: return StringView("KP5"); break;
	case InputKeyCode::KP_6: return StringView("KP6"); break;
	case InputKeyCode::KP_7: return StringView("KP7"); break;
	case InputKeyCode::KP_8: return StringView("KP8"); break;
	case InputKeyCode::KP_9: return StringView("KP9"); break;

	case InputKeyCode::APOSTROPHE: return StringView("AC11"); break;
	case InputKeyCode::COMMA: return StringView("AB08"); break;
	case InputKeyCode::MINUS: return StringView("AE11"); break;
	case InputKeyCode::PERIOD: return StringView("AB09"); break;
	case InputKeyCode::SLASH: return StringView("AB10"); break;
	case InputKeyCode::_0: return StringView("AE10"); break;
	case InputKeyCode::_1: return StringView("AE01"); break;
	case InputKeyCode::_2: return StringView("AE02"); break;
	case InputKeyCode::_3: return StringView("AE03"); break;
	case InputKeyCode::_4: return StringView("AE04"); break;
	case InputKeyCode::_5: return StringView("AE05"); break;
	case InputKeyCode::_6: return StringView("AE06"); break;
	case InputKeyCode::_7: return StringView("AE07"); break;
	case InputKeyCode::_8: return StringView("AE08"); break;
	case InputKeyCode::_9: return StringView("AE09"); break;
	case InputKeyCode::SEMICOLON: return StringView("AC10"); break;
	case InputKeyCode::EQUAL: return StringView("AE12"); break;

	case InputKeyCode::WORLD_1: return StringView("LSGT"); break;

	case InputKeyCode::A: return StringView("AC01"); break;
	case InputKeyCode::B: return StringView("AB05"); break;
	case InputKeyCode::C: return StringView("AB03"); break;
	case InputKeyCode::D: return StringView("AC03"); break;
	case InputKeyCode::E: return StringView("AD03"); break;
	case InputKeyCode::F: return StringView("AC04"); break;
	case InputKeyCode::G: return StringView("AC05"); break;
	case InputKeyCode::H: return StringView("AC06"); break;
	case InputKeyCode::I: return StringView("AD08"); break;
	case InputKeyCode::J: return StringView("AC07"); break;
	case InputKeyCode::K: return StringView("AC08"); break;
	case InputKeyCode::L: return StringView("AC09"); break;
	case InputKeyCode::M: return StringView("AB07"); break;
	case InputKeyCode::N: return StringView("AB06"); break;
	case InputKeyCode::O: return StringView("AD09"); break;
	case InputKeyCode::P: return StringView("AD10"); break;
	case InputKeyCode::Q: return StringView("AD01"); break;
	case InputKeyCode::R: return StringView("AD04"); break;
	case InputKeyCode::S: return StringView("AC02"); break;
	case InputKeyCode::T: return StringView("AD05"); break;
	case InputKeyCode::U: return StringView("AD07"); break;
	case InputKeyCode::V: return StringView("AB04"); break;
	case InputKeyCode::W: return StringView("AD02"); break;
	case InputKeyCode::X: return StringView("AB02"); break;
	case InputKeyCode::Y: return StringView("AD06"); break;
	case InputKeyCode::Z: return StringView("AB01"); break;
	case InputKeyCode::LEFT_BRACKET: return StringView("AD11"); break;
	case InputKeyCode::BACKSLASH: return StringView("BKSL"); break;
	case InputKeyCode::RIGHT_BRACKET: return StringView("AD12"); break;
	case InputKeyCode::GRAVE_ACCENT: return StringView("TLDE"); break;

		/* Function keys */
	case InputKeyCode::F1: return StringView("FK01"); break;
	case InputKeyCode::F2: return StringView("FK02"); break;
	case InputKeyCode::F3: return StringView("FK03"); break;
	case InputKeyCode::F4: return StringView("FK04"); break;
	case InputKeyCode::F5: return StringView("FK05"); break;
	case InputKeyCode::F6: return StringView("FK06"); break;
	case InputKeyCode::F7: return StringView("FK07"); break;
	case InputKeyCode::F8: return StringView("FK08"); break;
	case InputKeyCode::F9: return StringView("FK09"); break;
	case InputKeyCode::F10: return StringView("FK10"); break;
	case InputKeyCode::F11: return StringView("FK11"); break;
	case InputKeyCode::F12: return StringView("FK12"); break;
	case InputKeyCode::F13: return StringView("FK13"); break;
	case InputKeyCode::F14: return StringView("FK14"); break;
	case InputKeyCode::F15: return StringView("FK15"); break;
	case InputKeyCode::F16: return StringView("FK16"); break;
	case InputKeyCode::F17: return StringView("FK17"); break;
	case InputKeyCode::F18: return StringView("FK18"); break;
	case InputKeyCode::F19: return StringView("FK19"); break;
	case InputKeyCode::F20: return StringView("FK20"); break;
	case InputKeyCode::F21: return StringView("FK21"); break;
	case InputKeyCode::F22: return StringView("FK22"); break;
	case InputKeyCode::F23: return StringView("FK23"); break;
	case InputKeyCode::F24: return StringView("FK24"); break;
	case InputKeyCode::F25: return StringView("FK25"); break;

	case InputKeyCode::MENU: return StringView("MENU"); break;
	case InputKeyCode::PRINT_SCREEN: return StringView("PRSC"); break;
	case InputKeyCode::PAUSE: return StringView("PAUS"); break;
	case InputKeyCode::DELETE: return StringView("DELE"); break;
	default: break;
	}
	return StringView();
}

std::ostream &operator<<(std::ostream &stream, InputKeyCode code) {
	stream << "InputKeyCode(" << toInt(code) << ", "
			<< getInputKeyCodeName(code) << ", " << getInputKeyCodeKeyName(code) << ")";
	return stream;
}

}

namespace stappler::xenolith::profiling {

ProfileData begin(StringView tag, StringView variant, uint64_t limit) {
	ProfileData ret;
	ret.tag = tag;
	ret.variant = variant;
	ret.limit = limit;
	ret.timestamp = platform::device::_clock(platform::device::Thread);
	return ret;
}

void end(ProfileData &data) {
	if (!data.limit) {
		return;
	}

	auto dt = platform::device::_clock(platform::device::Thread) - data.timestamp;
	if (dt > data.limit) {
		log::vtext("Profiling", "[", data.tag , "] [", data.variant, "] limit exceeded: ", dt, " (from ", data.limit, ")");
		store(data);
	}
}

void store(ProfileData &data) {

}

}
