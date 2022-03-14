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

#ifndef XENOLITH_FEATURES_LOCALE_XLLOCALE_H_
#define XENOLITH_FEATURES_LOCALE_XLLOCALE_H_

#include "XLEventHeader.h"
#include "SPMetastring.h"

namespace stappler::xenolith {

// full localized string
template <typename CharType, CharType ... Chars> auto operator "" _locale() {
	return metastring::merge("@Locale:"_meta, metastring::metastring<Chars ...>());
}

/*inline String operator"" _locale ( const char* str, std::size_t len) {
	String ret;
	ret.reserve(len + "@Locale:"_len);
	ret.append("@Locale:");
	ret.append(str, len);
	return ret;
}*/

// localized token
inline String operator"" _token ( const char* str, std::size_t len) {
	String ret;
	ret.reserve(len + 2);
	ret.append("%");
	ret.append(str, len);
	ret.append("%");
	return ret;
}

inline String localeIndex(size_t idx) {
	String ret; ret.reserve(20);
	ret.append("%=");
	ret.append(toString(idx));
	ret.push_back('%');
	return ret;
}

template <size_t Index>
inline constexpr auto localeIndex() {
	return metastring::merge("%="_meta, metastring::numeric<Index>(), "%"_meta);
}

}

namespace stappler::xenolith::locale {

using LocaleInitList = std::initializer_list<Pair<StringView, StringView>>;
using LocaleIndexList = std::initializer_list<Pair<size_t, StringView>>;
using NumRule = Function<uint8_t(int64_t)>;

enum class TimeTokens {
	Today = 0,
	Yesterday,
	Jan,
	Feb,
	Mar,
	Apr,
	Nay,
	Jun,
	jul,
	Aug,
	Sep,
	Oct,
	Nov,
	Dec,
	Max
};

extern EventHeader onLocale;

struct Initializer {
	Initializer(const String &locale, LocaleInitList &&);
};

void define(const StringView &locale, LocaleInitList &&);
void define(const StringView &locale, LocaleIndexList &&);
void define(const StringView &locale, const std::array<StringView, toInt(TimeTokens::Max)> &);

void setDefault(const String &);
const String &getDefault();

void setLocale(const String &);
const String &getLocale();

void setNumRule(const String &, const NumRule &);

WideStringView string(const WideStringView &);
WideStringView string(size_t);
WideStringView numeric(const WideStringView &, size_t);

bool hasLocaleTagsFast(const WideStringView &);
bool hasLocaleTags(const WideStringView &);
WideString resolveLocaleTags(const WideStringView &);

String language(const String &locale);

// convert locale name to common form ('en-us', 'ru-ru', 'fr-fr')
String common(const String &locale);

StringView timeToken(TimeTokens);

String localDate(Time);
String localDate(const std::array<StringView, toInt(TimeTokens::Max)> &, Time);

}

#endif /* XENOLITH_FEATURES_LOCALE_XLLOCALE_H_ */
