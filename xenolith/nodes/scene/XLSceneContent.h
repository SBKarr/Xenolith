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

#ifndef XENOLITH_NODES_SCENE_XLSCENECONTENT_H_
#define XENOLITH_NODES_SCENE_XLSCENECONTENT_H_

#include "XLDynamicStateNode.h"

namespace stappler::xenolith {

class SceneLayout;

class SceneContent : public DynamicStateNode {
public:
	virtual ~SceneContent();

	virtual bool init() override;

	virtual void onEnter(Scene *) override;
	virtual void onExit() override;

	virtual void onContentSizeDirty() override;

	// replaced node will be alone in stack, so, no need for exit transition
	virtual void replaceLayout(SceneLayout *);
	virtual void pushLayout(SceneLayout *);

	virtual void replaceTopLayout(SceneLayout *);
	virtual void popLayout(SceneLayout *);

	virtual bool pushOverlay(SceneLayout *);
	virtual bool popOverlay(SceneLayout *);

	virtual SceneLayout *getTopLayout();
	virtual SceneLayout *getPrevLayout();

	virtual bool popTopLayout();
	virtual bool isActive() const;

	virtual bool onBackButton();
	virtual size_t getLayoutsCount() const;

	virtual const Vector<Rc<SceneLayout>> &getLayouts() const;
	virtual const Vector<Rc<SceneLayout>> &getOverlays() const;

	Padding getDecorationPadding() const { return _decorationPadding; }

	virtual void updateLayoutNode(SceneLayout *);

protected:
	friend class Scene;

	virtual void setDecorationPadding(Padding);

	virtual void pushNodeInternal(SceneLayout *node, Function<void()> &&cb);

	virtual void eraseLayout(SceneLayout *);
	virtual void eraseOverlay(SceneLayout *);
	virtual void replaceNodes();
	virtual void updateNodesVisibility();

	virtual void updateBackButtonStatus();

	Padding _decorationPadding;
	InputListener *_inputListener = nullptr;
	Vector<Rc<SceneLayout>> _layouts;
	Vector<Rc<SceneLayout>> _overlays;

	bool _retainBackButton = false;
	bool _backButtonRetained = false;
};

}

#endif /* XENOLITH_NODES_SCENE_XLSCENECONTENT_H_ */
