/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_FEATURES_FONT_XLFONTSTYLE_H_
#define XENOLITH_FEATURES_FONT_XLFONTSTYLE_H_

#include "SPGeometry.h"
#include "SPSpanView.h"
#include "SPSvgReader.h"

namespace stappler::xenolith::font {

using Metric = stappler::vg::Metric;

using EnumSize = uint8_t;

enum class FontVariableAxis {
	None,
	Weight = 1 << 0, // wght
	Width = 1 << 1, // wdth
	Italic = 1 << 2, // ital
	Slant = 1 << 3, // slnt
	OpticalSize = 1 << 4, // opsz
	Grade = 1 << 5, // GRAD

	Stretch = Width,
};

SP_DEFINE_ENUM_AS_MASK(FontVariableAxis)

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

// slnt axis or special value for Italic
struct FontStyle : ValueWrapper<int16_t, class FontWeightFlag> {
	static const FontStyle Normal;
	static const FontStyle Italic;
	static const FontStyle Oblique;

	static constexpr FontStyle FromDegrees(float d) {
		return FontStyle(std::floor(d * 64.0f));
	}

	using ValueWrapper::ValueWrapper;
};

struct FontWeight : ValueWrapper<uint16_t, class FontWeightFlag> {
	static const FontWeight Thin;
	static const FontWeight ExtraLight;
	static const FontWeight Light;
	static const FontWeight Normal;
	static const FontWeight Regular;
	static const FontWeight Medium;
	static const FontWeight SemiBold;
	static const FontWeight Bold;
	static const FontWeight ExtraBold;
	static const FontWeight Heavy;
	static const FontWeight Black;

	using ValueWrapper::ValueWrapper;
};

struct FontStretch : ValueWrapper<uint16_t, class FontStretchFlag> {
	static const FontStretch UltraCondensed;
	static const FontStretch ExtraCondensed;
	static const FontStretch Condensed;
	static const FontStretch SemiCondensed;
	static const FontStretch Normal;
	static const FontStretch SemiExpanded;
	static const FontStretch Expanded;
	static const FontStretch ExtraExpanded;
	static const FontStretch UltraExpanded;

	using ValueWrapper::ValueWrapper;
};

struct FontGrade : ValueWrapper<int16_t, class FontGradeFlag> {
	static const FontGrade Thin;
	static const FontGrade Reduced;
	static const FontGrade Normal;
	static const FontGrade Heavy;

	using ValueWrapper::ValueWrapper;
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

struct FontSize {
	static const FontSize XXSmall;
	static const FontSize XSmall;
	static const FontSize Small;
	static const FontSize Medium;
	static const FontSize Large;
	static const FontSize XLarge;
	static const FontSize XXLarge;

	static FontSize progress(FontSize source, FontSize target, float p) {
		auto value = source.val() * (1.0f - p) + target.val() * p;
		return make(value);
	}

	static constexpr FontSize make(float value) {
		FontSize ret;
		ret.value = static_cast<uint16_t>(std::floor(value * 16.0f));
		return ret;
	}

	inline constexpr FontSize() = default;
	inline constexpr FontSize(const FontSize &) = default;

	inline explicit constexpr FontSize(uint16_t val) : value(val << 4) { }

	constexpr FontSize scale(float density) const { return FontSize::make(val() * density); }

	constexpr FontSize operator*(float v) const { return scale(v); }
	constexpr FontSize operator/(float v) const { return scale(1.0f / v); }

	constexpr uint16_t get() const { return value >> 4; }
	constexpr float val() const { return static_cast<float>(value) / 16.0f; }

	constexpr FontSize &operator-=(FontSize v) { value -= v.value; return *this; }

	constexpr bool operator==(const FontSize &) const = default;
	constexpr bool operator!=(const FontSize &) const = default;

	uint16_t value = 0;
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

struct FontLayoutParameters {
	FontStyle fontStyle = FontStyle::Normal;
	FontWeight fontWeight = FontWeight::Normal;
	FontStretch fontStretch = FontStretch::Normal;
	FontGrade fontGrade = FontGrade::Normal;

	inline bool operator == (const FontLayoutParameters &other) const = default;
	inline bool operator != (const FontLayoutParameters &other) const = default;
};

struct FontSpecializationVector : FontLayoutParameters {
	FontSize fontSize = FontSize(14);
	float density = 1.0f;

	String getSpecializationArgs() const;

	inline bool operator == (const FontSpecializationVector &other) const = default;
	inline bool operator != (const FontSpecializationVector &other) const = default;
};

struct FontParameters : FontSpecializationVector {
	static FontParameters create(const String &);

	FontVariant fontVariant = FontVariant::Normal;
	ListStyleType listStyleType = ListStyleType::None;
	StringView fontFamily;
	bool persistent = false;

	String getConfigName(bool caps = false) const;
	FontParameters getSmallCaps() const;

