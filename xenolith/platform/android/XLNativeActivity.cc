/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "XLPlatform.h"

#if ANDROID

#include "XLPlatformAndroid.h"
#include <sys/timerfd.h>

namespace stappler::xenolith::platform {

/* AKEYCODE_BACK mapped to ESC
 * AKEYCODE_FORWARD - ENTER
 * AKEYCODE_DPAD_* mapped to arrows, AKEYCODE_DPAD_CENTER to Enter
 * AKEYCODE_SYM - WORLD_1
 * AKEYCODE_SWITCH_CHARSET - WORLD_2
 * AKEYCODE_DEL - BACKSPACE
 *
 * === Unknown (can be used with platform-depended keycodes in event) ===
 * AKEYCODE_CALL
 * AKEYCODE_ENDCALL
 * AKEYCODE_STAR
 * AKEYCODE_POUND
 * AKEYCODE_VOLUME_UP
 * AKEYCODE_VOLUME_DOWN
 * AKEYCODE_POWER
 * AKEYCODE_CAMERA
 * AKEYCODE_CLEAR
 * AKEYCODE_EXPLORER
 * AKEYCODE_ENVELOPE
 * AKEYCODE_AT
 * AKEYCODE_NUM
 * AKEYCODE_HEADSETHOOK
 * AKEYCODE_FOCUS
 * AKEYCODE_PLUS
 * AKEYCODE_NOTIFICATION
 * AKEYCODE_SEARCH
 * AKEYCODE_MEDIA_PLAY_PAUSE
 * AKEYCODE_MEDIA_STOP
 * AKEYCODE_MEDIA_NEXT
 * AKEYCODE_MEDIA_PREVIOUS
 * AKEYCODE_MEDIA_REWIND
 * AKEYCODE_MEDIA_FAST_FORWARD
 * AKEYCODE_MUTE
 * AKEYCODE_PICTSYMBOLS
 * AKEYCODE_BUTTON_A
 * AKEYCODE_BUTTON_B
 * AKEYCODE_BUTTON_C
 * AKEYCODE_BUTTON_X
 * AKEYCODE_BUTTON_Y
 * AKEYCODE_BUTTON_Z
 * AKEYCODE_BUTTON_L1
 * AKEYCODE_BUTTON_R1
 * AKEYCODE_BUTTON_L2
 * AKEYCODE_BUTTON_R2
 * AKEYCODE_BUTTON_THUMBL
 * AKEYCODE_BUTTON_THUMBR
 * AKEYCODE_BUTTON_START
 * AKEYCODE_BUTTON_SELECT
 * AKEYCODE_BUTTON_MODE
 * AKEYCODE_FUNCTION
 * AKEYCODE_MEDIA_PLAY
 * AKEYCODE_MEDIA_PAUSE
 * AKEYCODE_MEDIA_CLOSE
 * AKEYCODE_MEDIA_EJECT
 * AKEYCODE_MEDIA_RECORD
 * AKEYCODE_NUMPAD_DOT
 * AKEYCODE_NUMPAD_COMMA
 * AKEYCODE_NUMPAD_LEFT_PAREN
 * AKEYCODE_NUMPAD_RIGHT_PAREN
 * AKEYCODE_VOLUME_MUTE
 * AKEYCODE_INFO
 * AKEYCODE_CHANNEL_UP
 * AKEYCODE_CHANNEL_DOWN
 * AKEYCODE_ZOOM_IN
 * AKEYCODE_ZOOM_OUT
 * AKEYCODE_TV
 * AKEYCODE_WINDOW
 * AKEYCODE_GUIDE
 * AKEYCODE_DVR
 * AKEYCODE_BOOKMARK
 * AKEYCODE_CAPTIONS
 * AKEYCODE_SETTINGS
 * AKEYCODE_TV_POWER
 * AKEYCODE_TV_INPUT
 * AKEYCODE_STB_POWER
 * AKEYCODE_STB_INPUT
 * AKEYCODE_AVR_POWER
 * AKEYCODE_AVR_INPUT
 * AKEYCODE_PROG_RED
 * AKEYCODE_PROG_GREEN
 * AKEYCODE_PROG_YELLOW
 * AKEYCODE_PROG_BLUE
 * AKEYCODE_LANGUAGE_SWITCH
 * AKEYCODE_MANNER_MODE
 * AKEYCODE_3D_MODE
 * AKEYCODE_CONTACTS
 * AKEYCODE_CALENDAR
 * AKEYCODE_MUSIC
 * AKEYCODE_CALCULATOR
 * AKEYCODE_ZENKAKU_HANKAKU
 * AKEYCODE_EISU
 * AKEYCODE_MUHENKAN
 * AKEYCODE_HENKAN
 * AKEYCODE_KATAKANA_HIRAGANA
 * AKEYCODE_YEN
 * AKEYCODE_RO
 * AKEYCODE_KANA
 * AKEYCODE_ASSIST
 * AKEYCODE_BRIGHTNESS_DOWN
 * AKEYCODE_BRIGHTNESS_UP
 * AKEYCODE_MEDIA_AUDIO_TRACK
 * AKEYCODE_SLEEP
 * AKEYCODE_WAKEUP
 * AKEYCODE_PAIRING
 * AKEYCODE_MEDIA_TOP_MENU
 * AKEYCODE_11
 * AKEYCODE_12
 * AKEYCODE_LAST_CHANNEL
 * AKEYCODE_TV_DATA_SERVICE
 * AKEYCODE_VOICE_ASSIST
 * AKEYCODE_TV_RADIO_SERVICE
 * AKEYCODE_TV_TELETEXT
 * AKEYCODE_TV_NUMBER_ENTRY
 * AKEYCODE_TV_TERRESTRIAL_ANALOG
 * AKEYCODE_TV_TERRESTRIAL_DIGITAL
 * AKEYCODE_TV_SATELLITE
 * AKEYCODE_TV_SATELLITE_BS
 * AKEYCODE_TV_SATELLITE_CS
 * AKEYCODE_TV_SATELLITE_SERVICE
 * AKEYCODE_TV_NETWORK
 * AKEYCODE_TV_ANTENNA_CABLE
 * AKEYCODE_TV_INPUT_HDMI_1
 * AKEYCODE_TV_INPUT_HDMI_2
 * AKEYCODE_TV_INPUT_HDMI_3
 * AKEYCODE_TV_INPUT_HDMI_4
 * AKEYCODE_TV_INPUT_COMPOSITE_1
 * AKEYCODE_TV_INPUT_COMPOSITE_2
 * AKEYCODE_TV_INPUT_COMPONENT_1
 * AKEYCODE_TV_INPUT_COMPONENT_2
 * AKEYCODE_TV_INPUT_VGA_1
 * AKEYCODE_TV_AUDIO_DESCRIPTION
 * AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_UP
 * AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_DOWN
 * AKEYCODE_TV_ZOOM_MODE
 * AKEYCODE_TV_CONTENTS_MENU
 * AKEYCODE_TV_MEDIA_CONTEXT_MENU
 * AKEYCODE_TV_TIMER_PROGRAMMING
 * AKEYCODE_HELP
 * AKEYCODE_NAVIGATE_PREVIOUS
 * AKEYCODE_NAVIGATE_NEXT
 * AKEYCODE_NAVIGATE_IN
 * AKEYCODE_NAVIGATE_OUT
 * AKEYCODE_STEM_PRIMARY
 * AKEYCODE_STEM_1
 * AKEYCODE_STEM_2
 * AKEYCODE_STEM_3
 * AKEYCODE_DPAD_UP_LEFT
 * AKEYCODE_DPAD_DOWN_LEFT
 * AKEYCODE_DPAD_UP_RIGHT
 * AKEYCODE_DPAD_DOWN_RIGHT
 * AKEYCODE_MEDIA_SKIP_FORWARD
 * AKEYCODE_MEDIA_SKIP_BACKWARD
 * AKEYCODE_MEDIA_STEP_FORWARD
 * AKEYCODE_MEDIA_STEP_BACKWARD
 * AKEYCODE_SOFT_SLEEP
 * AKEYCODE_CUT
 * AKEYCODE_COPY
 * AKEYCODE_PASTE
 * AKEYCODE_SYSTEM_NAVIGATION_UP
 * AKEYCODE_SYSTEM_NAVIGATION_DOWN
 * AKEYCODE_SYSTEM_NAVIGATION_LEFT
 * AKEYCODE_SYSTEM_NAVIGATION_RIGHT
 * AKEYCODE_ALL_APPS
 * AKEYCODE_REFRESH
 * AKEYCODE_THUMBS_UP
 * AKEYCODE_THUMBS_DOWN
 * AKEYCODE_PROFILE_SWITCH
 */

static InputKeyCode s_keycodes[] = {
	InputKeyCode::Unknown, // AKEYCODE_UNKNOWN
	InputKeyCode::LEFT, // AKEYCODE_SOFT_LEFT
	InputKeyCode::RIGHT, // AKEYCODE_SOFT_RIGHT
	InputKeyCode::HOME, // AKEYCODE_HOME
	InputKeyCode::ESCAPE, // AKEYCODE_BACK
	InputKeyCode::Unknown, // AKEYCODE_CALL
	InputKeyCode::Unknown, // AKEYCODE_ENDCALL
    InputKeyCode::_0, // AKEYCODE_0
    InputKeyCode::_1, // AKEYCODE_1
    InputKeyCode::_2, // AKEYCODE_2
    InputKeyCode::_3, // AKEYCODE_3
    InputKeyCode::_4, // AKEYCODE_4
    InputKeyCode::_5, // AKEYCODE_5
    InputKeyCode::_6, // AKEYCODE_6
    InputKeyCode::_7, // AKEYCODE_7
    InputKeyCode::_8, // AKEYCODE_8
    InputKeyCode::_9, // AKEYCODE_9
    InputKeyCode::Unknown, // AKEYCODE_STAR
    InputKeyCode::Unknown, // AKEYCODE_POUND
    InputKeyCode::UP, // AKEYCODE_DPAD_UP
    InputKeyCode::DOWN, // AKEYCODE_DPAD_DOWN
    InputKeyCode::LEFT, // AKEYCODE_DPAD_LEFT
    InputKeyCode::RIGHT, // AKEYCODE_DPAD_RIGHT
    InputKeyCode::ENTER, // AKEYCODE_DPAD_CENTER
    InputKeyCode::Unknown, // AKEYCODE_VOLUME_UP
    InputKeyCode::Unknown, // AKEYCODE_VOLUME_DOWN
    InputKeyCode::Unknown, // AKEYCODE_POWER
    InputKeyCode::Unknown, // AKEYCODE_CAMERA
    InputKeyCode::Unknown, // AKEYCODE_CLEAR
    InputKeyCode::A, // AKEYCODE_A
    InputKeyCode::B, // AKEYCODE_B
    InputKeyCode::C, // AKEYCODE_C
    InputKeyCode::D, // AKEYCODE_D
    InputKeyCode::E, // AKEYCODE_E
    InputKeyCode::F, // AKEYCODE_F
    InputKeyCode::G, // AKEYCODE_G
    InputKeyCode::H, // AKEYCODE_H
    InputKeyCode::I, // AKEYCODE_I
    InputKeyCode::J, // AKEYCODE_J
    InputKeyCode::K, // AKEYCODE_K
    InputKeyCode::L, // AKEYCODE_L
    InputKeyCode::M, // AKEYCODE_M
    InputKeyCode::N, // AKEYCODE_N
    InputKeyCode::O, // AKEYCODE_O
    InputKeyCode::P, // AKEYCODE_P
    InputKeyCode::Q, // AKEYCODE_Q
    InputKeyCode::R, // AKEYCODE_R
    InputKeyCode::S, // AKEYCODE_S
    InputKeyCode::T, // AKEYCODE_T
    InputKeyCode::U, // AKEYCODE_U
    InputKeyCode::V, // AKEYCODE_V
    InputKeyCode::W, // AKEYCODE_W
    InputKeyCode::X, // AKEYCODE_X
    InputKeyCode::Y, // AKEYCODE_Y
    InputKeyCode::Z, // AKEYCODE_Z
    InputKeyCode::COMMA, // AKEYCODE_COMMA
    InputKeyCode::PERIOD, // AKEYCODE_PERIOD
    InputKeyCode::LEFT_ALT, // AKEYCODE_ALT_LEFT
    InputKeyCode::RIGHT_ALT, // AKEYCODE_ALT_RIGHT
    InputKeyCode::LEFT_SHIFT, // AKEYCODE_SHIFT_LEFT
    InputKeyCode::RIGHT_SHIFT, // AKEYCODE_SHIFT_RIGHT
    InputKeyCode::TAB, // AKEYCODE_TAB
    InputKeyCode::SPACE, // AKEYCODE_SPACE
    InputKeyCode::WORLD_1, // AKEYCODE_SYM
    InputKeyCode::Unknown, // AKEYCODE_EXPLORER
    InputKeyCode::Unknown, // AKEYCODE_ENVELOPE
    InputKeyCode::ENTER, // AKEYCODE_ENTER
    InputKeyCode::BACKSPACE, // AKEYCODE_DEL
    InputKeyCode::GRAVE_ACCENT, // AKEYCODE_GRAVE
    InputKeyCode::MINUS, // AKEYCODE_MINUS
    InputKeyCode::EQUAL, // AKEYCODE_EQUALS
    InputKeyCode::LEFT_BRACKET, // AKEYCODE_LEFT_BRACKET
    InputKeyCode::RIGHT_BRACKET, // AKEYCODE_RIGHT_BRACKET
    InputKeyCode::BACKSLASH, // AKEYCODE_BACKSLASH
    InputKeyCode::SEMICOLON, // AKEYCODE_SEMICOLON
    InputKeyCode::APOSTROPHE, // AKEYCODE_APOSTROPHE
    InputKeyCode::SLASH, // AKEYCODE_SLASH
    InputKeyCode::Unknown, // AKEYCODE_AT
    InputKeyCode::Unknown, // AKEYCODE_NUM
    InputKeyCode::Unknown, // AKEYCODE_HEADSETHOOK
    InputKeyCode::Unknown, // AKEYCODE_FOCUS
    InputKeyCode::Unknown, // AKEYCODE_PLUS
    InputKeyCode::MENU, // AKEYCODE_MENU
    InputKeyCode::Unknown, // AKEYCODE_NOTIFICATION
    InputKeyCode::Unknown, // AKEYCODE_SEARCH
    InputKeyCode::Unknown, // AKEYCODE_MEDIA_PLAY_PAUSE
    InputKeyCode::Unknown, // AKEYCODE_MEDIA_STOP
    InputKeyCode::Unknown, // AKEYCODE_MEDIA_NEXT
    InputKeyCode::Unknown, // AKEYCODE_MEDIA_PREVIOUS
    InputKeyCode::Unknown, // AKEYCODE_MEDIA_REWIND
    InputKeyCode::Unknown, // AKEYCODE_MEDIA_FAST_FORWARD
    InputKeyCode::Unknown, // AKEYCODE_MUTE
    InputKeyCode::PAGE_UP, // AKEYCODE_PAGE_UP
    InputKeyCode::PAGE_DOWN, // AKEYCODE_PAGE_DOWN
    InputKeyCode::Unknown, // AKEYCODE_PICTSYMBOLS
    InputKeyCode::WORLD_2, // AKEYCODE_SWITCH_CHARSET
    InputKeyCode::Unknown, // AKEYCODE_BUTTON_A
    InputKeyCode::Unknown, // AKEYCODE_BUTTON_B
    InputKeyCode::Unknown, // AKEYCODE_BUTTON_C
    InputKeyCode::Unknown, // AKEYCODE_BUTTON_X
    InputKeyCode::Unknown, // AKEYCODE_BUTTON_Y
    InputKeyCode::Unknown, // AKEYCODE_BUTTON_Z
    InputKeyCode::Unknown, // AKEYCODE_BUTTON_L1
    InputKeyCode::Unknown, // AKEYCODE_BUTTON_R1
    InputKeyCode::Unknown, // AKEYCODE_BUTTON_L2
    InputKeyCode::Unknown, // AKEYCODE_BUTTON_R2
    InputKeyCode::Unknown, // AKEYCODE_BUTTON_THUMBL
    InputKeyCode::Unknown, // AKEYCODE_BUTTON_THUMBR
    InputKeyCode::Unknown, // AKEYCODE_BUTTON_START
    InputKeyCode::Unknown, // AKEYCODE_BUTTON_SELECT
    InputKeyCode::Unknown, // AKEYCODE_BUTTON_MODE
    InputKeyCode::ESCAPE, // AKEYCODE_ESCAPE
    InputKeyCode::DELETE, // AKEYCODE_FORWARD_DEL
    InputKeyCode::LEFT_CONTROL, // AKEYCODE_CTRL_LEFT
    InputKeyCode::RIGHT_CONTROL, // AKEYCODE_CTRL_RIGHT
    InputKeyCode::CAPS_LOCK, // AKEYCODE_CAPS_LOCK
    InputKeyCode::SCROLL_LOCK, // AKEYCODE_SCROLL_LOCK
    InputKeyCode::LEFT_SUPER, // AKEYCODE_META_LEFT
    InputKeyCode::RIGHT_SUPER, // AKEYCODE_META_RIGHT
    InputKeyCode::Unknown, // AKEYCODE_FUNCTION
    InputKeyCode::PRINT_SCREEN, // AKEYCODE_SYSRQ
    InputKeyCode::PAUSE, // AKEYCODE_BREAK
    InputKeyCode::HOME, // AKEYCODE_MOVE_HOME
    InputKeyCode::END, // AKEYCODE_MOVE_END
    InputKeyCode::INSERT, // AKEYCODE_INSERT
    InputKeyCode::ENTER, // AKEYCODE_FORWARD
    InputKeyCode::Unknown, // AKEYCODE_MEDIA_PLAY
    InputKeyCode::Unknown, // AKEYCODE_MEDIA_PAUSE
    InputKeyCode::Unknown, // AKEYCODE_MEDIA_CLOSE
    InputKeyCode::Unknown, // AKEYCODE_MEDIA_EJECT
    InputKeyCode::Unknown, // AKEYCODE_MEDIA_RECORD
    InputKeyCode::F1, // AKEYCODE_F1
    InputKeyCode::F2, // AKEYCODE_F2
    InputKeyCode::F3, // AKEYCODE_F3
    InputKeyCode::F4, // AKEYCODE_F4
    InputKeyCode::F5, // AKEYCODE_F5
    InputKeyCode::F6, // AKEYCODE_F6
    InputKeyCode::F7, // AKEYCODE_F7
    InputKeyCode::F8, // AKEYCODE_F8
    InputKeyCode::F9, // AKEYCODE_F9
    InputKeyCode::F10, // AKEYCODE_F10
    InputKeyCode::F11, // AKEYCODE_F11
    InputKeyCode::F12, // AKEYCODE_F12
    InputKeyCode::NUM_LOCK, // AKEYCODE_NUM_LOCK
    InputKeyCode::KP_0, // AKEYCODE_NUMPAD_0
    InputKeyCode::KP_1, // AKEYCODE_NUMPAD_1
    InputKeyCode::KP_2, // AKEYCODE_NUMPAD_2
    InputKeyCode::KP_3, // AKEYCODE_NUMPAD_3
    InputKeyCode::KP_4, // AKEYCODE_NUMPAD_4
    InputKeyCode::KP_5, // AKEYCODE_NUMPAD_5
    InputKeyCode::KP_6, // AKEYCODE_NUMPAD_6
    InputKeyCode::KP_7, // AKEYCODE_NUMPAD_7
    InputKeyCode::KP_8, // AKEYCODE_NUMPAD_8
    InputKeyCode::KP_9, // AKEYCODE_NUMPAD_9
    InputKeyCode::KP_DIVIDE, // AKEYCODE_NUMPAD_DIVIDE
    InputKeyCode::KP_MULTIPLY, // AKEYCODE_NUMPAD_MULTIPLY
    InputKeyCode::KP_SUBTRACT, // AKEYCODE_NUMPAD_SUBTRACT
    InputKeyCode::KP_ADD, // AKEYCODE_NUMPAD_ADD
    InputKeyCode::Unknown, // AKEYCODE_NUMPAD_DOT
    InputKeyCode::Unknown, // AKEYCODE_NUMPAD_COMMA
    InputKeyCode::KP_ENTER, // AKEYCODE_NUMPAD_ENTER
    InputKeyCode::KP_EQUAL, // AKEYCODE_NUMPAD_EQUALS
    InputKeyCode::Unknown, // AKEYCODE_NUMPAD_LEFT_PAREN
    InputKeyCode::Unknown, // AKEYCODE_NUMPAD_RIGHT_PAREN
    InputKeyCode::Unknown, // AKEYCODE_VOLUME_MUTE
    InputKeyCode::Unknown, // AKEYCODE_INFO
    InputKeyCode::Unknown, // AKEYCODE_CHANNEL_UP
    InputKeyCode::Unknown, // AKEYCODE_CHANNEL_DOWN
    InputKeyCode::Unknown, // AKEYCODE_ZOOM_IN
    InputKeyCode::Unknown, // AKEYCODE_ZOOM_OUT
    InputKeyCode::Unknown, // AKEYCODE_TV
    InputKeyCode::Unknown, // AKEYCODE_WINDOW
    InputKeyCode::Unknown, // AKEYCODE_GUIDE
    InputKeyCode::Unknown, // AKEYCODE_DVR
    InputKeyCode::Unknown, // AKEYCODE_BOOKMARK
    InputKeyCode::Unknown, // AKEYCODE_CAPTIONS
    InputKeyCode::Unknown, // AKEYCODE_SETTINGS
    InputKeyCode::Unknown, // AKEYCODE_TV_POWER
    InputKeyCode::Unknown, // AKEYCODE_TV_INPUT
    InputKeyCode::Unknown, // AKEYCODE_STB_POWER
    InputKeyCode::Unknown, // AKEYCODE_STB_INPUT
    InputKeyCode::Unknown, // AKEYCODE_AVR_POWER
    InputKeyCode::Unknown, // AKEYCODE_AVR_INPUT
    InputKeyCode::Unknown, // AKEYCODE_PROG_RED
    InputKeyCode::Unknown, // AKEYCODE_PROG_GREEN
    InputKeyCode::Unknown, // AKEYCODE_PROG_YELLOW
    InputKeyCode::Unknown, // AKEYCODE_PROG_BLUE
    InputKeyCode::Unknown, // AKEYCODE_APP_SWITCH
    InputKeyCode::F1, // AKEYCODE_BUTTON_1
    InputKeyCode::F2, // AKEYCODE_BUTTON_2
    InputKeyCode::F3, // AKEYCODE_BUTTON_3
    InputKeyCode::F4, // AKEYCODE_BUTTON_4
    InputKeyCode::F5, // AKEYCODE_BUTTON_5
    InputKeyCode::F6, // AKEYCODE_BUTTON_6
    InputKeyCode::F7, // AKEYCODE_BUTTON_7
    InputKeyCode::F8, // AKEYCODE_BUTTON_8
    InputKeyCode::F9, // AKEYCODE_BUTTON_9
    InputKeyCode::F10, // AKEYCODE_BUTTON_10
    InputKeyCode::F11, // AKEYCODE_BUTTON_11
    InputKeyCode::F12, // AKEYCODE_BUTTON_12
    InputKeyCode::F13, // AKEYCODE_BUTTON_13
    InputKeyCode::F14, // AKEYCODE_BUTTON_14
    InputKeyCode::F15, // AKEYCODE_BUTTON_15
    InputKeyCode::F16, // AKEYCODE_BUTTON_16
    InputKeyCode::Unknown, // AKEYCODE_LANGUAGE_SWITCH
    InputKeyCode::Unknown, // AKEYCODE_MANNER_MODE
    InputKeyCode::Unknown, // AKEYCODE_3D_MODE
    InputKeyCode::Unknown, // AKEYCODE_CONTACTS
    InputKeyCode::Unknown, // AKEYCODE_CALENDAR
    InputKeyCode::Unknown, // AKEYCODE_MUSIC
    InputKeyCode::Unknown, // AKEYCODE_CALCULATOR
    InputKeyCode::Unknown, // AKEYCODE_ZENKAKU_HANKAKU
    InputKeyCode::Unknown, // AKEYCODE_EISU
    InputKeyCode::Unknown, // AKEYCODE_MUHENKAN
    InputKeyCode::Unknown, // AKEYCODE_HENKAN
    InputKeyCode::Unknown, // AKEYCODE_KATAKANA_HIRAGANA
    InputKeyCode::Unknown, // AKEYCODE_YEN
    InputKeyCode::Unknown, // AKEYCODE_RO
    InputKeyCode::Unknown, // AKEYCODE_KANA
    InputKeyCode::Unknown, // AKEYCODE_ASSIST
    InputKeyCode::Unknown, // AKEYCODE_BRIGHTNESS_DOWN
    InputKeyCode::Unknown, // AKEYCODE_BRIGHTNESS_UP
    InputKeyCode::Unknown, // AKEYCODE_MEDIA_AUDIO_TRACK
    InputKeyCode::Unknown, // AKEYCODE_SLEEP
    InputKeyCode::Unknown, // AKEYCODE_WAKEUP
    InputKeyCode::Unknown, // AKEYCODE_PAIRING
    InputKeyCode::Unknown, // AKEYCODE_MEDIA_TOP_MENU
    InputKeyCode::Unknown, // AKEYCODE_11
    InputKeyCode::Unknown, // AKEYCODE_12
    InputKeyCode::Unknown, // AKEYCODE_LAST_CHANNEL
    InputKeyCode::Unknown, // AKEYCODE_TV_DATA_SERVICE
    InputKeyCode::Unknown, // AKEYCODE_VOICE_ASSIST
    InputKeyCode::Unknown, // AKEYCODE_TV_RADIO_SERVICE
    InputKeyCode::Unknown, // AKEYCODE_TV_TELETEXT
    InputKeyCode::Unknown, // AKEYCODE_TV_NUMBER_ENTRY
    InputKeyCode::Unknown, // AKEYCODE_TV_TERRESTRIAL_ANALOG
    InputKeyCode::Unknown, // AKEYCODE_TV_TERRESTRIAL_DIGITAL
    InputKeyCode::Unknown, // AKEYCODE_TV_SATELLITE
    InputKeyCode::Unknown, // AKEYCODE_TV_SATELLITE_BS
    InputKeyCode::Unknown, // AKEYCODE_TV_SATELLITE_CS
    InputKeyCode::Unknown, // AKEYCODE_TV_SATELLITE_SERVICE
    InputKeyCode::Unknown, // AKEYCODE_TV_NETWORK
    InputKeyCode::Unknown, // AKEYCODE_TV_ANTENNA_CABLE
    InputKeyCode::Unknown, // AKEYCODE_TV_INPUT_HDMI_1
    InputKeyCode::Unknown, // AKEYCODE_TV_INPUT_HDMI_2
    InputKeyCode::Unknown, // AKEYCODE_TV_INPUT_HDMI_3
    InputKeyCode::Unknown, // AKEYCODE_TV_INPUT_HDMI_4
    InputKeyCode::Unknown, // AKEYCODE_TV_INPUT_COMPOSITE_1
    InputKeyCode::Unknown, // AKEYCODE_TV_INPUT_COMPOSITE_2
    InputKeyCode::Unknown, // AKEYCODE_TV_INPUT_COMPONENT_1
    InputKeyCode::Unknown, // AKEYCODE_TV_INPUT_COMPONENT_2
    InputKeyCode::Unknown, // AKEYCODE_TV_INPUT_VGA_1
    InputKeyCode::Unknown, // AKEYCODE_TV_AUDIO_DESCRIPTION
    InputKeyCode::Unknown, // AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_UP
    InputKeyCode::Unknown, // AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_DOWN
    InputKeyCode::Unknown, // AKEYCODE_TV_ZOOM_MODE
    InputKeyCode::Unknown, // AKEYCODE_TV_CONTENTS_MENU
    InputKeyCode::Unknown, // AKEYCODE_TV_MEDIA_CONTEXT_MENU
    InputKeyCode::Unknown, // AKEYCODE_TV_TIMER_PROGRAMMING
    InputKeyCode::F1, // AKEYCODE_HELP
    InputKeyCode::Unknown, // AKEYCODE_NAVIGATE_PREVIOUS
    InputKeyCode::Unknown, // AKEYCODE_NAVIGATE_NEXT
    InputKeyCode::Unknown, // AKEYCODE_NAVIGATE_IN
    InputKeyCode::Unknown, // AKEYCODE_NAVIGATE_OUT
    InputKeyCode::Unknown, // AKEYCODE_STEM_PRIMARY
    InputKeyCode::Unknown, // AKEYCODE_STEM_1
    InputKeyCode::Unknown, // AKEYCODE_STEM_2
    InputKeyCode::Unknown, // AKEYCODE_STEM_3
    InputKeyCode::Unknown, // AKEYCODE_DPAD_UP_LEFT
    InputKeyCode::Unknown, // AKEYCODE_DPAD_DOWN_LEFT
    InputKeyCode::Unknown, // AKEYCODE_DPAD_UP_RIGHT
    InputKeyCode::Unknown, // AKEYCODE_DPAD_DOWN_RIGHT
    InputKeyCode::Unknown, // AKEYCODE_MEDIA_SKIP_FORWARD
    InputKeyCode::Unknown, // AKEYCODE_MEDIA_SKIP_BACKWARD
    InputKeyCode::Unknown, // AKEYCODE_MEDIA_STEP_FORWARD
    InputKeyCode::Unknown, // AKEYCODE_MEDIA_STEP_BACKWARD
    InputKeyCode::Unknown, // AKEYCODE_SOFT_SLEEP
    InputKeyCode::Unknown, // AKEYCODE_CUT
    InputKeyCode::Unknown, // AKEYCODE_COPY
    InputKeyCode::Unknown, // AKEYCODE_PASTE
    InputKeyCode::Unknown, // AKEYCODE_SYSTEM_NAVIGATION_UP
    InputKeyCode::Unknown, // AKEYCODE_SYSTEM_NAVIGATION_DOWN
    InputKeyCode::Unknown, // AKEYCODE_SYSTEM_NAVIGATION_LEFT
    InputKeyCode::Unknown, // AKEYCODE_SYSTEM_NAVIGATION_RIGHT
    InputKeyCode::Unknown, // AKEYCODE_ALL_APPS
    InputKeyCode::Unknown, // AKEYCODE_REFRESH
    InputKeyCode::Unknown, // AKEYCODE_THUMBS_UP
    InputKeyCode::Unknown, // AKEYCODE_THUMBS_DOWN
    InputKeyCode::Unknown, // AKEYCODE_PROFILE_SWITCH
	InputKeyCode::Unknown,
	InputKeyCode::Unknown,
	InputKeyCode::Unknown,
	InputKeyCode::Unknown,
	InputKeyCode::Unknown,
	InputKeyCode::Unknown,
	InputKeyCode::Unknown,
	InputKeyCode::Unknown,
	InputKeyCode::Unknown,
	InputKeyCode::Unknown,
	InputKeyCode::Unknown,
	InputKeyCode::Unknown,
	InputKeyCode::Unknown,
};



EngineMainThread::~EngineMainThread() {
	_application->end();
	_thread.join();
}

bool EngineMainThread::init(Application *app, Value &&args) {
	_application = app;
	_thread = std::thread(EngineMainThread::workerThread, this, nullptr);
	_args = move(args);
	return true;
}

void EngineMainThread::threadInit() {
	_threadId = std::this_thread::get_id();
}

void EngineMainThread::threadDispose() {

}

bool EngineMainThread::worker() {
	_application->run(move(_args), [&] (Application *app) {
		_running.store(true);
		std::unique_lock lock(_runningMutex);
		_runningVar.notify_all();
	});
	return false;
}

void EngineMainThread::waitForRunning() {
	if (_running.load()) {
		return;
	}

	std::unique_lock lock(_runningMutex);
	_runningVar.wait(lock, [&] {
		return _running.load();
	});
}

static void onStart(ANativeActivity* activity) {
	stappler::log::format("NativeActivity", "Start: %p", activity);
}

static void onResume(ANativeActivity* activity) {
	stappler::log::format("NativeActivity", "Resume: %p", activity);
}

static void* onSaveInstanceState(ANativeActivity* activity, size_t* outLen) {
	stappler::log::format("NativeActivity", "SaveInstanceState: %p", activity);
	return nullptr;
}

static void onPause(ANativeActivity* activity) {
	stappler::log::format("NativeActivity", "Pause: %p", activity);
}

static void onStop(ANativeActivity* activity) {
	stappler::log::format("NativeActivity", "Stop: %p", activity);
}

static void onContentRectChanged(ANativeActivity* activity, const ARect* r) {
	stappler::log::format("NativeActivity", "ContentRectChanged: l=%d,t=%d,r=%d,b=%d", r->left, r->top, r->right, r->bottom);
}

static void onLowMemory(ANativeActivity* activity) {
	stappler::log::format("NativeActivity", "LowMemory: %p", activity);
}

static void onWindowFocusChanged(ANativeActivity* activity, int focused) {
	stappler::log::format("NativeActivity", "WindowFocusChanged: %p -- %d", activity, focused);
}

static void onNativeWindowResized(ANativeActivity* activity, ANativeWindow* window) {
	stappler::log::format("NativeActivity", "NativeWindowResized: %p -- %p", activity, window);
}

static void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue) {
	stappler::log::format("NativeActivity", "InputQueueCreated: %p -- %p", activity, queue);
}

