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

#ifndef UTILS_VIEW_SRC_VIEWSCENE_H_
#define UTILS_VIEW_SRC_VIEWSCENE_H_

#include "XLScene.h"

namespace stappler::xenolith::viewapp {

class AppDelegate;

class ViewScene : public Scene {
public:
	virtual ~ViewScene();

	virtual bool init(AppDelegate *, Extent2 extent);

	virtual void onPresented(Director *) override;
	virtual void onFinished(Director *) override;

	virtual void update(const UpdateTime &) override;

	virtual void onEnter(Scene *) override;
	virtual void onExit() override;
	virtual void onContentSizeDirty() override;

protected:
	Sprite *_sprite;
	Node *_layout = nullptr;
};

}

#endif /* UTILS_VIEW_SRC_VIEWSCENE_H_ */