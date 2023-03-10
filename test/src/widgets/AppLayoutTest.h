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

#ifndef TEST_SRC_WIDGETS_APPLAYOUTTEST_H_
#define TEST_SRC_WIDGETS_APPLAYOUTTEST_H_

#include "AppTests.h"
#include "XLSceneLayout.h"

namespace stappler::xenolith::app {

class LayoutTestBackButton : public VectorSprite {
public:
	virtual bool init(Function<void()> &&);

	virtual void onContentSizeDirty() override;

protected:
	using VectorSprite::init;

	void handleMouseEnter();
	void handleMouseLeave();
	bool handlePress();

	Layer *_background = nullptr;
	Function<void()> _callback;
};

class LayoutTest : public SceneLayout {
public:
	virtual ~LayoutTest() { }

	virtual bool init(LayoutName, StringView);

	virtual void onContentSizeDirty() override;

	virtual void onEnter(Scene *) override;

	virtual void setDataValue(Value &&) override;

protected:
	using Node::init;

	LayoutName _layout = LayoutName::Root;
	LayoutName _layoutRoot = LayoutName::Root;
	Node *_backButton = nullptr;
	Label *_infoLabel = nullptr;
};

}

#endif /* TEST_SRC_WIDGETS_APPLAYOUTTEST_H_ */