static void onInputQueueDestroyed(ANativeActivity* activity, AInputQueue* queue) {
	stappler::log::format("NativeActivity", "InputQueueDestroyed: %p -- %p", activity, queue);
}

NativeActivity::~NativeActivity() {
	filesystem::platform::Android_terminateFilesystem();
	auto app = thread->getApplication();
	app->end(true);

	thread->getApplication()->setNativeHandle(nullptr);
	thread = nullptr;

    if (rootView) {
        rootView->threadDispose();
        rootView = nullptr;
    }

	if (looper) {
		if (_eventfd >= 0) {
			ALooper_removeFd(looper, _eventfd);
		}
		if (_timerfd >= 0) {
			ALooper_removeFd(looper, _timerfd);
		}
		ALooper_release(looper);
		looper = nullptr;
	}
	if (config) {
		AConfiguration_delete(config);
		config = nullptr;
	}
	if (_eventfd) {
		::close(_eventfd);
		_eventfd = -1;
	}
	if (_timerfd) {
		::close(_timerfd);
		_timerfd = -1;
	}
}

bool NativeActivity::init(ANativeActivity *a) {
	activity = a;
	config = AConfiguration_new();
	AConfiguration_fromAssetManager(config, activity->assetManager);

	looper = ALooper_forThread();
	if (looper) {
		ALooper_acquire(looper);

		_eventfd = ::eventfd(0, EFD_NONBLOCK);

		ALooper_addFd(looper, _eventfd, ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT,
				[] (int fd, int events, void* data) -> int {
			return ((NativeActivity *)data)->handleLooperEvent(fd, events);
		}, this);

		struct itimerspec timer{
			{ 0, config::PresentationSchedulerInterval * 1000 },
			{ 0, config::PresentationSchedulerInterval * 1000 }
		};

		_timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
		::timerfd_settime(_timerfd, 0, &timer, nullptr);

		ALooper_addFd(looper, _timerfd, ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT,
				[] (int fd, int events, void* data) -> int {
			return ((NativeActivity *)data)->handleLooperEvent(fd, events);
		}, this);
	}

	activity->callbacks->onConfigurationChanged = [] (ANativeActivity* activity) {
		((NativeActivity *)activity->instance)->handleConfigurationChanged();
	};

	activity->callbacks->onContentRectChanged = onContentRectChanged;

	activity->callbacks->onDestroy = [] (ANativeActivity* activity) {
		stappler::log::format("NativeActivity", "Destroy: %p", activity);

		auto ref = (NativeActivity *)activity->instance;
		delete ref;
	};
	activity->callbacks->onInputQueueCreated = [] (ANativeActivity* activity, AInputQueue* queue) {
		((NativeActivity *)activity->instance)->handleInputQueueCreated(queue);
	};
	activity->callbacks->onInputQueueDestroyed = [] (ANativeActivity* activity, AInputQueue* queue) {
		((NativeActivity *)activity->instance)->handleInputQueueDestroyed(queue);
	};
	activity->callbacks->onLowMemory = onLowMemory;

	activity->callbacks->onNativeWindowCreated = [] (ANativeActivity* activity, ANativeWindow* window) {
		((NativeActivity *)activity->instance)->handleNativeWindowCreated(window);
	};

	activity->callbacks->onNativeWindowDestroyed = [] (ANativeActivity* activity, ANativeWindow* window) {
		((NativeActivity *)activity->instance)->handleNativeWindowDestroyed(window);
	};

	activity->callbacks->onNativeWindowRedrawNeeded = [] (ANativeActivity* activity, ANativeWindow* window) {
		((NativeActivity *)activity->instance)->handleNativeWindowRedrawNeeded(window);
	};

	activity->callbacks->onNativeWindowResized = onNativeWindowResized;
	activity->callbacks->onPause = onPause;
	activity->callbacks->onResume = onResume;
	activity->callbacks->onSaveInstanceState = onSaveInstanceState;
	activity->callbacks->onStart = onStart;
	activity->callbacks->onStop = onStop;
	activity->callbacks->onWindowFocusChanged = onWindowFocusChanged;

	filesystem::platform::Android_initializeFilesystem(activity->assetManager, activity->internalDataPath, activity->externalDataPath);

	// ANativeActivity_setWindowFlags(activity, 0, toInt(WindowFlags::FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS));

	activity->instance = this;

	auto app = Application::getInstance();
	app->setNativeHandle(this);
	thread = Rc<EngineMainThread>::create(app, getAppInfo(config));

	return true;
}