	inline bool operator == (const FontParameters &other) const = default;
	inline bool operator != (const FontParameters &other) const = default;
};

String getFontConfigName(const StringView &fontFamily, FontSize fontSize, FontStyle fontStyle, FontWeight fontWeight,
		FontStretch fontStretch, FontGrade fontGrade, FontVariant fontVariant, bool caps);

constexpr FontStretch FontStretch::UltraCondensed = FontStretch(50 << 1);
constexpr FontStretch FontStretch::ExtraCondensed = FontStretch((62 << 1) | 1);
constexpr FontStretch FontStretch::Condensed = FontStretch(75 << 1);
constexpr FontStretch FontStretch::SemiCondensed = FontStretch((87 << 1) | 1);
constexpr FontStretch FontStretch::Normal = FontStretch(100 << 1);
constexpr FontStretch FontStretch::SemiExpanded = FontStretch((112 << 1) | 1);
constexpr FontStretch FontStretch::Expanded = FontStretch(125 << 1);
constexpr FontStretch FontStretch::ExtraExpanded = FontStretch(150 << 1);
constexpr FontStretch FontStretch::UltraExpanded = FontStretch(200 << 1);

constexpr FontWeight FontWeight::Thin = FontWeight(100);
constexpr FontWeight FontWeight::ExtraLight = FontWeight(200);
constexpr FontWeight FontWeight::Light = FontWeight(300);
constexpr FontWeight FontWeight::Normal = FontWeight(400);
constexpr FontWeight FontWeight::Regular = FontWeight(400);
constexpr FontWeight FontWeight::Medium = FontWeight(500);
constexpr FontWeight FontWeight::SemiBold = FontWeight(600);
constexpr FontWeight FontWeight::Bold = FontWeight(700);
constexpr FontWeight FontWeight::ExtraBold = FontWeight(800);
constexpr FontWeight FontWeight::Heavy = FontWeight(900);
constexpr FontWeight FontWeight::Black = FontWeight(1000);

constexpr FontSize FontSize::XXSmall = FontSize(uint16_t(8));
constexpr FontSize FontSize::XSmall = FontSize(uint16_t(10));
constexpr FontSize FontSize::Small = FontSize(uint16_t(12));
constexpr FontSize FontSize::Medium = FontSize(uint16_t(14));
constexpr FontSize FontSize::Large = FontSize(uint16_t(16));
constexpr FontSize FontSize::XLarge = FontSize(uint16_t(20));
constexpr FontSize FontSize::XXLarge = FontSize(uint16_t(24));

constexpr FontStyle FontStyle::Normal = FontStyle(0);
constexpr FontStyle FontStyle::Italic = FontStyle(minOf<int16_t>());
constexpr FontStyle FontStyle::Oblique = FontStyle(-10 << 6);

constexpr FontGrade FontGrade::Thin = FontGrade(-200);
constexpr FontGrade FontGrade::Reduced = FontGrade(-50);
constexpr FontGrade FontGrade::Normal = FontGrade(0);
constexpr FontGrade FontGrade::Heavy = FontGrade(150);

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
	static constexpr uint32_t CharMask = 0x0000FFFFU;
	static constexpr uint32_t AnchorMask = 0x00030000U;
	static constexpr uint32_t SourceMask = 0xFFFC0000U;
	static constexpr uint32_t SourceMax = (SourceMask >> 18);

	static uint32_t getObjectId(uint16_t sourceId, char16_t, FontAnchor);
	static uint32_t getObjectId(uint32_t, FontAnchor);
	static FontAnchor getAnchorForObject(uint32_t);

	char16_t charID = 0;
	uint16_t xAdvance = 0;
	//int16_t xOffset = 0;
	//int16_t yOffset = 0;
	//uint16_t width;
	//uint16_t height;

	operator char16_t() const { return charID; }
};

struct CharSpec final {
	char16_t charID = 0;
	int16_t pos = 0;
	uint16_t advance = 0;
	uint16_t face = 0;
};

struct CharTexture final {
	uint16_t fontID = 0;
	char16_t charID = 0;
	int16_t x = 0;
	int16_t y = 0;
	uint16_t width = 0;
	uint16_t height = 0;

	uint32_t bitmapWidth;
	uint32_t bitmapRows;
	int pitch;
	uint8_t *bitmap;
};

struct FontAtlasValue {
	Vec2 pos;
	Vec2 tex;
};

struct FontCharString final {
	void addChar(char16_t);
	void addString(const String &);
	void addString(const WideString &);
	void addString(const char16_t *, size_t);
	void addString(const FontCharString &);

	bool empty() const { return chars.empty(); }

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

inline bool operator< (const CharLayout &l, const CharLayout &c) { return l.charID < c.charID; }
inline bool operator> (const CharLayout &l, const CharLayout &c) { return l.charID > c.charID; }
inline bool operator<= (const CharLayout &l, const CharLayout &c) { return l.charID <= c.charID; }
inline bool operator>= (const CharLayout &l, const CharLayout &c) { return l.charID >= c.charID; }

inline bool operator< (const CharLayout &l, const char16_t &c) { return l.charID < c; }
inline bool operator> (const CharLayout &l, const char16_t &c) { return l.charID > c; }
inline bool operator<= (const CharLayout &l, const char16_t &c) { return l.charID <= c; }
inline bool operator>= (const CharLayout &l, const char16_t &c) { return l.charID >= c; }

}

namespace stappler {

inline xenolith::font::FontSize progress(xenolith::font::FontSize source, xenolith::font::FontSize target, float p) {
	return xenolith::font::FontSize::progress(source, target, p);
}

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
