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

#ifndef XENOLITH_FEATURES_FONT_XLFONTSTYLE_H_
#define XENOLITH_FEATURES_FONT_XLFONTSTYLE_H_

#include "SPGeometry.h"
#include "SPSpanView.h"

namespace stappler::xenolith::font {

using EnumSize = uint8_t;

struct Metric {
	enum Units : EnumSize {
		Percent,
		Px,
		Em,
		Rem,
		Auto,
		Dpi,
		Dppx,
		Contain, // only for background-size
		Cover, // only for background-size
		Vw,
		Vh,
		VMin,
		VMax
	};

	inline bool isAuto() const { return metric == Units::Auto; }

	inline bool isFixed() const {
		switch (metric) {
		case Units::Px:
		case Units::Em:
		case Units::Rem:
		case Units::Vw:
		case Units::Vh:
		case Units::VMin:
		case Units::VMax:
			return true;
			break;
		default:
			break;
		}
		return false;
	}

	float value = 0.0f;
	Units metric = Units::Auto;

	Metric(float value, Units m) : value(value), metric(m) { }

	Metric() = default;
};

enum class Autofit : EnumSize {
	None,
	Width,
	Height,
	Cover,
	Contain,
};

enum class TextTransform : EnumSize {
	None,
	Uppercase,
	Lowercase,
};

enum class TextDecoration : EnumSize {
	None,
	LineThrough,
	Overline,
	Underline,
};

enum class TextAlign : EnumSize {
	Left,
	Center,
	Right,
	Justify,
};

enum class WhiteSpace : EnumSize {
	Normal,
	Nowrap,
	Pre,
	PreLine,
	PreWrap,
};

enum class Hyphens : EnumSize {
	None,
	Manual,
	Auto,
};

enum class VerticalAlign : EnumSize {
	Baseline,
	Middle,
	Sub,
	Super,
	Top,
	Bottom
};

enum class FontStyle : EnumSize {
	Normal,
	Italic,
	Oblique,
};

enum class FontWeight : EnumSize {
	W100,
	W200,
	W300,
	W400,
	W500,
	W600,
	W700,
	W800,
	W900,
	Thin = W100,
	ExtraLight = W200,
	Light = W300,
	Normal = W400,
	Regular = W400,
	Medium = W500,
	SemiBold = W600,
	Bold = W700,
	ExtraBold = W800,
	Heavy = W900,
	Black = W900,
};

enum class FontStretch : EnumSize {
	UltraCondensed,
	ExtraCondensed,
	Condensed,
	SemiCondensed,
	Normal,
	SemiExpanded,
	Expanded,
	ExtraExpanded,
	UltraExpanded
};

enum class FontVariant : EnumSize {
	Normal,
	SmallCaps,
};

enum class ListStyleType : EnumSize {
	None,
	Circle,
	Disc,
	Square,
	XMdash,
	Decimal,
	DecimalLeadingZero,
	LowerAlpha,
	LowerGreek,
	LowerRoman,
	UpperAlpha,
	UpperRoman
};

struct FontSize : ValueWrapper<uint16_t, class FontSizeFlag> {
	static const FontSize XXSmall;
	static const FontSize XSmall;
	static const FontSize Small;
	static const FontSize Medium;
	static const FontSize Large;
	static const FontSize XLarge;
	static const FontSize XXLarge;

	using ValueWrapper::ValueWrapper;
};

struct TextParameters {
	TextTransform textTransform = TextTransform::None;
	TextDecoration textDecoration = TextDecoration::None;
	WhiteSpace whiteSpace = WhiteSpace::Normal;
	Hyphens hyphens = Hyphens::Manual;
	VerticalAlign verticalAlign = VerticalAlign::Baseline;
	Color3B color = Color3B::BLACK;
	uint8_t opacity = 222;

	inline bool operator == (const TextParameters &other) const = default;
	inline bool operator != (const TextParameters &other) const = default;
};

struct FontParameters {
	static FontParameters create(const String &);

	FontStyle fontStyle = FontStyle::Normal;
	FontWeight fontWeight = FontWeight::Normal;
	FontStretch fontStretch = FontStretch::Normal;
	FontVariant fontVariant = FontVariant::Normal;
	ListStyleType listStyleType = ListStyleType::None;
	FontSize fontSize = FontSize::Medium;
	StringView fontFamily;

	String getConfigName(bool caps = false) const;
	FontParameters getSmallCaps() const;