void NativeActivity::wakeup() {
	uint64_t value = 1;
	::write(_eventfd, &value, sizeof(value));
}

void NativeActivity::setView(graphic::ViewImpl *view) {
	std::unique_lock lock(rootViewMutex);
	rootViewTmp = view;
	rootViewVar.notify_all();
}

void NativeActivity::handleConfigurationChanged() {
	if (config) {
		AConfiguration_delete(config);
	}
	config = AConfiguration_new();
	AConfiguration_fromAssetManager(config, activity->assetManager);

	auto appInfo = getAppInfo(config);

	auto app = thread->getApplication().get();
	app->performOnMainThread([appInfo = move(appInfo), app] () mutable {
		app->updateConfig(move(appInfo));
	}, app);
}

void NativeActivity::handleInputQueueCreated(AInputQueue *queue) {
	auto it = _input.emplace(queue, InputLooperData{this, queue}).first;
	AInputQueue_attachLooper(queue, looper, 0, [] (int fd, int events, void* data) {
		auto d = (InputLooperData *)data;
		return d->activity->handleInputEventQueue(fd, events, d->queue);
	}, &it->second);
}

void NativeActivity::handleInputQueueDestroyed(AInputQueue *queue) {
	AInputQueue_detachLooper(queue);
	_input.erase(queue);
}

