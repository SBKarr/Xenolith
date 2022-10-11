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

#ifndef TEST_SRC_WIDGETS_APPCHECKBOX_H_
#define TEST_SRC_WIDGETS_APPCHECKBOX_H_

#include "XLLayer.h"

namespace stappler::xenolith::app {

class AppCheckbox : public Layer {
public:
	virtual ~AppCheckbox() { }

	virtual bool init(bool, Function<void(bool)> &&);

	virtual void setValue(bool);
	virtual bool getValue() const;

	virtual void setForegroundColor(const Color4F &);
	virtual Color4F getForegroundColor() const;

	virtual void setBackgroundColor(const Color4F &);
	virtual Color4F getBackgroundColor() const;

protected:
	virtual void updateValue();

	bool _value = false;
	Function<void(bool)> _callback;
	Color4F _backgroundColor = Color::Grey_200;
	Color4F _foregroundColor = Color::Grey_500;

	InputListener *_input = nullptr;
};

class AppCheckboxWithLabel : public AppCheckbox {
public:
	virtual ~AppCheckboxWithLabel() { }

	virtual bool init(StringView, bool, Function<void(bool)> &&);

	virtual void onContentSizeDirty() override;

protected:
	Label *_label = nullptr;
};

}

#endif /* TEST_SRC_WIDGETS_APPCHECKBOX_H_ */
