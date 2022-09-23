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

#include "XLDynamicStateNode.h"

namespace stappler::xenolith {

bool DynamicStateNode::init() {
	return Node::init();
}

void DynamicStateNode::setApplyMode(StateApplyMode value) {
	_applyMode = value;
}

bool DynamicStateNode::visitDraw(RenderFrameInfo &info, NodeFlags parentFlags) {
	if (_applyMode == DoNotApply) {
		return Node::visitDraw(info, parentFlags);
	}

	if (!_visible) {
		return false;
	}

	NodeFlags flags = processParentFlags(info, parentFlags);

	bool visibleByCamera = true;

	info.modelTransformStack.push_back(_modelViewTransform);
	info.zPath.push_back(getLocalZOrder());

	auto prevStateId = info.currentStateId;
	auto currentState = info.commands->getState(prevStateId);
	if (!currentState) {
		// invalid state
		return Node::visitDraw(info, parentFlags);
	}

	auto newState = updateState(*currentState);

	if (newState.enabled == renderqueue::DynamicState::None) {
		// no need to enable anything, drop to 0
		info.currentStateId = 0;
		info.commands->setCurrentState(0);
		auto ret = Node::visitDraw(info, parentFlags);
		info.commands->setCurrentState(prevStateId);
		info.currentStateId = prevStateId;
		return ret;
	}

	gl::StateId stateId = info.commands->addState(newState);

	if (!_children.empty()) {
		sortAllChildren();

		switch (_applyMode) {
		case ApplyForAll:
		case ApplyForNodesBelow:
			info.currentStateId = stateId;
			info.commands->setCurrentState(stateId);
			break;
		default: break;
		}

		size_t i = 0;

		// draw children zOrder < 0
		for (; i < _children.size(); i++) {
			auto node = _children.at(i);

			if (node && node->getLocalZOrder() < 0)
				node->visitDraw(info, flags);
			else
				break;
		}

		switch (_applyMode) {
		case ApplyForNodesAbove:
			info.currentStateId = stateId;
			info.commands->setCurrentState(stateId);
			break;
		default: break;
		}

		visitSelf(info, flags, visibleByCamera);

		switch (_applyMode) {
		case ApplyForNodesBelow:
			info.commands->setCurrentState(prevStateId);
			info.currentStateId = prevStateId;
			break;
		default: break;
		}

		for (auto it = _children.cbegin() + i; it != _children.cend(); ++it) {
			(*it)->visitDraw(info, flags);
		}

		switch (_applyMode) {
		case ApplyForAll:
		case ApplyForNodesAbove:
			info.commands->setCurrentState(prevStateId);
			info.currentStateId = prevStateId;
			break;
		default: break;
		}

	} else {
		info.currentStateId = stateId;
		info.commands->setCurrentState(stateId);

		visitSelf(info, flags, visibleByCamera);

		info.commands->setCurrentState(prevStateId);
		info.currentStateId = prevStateId;
	}

	info.zPath.pop_back();
	info.modelTransformStack.pop_back();

	return true;
}

void DynamicStateNode::enableScissor(Padding outline) {
	_scissorEnabled = true;
	_scissorOutline = outline;
}

void DynamicStateNode::disableScissor() {
	_scissorEnabled = false;
}

gl::DrawStateValues DynamicStateNode::updateState(const gl::DrawStateValues &values) const {
	auto getViewRect = [&] {
	    Vec2 bottomLeft = convertToWorldSpace(Vec2(-_scissorOutline.left, -_scissorOutline.bottom));
		Vec2 topRight = convertToWorldSpace(Vec2(_contentSize.width + _scissorOutline.right, _contentSize.height + _scissorOutline.top));

		if (bottomLeft.x > topRight.x) {
			float b = topRight.x;
			topRight.x = bottomLeft.x;
			bottomLeft.x = b;
		}

		if (bottomLeft.y > topRight.y) {
			float b = topRight.y;
			topRight.y = bottomLeft.y;
			bottomLeft.y = b;
		}

		return URect{uint32_t(roundf(bottomLeft.x)), uint32_t(roundf(bottomLeft.y)),
			uint32_t(roundf(topRight.x - bottomLeft.x)), uint32_t(roundf(topRight.y - bottomLeft.y))};
	};

	gl::DrawStateValues ret(values);
	if (_scissorEnabled) {
		auto viewRect = getViewRect();
		if ((ret.enabled & renderqueue::DynamicState::Scissor) == renderqueue::DynamicState::None) {
			ret.enabled |= renderqueue::DynamicState::Scissor;
			ret.scissor = viewRect;
		} else if (ret.scissor.intersectsRect(viewRect)) {
			ret.scissor = URect{
				std::max(ret.scissor.x, viewRect.x),
				std::max(ret.scissor.y, viewRect.y),
				std::min(ret.scissor.width, viewRect.width),
				std::min(ret.scissor.height, viewRect.height),
			};
		}
	}
	return ret;
}

}