void NativeActivity::handleNativeWindowCreated(ANativeWindow *window) {
    stappler::log::format("NativeActivity", "NativeWindowCreated: %p -- %p", activity, window);

	thread->waitForRunning();

	std::unique_lock lock(rootViewMutex);
	if (rootView) {
		lock.unlock();
	} else {
		rootViewVar.wait(lock, [this] {
			return rootViewTmp != nullptr;
		});
		rootView = move(rootViewTmp);
	}

	if (rootView) {
		rootView->runWithWindow(window);
		_windowSize = Size2(ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));
	}
}

void NativeActivity::handleNativeWindowDestroyed(ANativeWindow *window) {
	if (rootView) {
		rootView->stopWindow();
		// rootView = nullptr;
	}

	stappler::log::format("NativeActivity", "NativeWindowDestroyed: %p -- %p", activity, window);
}

void NativeActivity::handleNativeWindowRedrawNeeded(ANativeWindow *window) {
	if (rootView) {
		rootView->update(true);
	}

	stappler::log::format("NativeActivity", "NativeWindowRedrawNeeded: %p -- %p", activity, window);
}

int NativeActivity::handleLooperEvent(int fd, int events) {
	if (fd == _eventfd && events == ALOOPER_EVENT_INPUT) {
		uint64_t value = 0;
		::read(fd, &value, sizeof(value));
		if (value > 0) {
			if (rootView) {
				rootView->update(false);
			}
		}
		return 1;
	} else if (fd == _timerfd && events == ALOOPER_EVENT_INPUT) {
		if (rootView) {
			rootView->update(false);
		}
		return 1;
	}
	return 0;
}

