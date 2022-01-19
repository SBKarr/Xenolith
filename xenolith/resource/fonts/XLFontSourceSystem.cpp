/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>

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

// Generated with FontGenerator (%STAPPLER_ROOT%/test/generators/FontGenerator)

#include "XLDefine.h"
#include "XLFontSourceSystem.h"

#include "XLFont-DejaVuSans.cc"
#include "XLFont-DejaVuSans-Bold.cc"
#include "XLFont-DejaVuSans-BoldOblique.cc"
#include "XLFont-DejaVuSans-ExtraLight.cc"
#include "XLFont-DejaVuSans-Oblique.cc"
#include "XLFont-DejaVuSansCondensed.cc"
#include "XLFont-DejaVuSansCondensed-Bold.cc"
#include "XLFont-DejaVuSansCondensed-BoldOblique.cc"
#include "XLFont-DejaVuSansCondensed-Oblique.cc"
#include "XLFont-DejaVuSansMono.cc"
#include "XLFont-DejaVuSansMono-Bold.cc"
#include "XLFont-DejaVuSansMono-BoldOblique.cc"
#include "XLFont-DejaVuSansMono-Oblique.cc"

namespace stappler::xenolith::font {

StringView getSystemFontName(SystemFontName name) {
	switch (name) {
	case SystemFontName::DejaVuSans: return StringView("system://SystemFontName::DejaVuSans"); break;
	case SystemFontName::DejaVuSans_Bold: return StringView("system://SystemFontName::DejaVuSans_Bold"); break;
	case SystemFontName::DejaVuSans_BoldOblique: return StringView("system://SystemFontName::DejaVuSans_BoldOblique"); break;
	case SystemFontName::DejaVuSans_ExtraLight: return StringView("system://SystemFontName::DejaVuSans_ExtraLight"); break;
	case SystemFontName::DejaVuSans_Oblique: return StringView("system://SystemFontName::DejaVuSans_Oblique"); break;
	case SystemFontName::DejaVuSansCondensed: return StringView("system://SystemFontName::DejaVuSansCondensed"); break;
	case SystemFontName::DejaVuSansCondensed_Bold: return StringView("system://SystemFontName::DejaVuSansCondensed_Bold"); break;
	case SystemFontName::DejaVuSansCondensed_BoldOblique: return StringView("system://SystemFontName::DejaVuSansCondensed_BoldOblique"); break;
	case SystemFontName::DejaVuSansCondensed_Oblique: return StringView("system://SystemFontName::DejaVuSansCondensed_Oblique"); break;
	case SystemFontName::DejaVuSansMono: return StringView("system://SystemFontName::DejaVuSansMono"); break;
	case SystemFontName::DejaVuSansMono_Bold: return StringView("system://SystemFontName::DejaVuSansMono_Bold"); break;
	case SystemFontName::DejaVuSansMono_BoldOblique: return StringView("system://SystemFontName::DejaVuSansMono_BoldOblique"); break;
	case SystemFontName::DejaVuSansMono_Oblique: return StringView("system://SystemFontName::DejaVuSansMono_Oblique"); break;
	}

	return StringView();
}

BytesView getSystemFont(SystemFontName name) {
	switch (name) {
	case SystemFontName::DejaVuSans: return BytesView(s_font_DejaVuSans, 442637); break;
	case SystemFontName::DejaVuSans_Bold: return BytesView(s_font_DejaVuSans_Bold, 409601); break;
	case SystemFontName::DejaVuSans_BoldOblique: return BytesView(s_font_DejaVuSans_BoldOblique, 383788); break;
	case SystemFontName::DejaVuSans_ExtraLight: return BytesView(s_font_DejaVuSans_ExtraLight, 231320); break;
	case SystemFontName::DejaVuSans_Oblique: return BytesView(s_font_DejaVuSans_Oblique, 378191); break;
	case SystemFontName::DejaVuSansCondensed: return BytesView(s_font_DejaVuSansCondensed, 389680); break;
	case SystemFontName::DejaVuSansCondensed_Bold: return BytesView(s_font_DejaVuSansCondensed_Bold, 383358); break;
	case SystemFontName::DejaVuSansCondensed_BoldOblique: return BytesView(s_font_DejaVuSansCondensed_BoldOblique, 366942); break;
	case SystemFontName::DejaVuSansCondensed_Oblique: return BytesView(s_font_DejaVuSansCondensed_Oblique, 357522); break;
	case SystemFontName::DejaVuSansMono: return BytesView(s_font_DejaVuSansMono, 231879); break;
	case SystemFontName::DejaVuSansMono_Bold: return BytesView(s_font_DejaVuSansMono_Bold, 228780); break;
	case SystemFontName::DejaVuSansMono_BoldOblique: return BytesView(s_font_DejaVuSansMono_BoldOblique, 174218); break;
	case SystemFontName::DejaVuSansMono_Oblique: return BytesView(s_font_DejaVuSansMono_Oblique, 172330); break;
	}

	return BytesView();
}

}
