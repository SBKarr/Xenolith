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

#ifndef MODULES_MATERIAL_LAYOUT_MATERIALFLEXIBLELAYOUT_H_
#define MODULES_MATERIAL_LAYOUT_MATERIALFLEXIBLELAYOUT_H_

#include "MaterialDecoratedLayout.h"
#include "XLGuiScrollView.h"

namespace stappler::xenolith::material {

class FlexibleLayout : public DecoratedLayout {
public:
	struct NodeParams {
		enum Mask {
			None = 0,
			Position = 1 << 0,
			ContentSize = 1 << 1,
			AnchorPoint = 1 << 2,
			Visibility = 1 << 3,
		};

		void setPosition(float x, float y);
		void setPosition(const Vec2 &pos);
		void setAnchorPoint(const Vec2 &pt);
		void setContentSize(const Size2 &size);
		void setVisible(bool value);
		void apply(Node *) const;

		Mask mask = None;
		Vec2 position;
		Vec2 anchorPoint;
		Size2 contentSize;
		bool visible = false;
	};

	using HeightFunction = Function<Pair<float, float>()>; // pair< min, max >

	virtual ~FlexibleLayout() { }

	virtual bool init() override;
	virtual void onContentSizeDirty() override;

	template <typename T, typename ... Args>
	auto setBaseNode(const Rc<T> &ptr, Args && ... args) -> T * {
		setBaseNode(ptr.get(), std::forward<Args>(args)...);
		return ptr.get();
	}

	template <typename T, typename ... Args>
	auto setFlexibleNode(const Rc<T> &ptr, Args && ... args) -> T * {
		setFlexibleNode(ptr.get(), std::forward<Args>(args)...);
		return ptr.get();
	}

	virtual void setBaseNode(ScrollView *node, ZOrder zOrder = ZOrder(2));
	virtual void setFlexibleNode(Node *, ZOrder zOrder = ZOrder(3));

	virtual void setFlexibleAutoComplete(bool value);

	virtual void setFlexibleMinHeight(float height);
	virtual float getFlexibleMinHeight() const;

	virtual void setFlexibleMaxHeight(float height);
	virtual float getFlexibleMaxHeight() const;

	virtual void setFlexibleBaseNode(bool val);
	virtual bool isFlexibleBaseNode() const;

	virtual void setFlexibleHeightFunction(const HeightFunction &);
	virtual const HeightFunction &getFlexibleHeightFunction() const;

	virtual float getFlexibleLevel() const;
	virtual void setFlexibleLevel(float value);
	virtual void setFlexibleLevelAnimated(float value, float duration);
	virtual void setFlexibleHeight(float value);

	virtual void setBaseNodePadding(float);
	virtual float getBaseNodePadding() const;

	float getCurrentFlexibleHeight() const;
	float getCurrentFlexibleMax() const;

	virtual void onPush(SceneContent *l, bool replace) override;
	virtual void onForegroundTransitionBegan(SceneContent *l, SceneLayout *overlay) override;

	// Безопасный триггер препятствует анимации, пока достаточный объём скролла не пройден
	virtual void setSafeTrigger(bool value);
	virtual bool isSafeTrigger() const;

	virtual void expandFlexibleNode(float extraSpace, float animation = 0.0f);
	virtual void clearFlexibleExpand(float animation = 0.0f);

protected:
	virtual void updateFlexParams();
	virtual void onScroll(float delta, bool finished);
	virtual void onDecorNode(const NodeParams &);
	virtual void onFlexibleNode(const NodeParams &);
	virtual void onBaseNode(const NodeParams &, const Padding &, float offset);

	static constexpr int AutoCompleteTag() { return 5; }

	bool _flexibleAutoComplete = true;
	bool _flexibleBaseNode = true;
	bool _safeTrigger = true;

	float _flexibleLevel = 1.0f;
	float _flexibleMinHeight = 0.0f;
	float _flexibleMaxHeight = 0.0f;
	float _baseNodePadding = 4.0f;

	float _flexibleExtraSpace = 0.0f;

	HeightFunction _flexibleHeightFunction = nullptr;

	Node *_flexibleNode = nullptr;
	ScrollView *_baseNode = nullptr;
};

}

#endif /* MODULES_MATERIAL_LAYOUT_MATERIALFLEXIBLELAYOUT_H_ */