int NativeActivity::handleInputEventQueue(int fd, int events, AInputQueue *queue) {
    AInputEvent* event = NULL;
    while (AInputQueue_getEvent(queue, &event) >= 0) {
        if (AInputQueue_preDispatchEvent(queue, event)) {
            continue;
        }
        int32_t handled = handleInputEvent(event);
        AInputQueue_finishEvent(queue, event, handled);
    }
    return 1;
}

int NativeActivity::handleInputEvent(AInputEvent *event) {
	auto type = AInputEvent_getType(event);
	auto source = AInputEvent_getSource(event);
	switch (type) {
	case AINPUT_EVENT_TYPE_KEY:
		stappler::log::format("NativeActivity", "New key event");
		return handleKeyEvent(event);
		break;
	case AINPUT_EVENT_TYPE_MOTION:
		stappler::log::format("NativeActivity", "New motion event");
		return handleMotionEvent(event);
		break;
	}
	return 0;
}

int NativeActivity::handleKeyEvent(AInputEvent *event) {
	auto action = AKeyEvent_getAction(event);
	auto flags = AKeyEvent_getFlags(event);
	auto modsFlags = AKeyEvent_getMetaState(event);

	auto keyCode = AKeyEvent_getKeyCode(event);
	auto scanCode = AKeyEvent_getKeyCode(event);

	if (keyCode == AKEYCODE_BACK) {
		if (rootView->isNavigationEmpty()) {
			return 0;
		}
	}

	InputModifier mods = InputModifier::None;
	if (modsFlags != AMETA_NONE) {
		if ((modsFlags & AMETA_ALT_ON) != AMETA_NONE) { mods |= InputModifier::Alt; }
		if ((modsFlags & AMETA_ALT_LEFT_ON) != AMETA_NONE) { mods |= InputModifier::AltL; }
		if ((modsFlags & AMETA_ALT_RIGHT_ON) != AMETA_NONE) { mods |= InputModifier::AltR; }
		if ((modsFlags & AMETA_SHIFT_ON) != AMETA_NONE) { mods |= InputModifier::Shift; }
		if ((modsFlags & AMETA_SHIFT_LEFT_ON) != AMETA_NONE) { mods |= InputModifier::ShiftL; }
		if ((modsFlags & AMETA_SHIFT_RIGHT_ON) != AMETA_NONE) { mods |= InputModifier::ShiftR; }
		if ((modsFlags & AMETA_CTRL_ON) != AMETA_NONE) { mods |= InputModifier::Ctrl; }
		if ((modsFlags & AMETA_CTRL_LEFT_ON) != AMETA_NONE) { mods |= InputModifier::CtrlL; }
		if ((modsFlags & AMETA_CTRL_RIGHT_ON) != AMETA_NONE) { mods |= InputModifier::CtrlR; }
		if ((modsFlags & AMETA_META_ON) != AMETA_NONE) { mods |= InputModifier::Mod3; }
		if ((modsFlags & AMETA_META_LEFT_ON) != AMETA_NONE) { mods |= InputModifier::Mod3L; }
		if ((modsFlags & AMETA_META_RIGHT_ON) != AMETA_NONE) { mods |= InputModifier::Mod3R; }

		if ((modsFlags & AMETA_CAPS_LOCK_ON) != AMETA_NONE) { mods |= InputModifier::CapsLock; }
		if ((modsFlags & AMETA_NUM_LOCK_ON) != AMETA_NONE) { mods |= InputModifier::NumLock; }

		if ((modsFlags & AMETA_SCROLL_LOCK_ON) != AMETA_NONE) { mods |= InputModifier::ScrollLock; }
		if ((modsFlags & AMETA_SYM_ON) != AMETA_NONE) { mods |= InputModifier::Sym; }
		if ((modsFlags & AMETA_FUNCTION_ON) != AMETA_NONE) { mods |= InputModifier::Function; }
	}

	_activeModifiers = mods;

	Vector<InputEventData> events;

	bool isCanceled = (flags & AKEY_EVENT_FLAG_CANCELED) | (flags & AKEY_EVENT_FLAG_CANCELED_LONG_PRESS);

	switch (action) {
	case AKEY_EVENT_ACTION_DOWN: {
		auto &ev = events.emplace_back(InputEventData{uint32_t(keyCode),
			InputEventName::KeyPressed, InputMouseButton::Touch, _activeModifiers,
			_hoverLocation.x, _hoverLocation.y});
		ev.key.keycode = s_keycodes[keyCode];
		ev.key.compose = InputKeyComposeState::Nothing;
		ev.key.keysym = keyCode;
		ev.key.keychar = 0;
		break;
	}
	case AKEY_EVENT_ACTION_UP: {
		auto &ev = events.emplace_back(InputEventData{uint32_t(keyCode),
			isCanceled ? InputEventName::KeyReleased : InputEventName::KeyCanceled, InputMouseButton::Touch, _activeModifiers,
			_hoverLocation.x, _hoverLocation.y});
		ev.key.keycode = s_keycodes[keyCode];
		ev.key.compose = InputKeyComposeState::Nothing;
		ev.key.keysym = keyCode;
		ev.key.keychar = 0;
		break;
	}
	case AKEY_EVENT_ACTION_MULTIPLE: {
		auto &ev = events.emplace_back(InputEventData{uint32_t(keyCode),
			InputEventName::KeyRepeated, InputMouseButton::Touch, _activeModifiers,
			_hoverLocation.x, _hoverLocation.y});
		ev.key.keycode = s_keycodes[keyCode];
		ev.key.compose = InputKeyComposeState::Nothing;
		ev.key.keysym = keyCode;
		ev.key.keychar = 0;
		break;
	}
	}
	if (!events.empty()) {
		if (rootView) {
			rootView->handleInputEvents(move(events));
		}
		return 1;
	}
	return 0;
}