	inline bool operator == (const FontParameters &other) const = default;
	inline bool operator != (const FontParameters &other) const = default;
};

bool readStyleValue(StringView r, Metric &value, bool resolutionMetric, bool allowEmptyMetric);

String getFontConfigName(const StringView &fontFamily, FontSize fontSize, FontStyle fontStyle, FontWeight fontWeight,
		FontStretch fontStretch, FontVariant fontVariant, bool caps);

constexpr FontSize FontSize::XXSmall = FontSize(uint16_t(8));
constexpr FontSize FontSize::XSmall = FontSize(uint16_t(10));
constexpr FontSize FontSize::Small = FontSize(uint16_t(12));
constexpr FontSize FontSize::Medium = FontSize(uint16_t(14));
constexpr FontSize FontSize::Large = FontSize(uint16_t(16));
constexpr FontSize FontSize::XLarge = FontSize(uint16_t(20));
constexpr FontSize FontSize::XXLarge = FontSize(uint16_t(24));

using FontLayoutId = ValueWrapper<uint16_t, class FontLayoutIdTag>;

enum FontAnchor : uint32_t {
	BottomLeft,
	TopLeft,
	TopRight,
	BottomRight
};

struct Metrics final {
	uint16_t size = 0; // font size in pixels
	uint16_t height = 0; // default font line height
	int16_t ascender = 0; // The distance from the baseline to the highest coordinate used to place an outline point
	int16_t descender = 0; // The distance from the baseline to the lowest grid coordinate used to place an outline point
	int16_t underlinePosition = 0;
	int16_t underlineThickness = 0;
};

struct CharLayout final {
	static uint32_t getObjectId(uint16_t sourceId, char16_t, FontAnchor);
	static uint32_t getObjectId(uint32_t, FontAnchor);

	char16_t charID = 0;
	int16_t xOffset = 0;
	int16_t yOffset = 0;
	uint16_t xAdvance = 0;
	uint16_t width;
	uint16_t height;

	operator char16_t() const { return charID; }
};

struct CharSpec final {
	char16_t charID = 0;
	int16_t pos = 0;
	uint16_t advance = 0;
	uint16_t face = 0;
};

struct CharTexture final {
	char16_t charID = 0;
	uint16_t x = 0;
	uint16_t y = 0;
	uint16_t width = 0;
	uint16_t height = 0;
	uint8_t texture = maxOf<uint8_t>();

	operator char16_t() const { return charID; }
};

struct FontCharString final {
	void addChar(char16_t);
	void addString(const String &);
	void addString(const WideString &);
	void addString(const char16_t *, size_t);

	Vector<char16_t> chars;
};

struct EmplaceCharInterface {
	uint16_t (*getX) (void *) = nullptr;
	uint16_t (*getY) (void *) = nullptr;
	uint16_t (*getWidth) (void *) = nullptr;
	uint16_t (*getHeight) (void *) = nullptr;
	void (*setX) (void *, uint16_t) = nullptr;
	void (*setY) (void *, uint16_t) = nullptr;
	void (*setTex) (void *, uint16_t) = nullptr;
};

Extent2 emplaceChars(const EmplaceCharInterface &, const SpanView<void *> &,
		float totalSquare = std::numeric_limits<float>::quiet_NaN());

inline bool operator< (const CharTexture &t, const CharTexture &c) { return t.charID < c.charID; }
inline bool operator> (const CharTexture &t, const CharTexture &c) { return t.charID > c.charID; }
inline bool operator<= (const CharTexture &t, const CharTexture &c) { return t.charID <= c.charID; }
inline bool operator>= (const CharTexture &t, const CharTexture &c) { return t.charID >= c.charID; }

inline bool operator< (const CharTexture &t, const char16_t &c) { return t.charID < c; }
inline bool operator> (const CharTexture &t, const char16_t &c) { return t.charID > c; }
inline bool operator<= (const CharTexture &t, const char16_t &c) { return t.charID <= c; }
inline bool operator>= (const CharTexture &t, const char16_t &c) { return t.charID >= c; }

inline bool operator< (const CharLayout &l, const CharLayout &c) { return l.charID < c.charID; }
inline bool operator> (const CharLayout &l, const CharLayout &c) { return l.charID > c.charID; }
inline bool operator<= (const CharLayout &l, const CharLayout &c) { return l.charID <= c.charID; }
inline bool operator>= (const CharLayout &l, const CharLayout &c) { return l.charID >= c.charID; }

inline bool operator< (const CharLayout &l, const char16_t &c) { return l.charID < c; }
inline bool operator> (const CharLayout &l, const char16_t &c) { return l.charID > c; }
inline bool operator<= (const CharLayout &l, const char16_t &c) { return l.charID <= c; }
inline bool operator>= (const CharLayout &l, const char16_t &c) { return l.charID >= c; }

}

namespace std {

template <>
struct hash<stappler::xenolith::font::FontSize> {
	hash() { }

	size_t operator() (const stappler::xenolith::font::FontSize &value) const noexcept {
		return hash<uint16_t>{}(value.get());
	}
};

}

#endif /* XENOLITH_FEATURES_FONT_XLFONTSTYLE_H_ */