int NativeActivity::handleMotionEvent(AInputEvent *event) {
	Vector<InputEventData> events;
	auto action = AMotionEvent_getAction(event);
	auto count = AMotionEvent_getPointerCount(event);
	switch (action & AMOTION_EVENT_ACTION_MASK) {
	case AMOTION_EVENT_ACTION_DOWN:
		for (size_t i = 0; i < count; ++ i) {
			auto &ev = events.emplace_back(InputEventData{uint32_t(AMotionEvent_getPointerId(event, i)),
				InputEventName::Begin, InputMouseButton::Touch, _activeModifiers,
				AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i)});
			ev.point.density = _density;
		}
		break;
	case AMOTION_EVENT_ACTION_UP:
		for (size_t i = 0; i < count; ++ i) {
			auto &ev = events.emplace_back(InputEventData{uint32_t(AMotionEvent_getPointerId(event, i)),
				InputEventName::End, InputMouseButton::Touch, _activeModifiers,
				AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i)});
			ev.point.density = _density;
		}
		break;
	case AMOTION_EVENT_ACTION_MOVE:
		for (size_t i = 0; i < count; ++ i) {
			if (AMotionEvent_getHistorySize(event) == 0
					|| AMotionEvent_getX(event, i) - AMotionEvent_getHistoricalX(event, i, AMotionEvent_getHistorySize(event) - 1) != 0.0f
					|| AMotionEvent_getY(event, i) - AMotionEvent_getHistoricalY(event, i, AMotionEvent_getHistorySize(event) - 1) != 0.0f) {
				auto &ev = events.emplace_back(InputEventData{uint32_t(AMotionEvent_getPointerId(event, i)),
					InputEventName::Move, InputMouseButton::Touch, _activeModifiers,
					AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i)});
				ev.point.density = _density;
			}
		}
		break;
	case AMOTION_EVENT_ACTION_CANCEL:
		for (size_t i = 0; i < count; ++ i) {
			auto &ev = events.emplace_back(InputEventData{uint32_t(AMotionEvent_getPointerId(event, i)),
				InputEventName::Cancel, InputMouseButton::Touch, _activeModifiers,
				AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i)});
			ev.point.density = _density;
		}
		break;
	case AMOTION_EVENT_ACTION_OUTSIDE:
		stappler::log::vtext("NativeActivity", "Motion AMOTION_EVENT_ACTION_OUTSIDE ", count,
				" ", AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_POINTER_DOWN: {
		auto pointer = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> 8;
		auto &ev = events.emplace_back(InputEventData{uint32_t(AMotionEvent_getPointerId(event, pointer)),
			InputEventName::Begin, InputMouseButton::Touch, _activeModifiers,
			AMotionEvent_getX(event, pointer), _windowSize.height - AMotionEvent_getY(event, pointer)});
		ev.point.density = _density;
		break;
	}
	case AMOTION_EVENT_ACTION_POINTER_UP: {
		auto pointer = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> 8;
		auto &ev = events.emplace_back(InputEventData{uint32_t(AMotionEvent_getPointerId(event, pointer)),
			InputEventName::Begin, InputMouseButton::Touch, _activeModifiers,
			AMotionEvent_getX(event, pointer), _windowSize.height - AMotionEvent_getY(event, pointer)});
		ev.point.density = _density;
		break;
	}
	case AMOTION_EVENT_ACTION_HOVER_MOVE:
		for (size_t i = 0; i < count; ++ i) {
			auto &ev = events.emplace_back(InputEventData{uint32_t(AMotionEvent_getPointerId(event, i)),
				InputEventName::MouseMove, InputMouseButton::Touch, _activeModifiers,
				AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i)});
			ev.point.density = _density;
			_hoverLocation = Vec2(AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i));
		}
		break;
	case AMOTION_EVENT_ACTION_SCROLL:
		stappler::log::vtext("NativeActivity", "Motion AMOTION_EVENT_ACTION_SCROLL ", count,
				" ", AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_HOVER_ENTER:
		for (size_t i = 0; i < count; ++ i) {
			auto &ev = events.emplace_back(InputEventData::BoolEvent(InputEventName::PointerEnter, true,
					Vec2(AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i))));
			ev.id = uint32_t(AMotionEvent_getPointerId(event, i));
			ev.point.density = _density;
		}
		stappler::log::vtext("NativeActivity", "Motion AMOTION_EVENT_ACTION_HOVER_ENTER ", count,
				" ", AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_HOVER_EXIT:
		for (size_t i = 0; i < count; ++ i) {
			auto &ev = events.emplace_back(InputEventData::BoolEvent(InputEventName::PointerEnter, false,
					Vec2(AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i))));
			ev.id = uint32_t(AMotionEvent_getPointerId(event, i));
			ev.point.density = _density;
		}
		stappler::log::vtext("NativeActivity", "Motion AMOTION_EVENT_ACTION_HOVER_EXIT ", count,
				" ", AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_BUTTON_PRESS:
		stappler::log::vtext("NativeActivity", "Motion AMOTION_EVENT_ACTION_BUTTON_PRESS ", count,
				" ", AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_BUTTON_RELEASE:
		stappler::log::vtext("NativeActivity", "Motion AMOTION_EVENT_ACTION_BUTTON_RELEASE ", count,
				" ", AMotionEvent_getPointerId(event, 0));
		break;
	}
	if (!events.empty()) {
		if (rootView) {
			rootView->handleInputEvents(move(events));
		}
		return 1;
	}
	return 0;
}

Value NativeActivity::getAppInfo(AConfiguration *config) {
	Value appInfo;

	auto cl = getClass();

	auto j_getPackageName = getMethodID(cl, "getPackageName", "()Ljava/lang/String;");
	if (jstring strObj = (jstring) activity->env->CallObjectMethod(activity->clazz, j_getPackageName)) {
		appInfo.setString(activity->env->GetStringUTFChars(strObj, NULL), "bundle");
	}

	float density = nan();
	auto j_getResources = getMethodID(cl, "getResources", "()Landroid/content/res/Resources;");
	if (jobject resObj = (jobject) activity->env->CallObjectMethod(activity->clazz, j_getResources)) {
		auto j_getDisplayMetrics = getMethodID(activity->env->GetObjectClass(resObj),
			"getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
		if (jobject dmObj = (jobject) activity->env->CallObjectMethod(resObj, j_getDisplayMetrics)) {
			auto j_density = activity->env->GetFieldID(activity->env->GetObjectClass(dmObj), "density", "F");
			density = activity->env->GetFloatField(dmObj, j_density);
		}
	}

	String language = "en-us";
	AConfiguration_getLanguage(config, language.data());
	AConfiguration_getCountry(config, language.data() + 3);
	string::tolower(language);
	appInfo.setString(language, "locale");

	if (isnan(density)) {
		auto densityValue = AConfiguration_getDensity(config);
		switch (densityValue) {
			case ACONFIGURATION_DENSITY_LOW: density = 0.75f; break;
			case ACONFIGURATION_DENSITY_MEDIUM: density = 1.0f; break;
			case ACONFIGURATION_DENSITY_TV: density = 1.5f; break;
			case ACONFIGURATION_DENSITY_HIGH: density = 1.5f; break;
			case 280: density = 2.0f; break;
			case ACONFIGURATION_DENSITY_XHIGH: density = 2.0f; break;
			case 360: density = 3.0f; break;
			case 400: density = 3.0f; break;
			case 420: density = 3.0f; break;
			case ACONFIGURATION_DENSITY_XXHIGH: density = 3.0f; break;
			case 560: density = 4.0f; break;
			case ACONFIGURATION_DENSITY_XXXHIGH: density = 4.0f; break;
			default: break;
		}
	}

	appInfo.setDouble(density, "density");
	_density = density;

	int32_t orientation = AConfiguration_getOrientation(config);
	int32_t width = AConfiguration_getScreenWidthDp(config);
	int32_t height = AConfiguration_getScreenHeightDp(config);

	switch (orientation) {
	case ACONFIGURATION_ORIENTATION_ANY:
	case ACONFIGURATION_ORIENTATION_SQUARE:
		appInfo.setInteger(width, "width");
		appInfo.setInteger(height, "height");
		break;
	case ACONFIGURATION_ORIENTATION_PORT:
		appInfo.setInteger(std::min(width, height), "width");
		appInfo.setInteger(std::max(width, height), "height");
		break;
	case ACONFIGURATION_ORIENTATION_LAND:
		appInfo.setInteger(std::max(width, height), "width");
		appInfo.setInteger(std::min(width, height), "height");
		break;
	}

	return appInfo;
}

SP_EXTERN_C JNIEXPORT
void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize) {
	stappler::log::format("NativeActivity", "Creating: %p %s %s", activity, activity->internalDataPath, activity->externalDataPath);

	NativeActivity *a = new NativeActivity();
	a->init(activity);
}

}

namespace stappler::xenolith::platform::graphic {

Rc<gl::View> createView(gl::Loop &loop, gl::Device &dev, gl::ViewInfo &&info) {
	return Rc<ViewImpl>::create(loop, dev, move(info));
}

}

#endif
