/**
Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLNode.h"

#include "XLInputDispatcher.h"
#include "XLInputListener.h"
#include "XLComponent.h"
#include "XLScene.h"
#include "XLDirector.h"
#include "XLScheduler.h"
#include "XLActionManager.h"

namespace stappler::xenolith {

bool Node::isParent(Node *parent, Node *node) {
	if (!node) {
		return false;
	}
	auto p = node->getParent();
	while (p) {
		if (p == parent) {
			return true;
		}
		p = p->getParent();
	}
	return false;
}

Mat4 Node::getChainNodeToParentTransform(Node *parent, Node *node, bool withParent) {
	if (!isParent(parent, node)) {
		return Mat4::IDENTITY;
	}

	Mat4 ret = node->getNodeToParentTransform();
	auto p = node->getParent();
	for (; p != parent; p = p->getParent()) {
		ret = ret * p->getNodeToParentTransform();
	}
	if (withParent && p == parent) {
		ret = ret * p->getNodeToParentTransform();
	}
	return ret;
}

Mat4 Node::getChainParentToNodeTransform(Node *parent, Node *node, bool withParent) {
	if (!isParent(parent, node)) {
		return Mat4::IDENTITY;
	}

	Mat4 ret = node->getParentToNodeTransform();
	auto p = node->getParent();
	for (; p != parent; p = p->getParent()) {
		ret = p->getParentToNodeTransform() * ret;
	}
	if (withParent && p == parent) {
		ret = p->getParentToNodeTransform() * ret;
	}
	return ret;
}

Node::Node() { }

Node::~Node() {
	for (auto &child : _children) {
		child->_parent = nullptr;
	}

	// stopAllActions();

	XLASSERT(!_running, "Node still marked as running on node destruction! Was base class onExit() called in derived class onExit() implementations?");
}

bool Node::init() {
	return true;
}

void Node::setLocalZOrder(int16_t z) {
	if (_zOrder == z) {
		return;
	}

	_zOrder = z;
	if (_parent) {
		_parent->reorderChild(this, z);
	}
}

void Node::setScale(float scale) {
	if (_scale.x == scale && _scale.x == scale && _scale.x == scale) {
		return;
	}

	_scale.x = _scale.y = _scale.z = scale;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setScale(const Vec2 &scale) {
	if (_scale.x == scale.x && _scale.y == scale.y) {
		return;
	}

	_scale.x = scale.x;
	_scale.y = scale.y;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setScale(const Vec3 &scale) {
	if (_scale == scale) {
		return;
	}

	_scale = scale;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setScaleX(float scaleX) {
	if (_scale.x == scaleX) {
		return;
	}

	_scale.x = scaleX;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setScaleY(float scaleY) {
	if (_scale.y == scaleY) {
		return;
	}

	_scale.y = scaleY;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setScaleZ(float scaleZ) {
	if (_scale.z == scaleZ) {
		return;
	}

	_scale.z = scaleZ;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setPosition(const Vec2 &position) {
	if (_position.x == position.x && _position.y == position.y) {
		return;
	}

	_position.x = position.x;
	_position.y = position.y;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setPosition(const Vec3 &position) {
	if (_position == position) {
		return;
	}

	_position = position;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setPositionX(float value) {
	if (_position.x == value) {
		return;
	}

	_position.x = value;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setPositionY(float value) {
	if (_position.y == value) {
		return;
	}

	_position.y = value;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setPositionZ(float value) {
	if (_position.z == value) {
		return;
	}

	_position.z = value;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setSkewX(float skewX) {
	if (_skew.x == skewX) {
		return;
	}

	_skew.x = skewX;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setSkewY(float skewY) {
	if (_skew.y == skewY) {
		return;
	}

	_skew.y = skewY;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setAnchorPoint(const Vec2 &point) {
	if (point == _anchorPoint) {
		return;
	}

	_anchorPoint = point;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setContentSize(const Size2 &size) {
	if (size == _contentSize) {
		return;
	}

	_contentSize = size;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = _contentSizeDirty = true;
}

void Node::setVisible(bool visible) {
	if (visible == _visible) {
		return;
	}
	_visible = visible;
	if (_visible) {
		_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
	}
}

void Node::setRotation(float rotation) {
	if (_rotation.z == rotation && _rotation.x == 0 && _rotation.y == 0) {
		return;
	}

	_rotation = Vec3(0.0f, 0.0f, rotation);
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
	_rotationQuat = Quaternion(_rotation);
}

void Node::setRotation(const Vec3 &rotation) {
	if (_rotation == rotation) {
		return;
	}

	_rotation = rotation;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
	_rotationQuat = Quaternion(_rotation);
}

void Node::setRotation(const Quaternion &quat) {
	if (_rotationQuat == quat) {
		return;
	}

	_rotationQuat = quat;
	_rotation = _rotationQuat.toEulerAngles();
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::addChildNode(Node *child) {
	addChildNode(child, child->_zOrder, child->_tag);
}

void Node::addChildNode(Node *child, int16_t localZOrder) {
	addChildNode(child, localZOrder, child->_tag);
}

void Node::addChildNode(Node *child, int16_t localZOrder, uint64_t tag) {
	XLASSERT( child != nullptr, "Argument must be non-nil");
	XLASSERT( child->_parent == nullptr, "child already added. It can't be added again");

	if constexpr (config::NodePreallocateChilds > 1) {
		if (_children.empty()) {
			_children.reserve(config::NodePreallocateChilds);
		}
	}

	_reorderChildDirty = true;
	_children.push_back(child);
	child->setLocalZOrder(localZOrder);
	if (tag != InvalidTag) {
		child->setTag(tag);
	}
	child->setParent(this);

	if (_running) {
		child->onEnter(_scene);
	}

	if (_cascadeColorEnabled) {
		updateCascadeColor();
	}

	if (_cascadeOpacityEnabled) {
		updateCascadeOpacity();
	}
}

Node* Node::getChildByTag(uint64_t tag) const {
	XLASSERT( tag != InvalidTag, "Invalid tag");
	for (const auto &child : _children) {
		if (child && child->_tag == tag) {
			return child;
		}
	}
	return nullptr;
}

void Node::setParent(Node *parent) {
	if (parent == _parent) {
		return;
	}
	_parent = parent;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::removeFromParent(bool cleanup) {
	if (_parent != nullptr) {
		_parent->removeChild(this, cleanup);
	}
}

void Node::removeChild(Node *child, bool cleanup) {
	// explicit nil handling
	if (_children.empty()) {
		return;
	}

	auto it = std::find(_children.begin(), _children.end(), child);
	if (it != _children.end()) {
		if (_running) {
			child->onExit();
		}

		if (cleanup) {
			child->cleanup();
		}

		// set parent nil at the end
		child->setParent(nullptr);
		_children.erase(it);
	}
}

void Node::removeChildByTag(uint64_t tag, bool cleanup) {
	XLASSERT( tag != InvalidTag, "Invalid tag");

	Node *child = this->getChildByTag(tag);
	if (child == nullptr) {
		stappler::log::format("Node", "removeChildByTag(tag = %ld): child not found!", tag);
	} else {
		this->removeChild(child, cleanup);
	}
}

void Node::removeAllChildren(bool cleanup) {
	for (const auto &child : _children) {
		if (_running) {
			child->onExit();
		}

		if (cleanup) {
			child->cleanup();
		}
		// set parent nil at the end
		child->setParent(nullptr);
	}

	_children.clear();
}

void Node::reorderChild(Node * child, int32_t localZOrder) {
	XLASSERT( child != nullptr, "Child must be non-nil");
	_reorderChildDirty = true;
	child->setLocalZOrder(localZOrder);
}

void Node::sortAllChildren() {
	if (_reorderChildDirty) {
		std::sort(std::begin(_children), std::end(_children), [&] (const Node *l, const Node *r) {
			return l->getLocalZOrder() < r->getLocalZOrder();
		});
		onReorderChildDirty();
		_reorderChildDirty = false;
	}
}

void Node::runActionObject(Action *action) {
	XLASSERT( action != nullptr, "Argument must be non-nil");
	if (_actionManager) {
		_actionManager->addAction(action, this, !_running);
	}
}

void Node::runActionObject(Action *action, uint32_t tag) {
	if (action) {
		action->setTag(tag);
	}
	runActionObject(action);
}

void Node::stopAllActions() {
	if (_actionManager) {
		_actionManager->removeAllActionsFromTarget(this);
	}
}

void Node::stopAction(Action *action) {
	if (_actionManager) {
		_actionManager->removeAction(action);
	}
}

void Node::stopActionByTag(uint32_t tag) {
	XLASSERT(tag != Action::INVALID_TAG, "Invalid tag");
	if (_actionManager) {
		_actionManager->removeActionByTag(tag, this);
	}
}

void Node::stopAllActionsByTag(uint32_t tag) {
	XLASSERT(tag != Action::INVALID_TAG, "Invalid tag");
	if (_actionManager) {
		_actionManager->removeAllActionsByTag(tag, this);
	}
}

Action* Node::getActionByTag(uint32_t tag) {
	XLASSERT(tag != Action::INVALID_TAG, "Invalid tag");
	if (_actionManager) {
		return _actionManager->getActionByTag(tag, this);
	}
	return nullptr;
}

size_t Node::getNumberOfRunningActions() const {
	if (_actionManager) {
		return _actionManager->getNumberOfRunningActionsInTarget(this);
	}
	return 0;
}


void Node::setTag(uint64_t tag) {
	_tag = tag;
}


bool Node::addComponentItem(Component *com) {
	XLASSERT(com != nullptr, "Argument must be non-nil");
	XLASSERT(com->getOwner() == nullptr, "Component already added. It can't be added again");

	com->setOwner(this);
	_components.push_back(com);
	com->onAdded();
	if (this->isRunning()) {
		com->onEnter(_scene);
	}

	return true;
}

bool Node::removeComponent(Component *com) {
	if (_components.empty()) {
		return false;
	}

	for (auto iter = _components.begin(); iter != _components.end(); ++iter) {
		if ((*iter) == com) {
			if (this->isRunning()) {
				com->onExit();
			}
			com->onRemoved();
			com->setOwner(nullptr);
			_components.erase(iter);
			return true;
		}
	}
	return false;
}

bool Node::removeComponentByTag(uint64_t tag) {
	if (_components.empty()) {
		return false;
	}

	for (auto iter = _components.begin(); iter != _components.end(); ++iter) {
		if ((*iter)->getTag() == tag) {
			auto com = (*iter);
			if (this->isRunning()) {
				com->onExit();
			}
			com->onRemoved();
			com->setOwner(nullptr);
			_components.erase(iter);
			return true;
		}
	}
	return false;
}

bool Node::removeAllComponentByTag(uint64_t tag) {
	if (_components.empty()) {
		return false;
	}

	auto iter = _components.begin();
	while (iter != _components.end()) {
		if ((*iter)->getTag() == tag) {
			auto com = (*iter);
			if (this->isRunning()) {
				com->onExit();
			}
			com->onRemoved();
			com->setOwner(nullptr);
			iter = _components.erase(iter);
		} else {
			++ iter;
		}
	}
	for (; iter != _components.end(); ++iter) {
		if ((*iter)->getTag() == tag) {
			auto com = (*iter);
			if (this->isRunning()) {
				com->onExit();
			}
			com->onRemoved();
			com->setOwner(nullptr);
			_components.erase(iter);
			return true;
		}
	}
	return false;
}

void Node::removeAllComponents() {
	for (auto iter : _components) {
		if (this->isRunning()) {
			iter->onExit();
		}
		iter->onRemoved();
		iter->setOwner(nullptr);
	}

	_components.clear();
}

bool Node::addInputListenerItem(InputListener *input) {
	XLASSERT(input != nullptr, "Argument must be non-nil");
	XLASSERT(input->getOwner() == nullptr, "Component already added. It can't be added again");

	input->setOwner(this);
	_inputEvents.push_back(input);
	if (this->isRunning()) {
		input->onEnter(_scene);
	}

	return true;
}

bool Node::removeInputListener(InputListener *input) {
	if (_inputEvents.empty()) {
		return false;
	}

	for (auto iter = _inputEvents.begin(); iter != _inputEvents.end(); ++iter) {
		if ((*iter) == input) {
			if (this->isRunning()) {
				input->onExit();
			}
			input->setOwner(nullptr);
			_inputEvents.erase(iter);
			return true;
		}
	}
	return false;
}

void Node::onEnter(Scene *scene) {
	_scene = scene;
	_director = scene->getDirector();

	if (_scheduler != _director->getScheduler()) {
		if (_scheduler) {
			_scheduler->unschedule(this);
		}
		_scheduler = _director->getScheduler();
	}

	if (_actionManager != _director->getActionManager()) {
		if (_actionManager) {
			_actionManager->removeAllActionsFromTarget(this);
		}
		_actionManager = _director->getActionManager();
	}

	if (_onEnterCallback) {
		_onEnterCallback(scene);
	}

	for (auto &it : _components) {
		it->onEnter(scene);
	}

	for (auto &it : _inputEvents) {
		it->onEnter(scene);
	}

	for (auto &child : _children) {
		child->onEnter(scene);
	}

	if (_scheduled) {
		_scheduler->scheduleUpdate(this, 0, _paused);
	}

	_running = true;
	this->resume();
}

void Node::onExit() {
	// In reverse order from onEnter()

	this->pause();
	_running = false;

	if (_scheduled) {
		_scheduler->unschedule(this);
	}

	for (auto &child : _children) {
		child->onExit();
	}

	for (auto &it : _inputEvents) {
		it->onExit();
	}

	for (auto &it : _components) {
		it->onExit();
	}

	if (_onExitCallback) {
		_onExitCallback();
	}

	// prevent node destruction until update is ended
	_director->autorelease(this);

	_scene = nullptr;
	_director = nullptr;
}

void Node::onContentSizeDirty() {
	if (_onContentSizeDirtyCallback) {
		_onContentSizeDirtyCallback();
	}

	for (auto &it : _components) {
		it->onContentSizeDirty();
	}
}

void Node::onTransformDirty(const Mat4 &parentTransform) {
	if (_onTransformDirtyCallback) {
		_onTransformDirtyCallback(parentTransform);
	}

	for (auto &it : _components) {
		it->onTransformDirty(parentTransform);
	}
}

void Node::onReorderChildDirty() {
	if (_onReorderChildDirtyCallback) {
		_onReorderChildDirtyCallback();
	}

	for (auto &it : _components) {
		it->onReorderChildDirty();
	}
}

void Node::cleanup() {
	this->stopAllActions();
	this->unscheduleUpdate();

	for (auto &child : _children) {
		child->cleanup();
	}
}

Rect Node::getBoundingBox() const {
	Rect rect(0, 0, _contentSize.width, _contentSize.height);
	return TransformRect(rect, getNodeToParentTransform());
}

void Node::resume() {
	if (_paused) {
		_paused = false;
		if (_running && _scheduled) {
			_scheduler->resume(this);
			_actionManager->resumeTarget(this);
		}
	}
}

void Node::pause() {
	if (!_paused) {
		if (_running && _scheduled) {
			_actionManager->pauseTarget(this);
			_scheduler->pause(this);
		}
		_paused = true;
	}
}

void Node::update(const UpdateTime &time) {

}

void Node::updateChildrenTransform() {
	// Recursively iterate over children
	for (auto &child: _children) {
		child->updateChildrenTransform();
	}
}

const Mat4& Node::getNodeToParentTransform() const {
	if (_transformCacheDirty) {
		// Translate values
		float x = _position.x;
		float y = _position.y;
		float z = _position.z;

		bool needsSkewMatrix = (_skew.x || _skew.y);

		Vec2 anchorPointInPoints(_contentSize.width * _anchorPoint.x, _contentSize.height * _anchorPoint.y);
		Vec2 anchorPoint(anchorPointInPoints.x * _scale.x, anchorPointInPoints.y * _scale.y);

		// caculate real position
		if (!needsSkewMatrix && !anchorPointInPoints.isZero()) {
			x += -anchorPoint.x;
			y += -anchorPoint.y;
		}

		// Build Transform Matrix = translation * rotation * scale
		Mat4 translation;
		//move to anchor point first, then rotate
		Mat4::createTranslation(x + anchorPoint.x, y + anchorPoint.y, z, &translation);

		Mat4::createRotation(_rotationQuat, &_transform);

		_transform = translation * _transform;
		//move by (-anchorPoint.x, -anchorPoint.y, 0) after rotation
		_transform.translate(-anchorPoint.x, -anchorPoint.y, 0);

		if (_scale.x != 1.f) {
			_transform.m[0] *= _scale.x, _transform.m[1] *= _scale.x, _transform.m[2] *= _scale.x;
		}
		if (_scale.y != 1.f) {
			_transform.m[4] *= _scale.y, _transform.m[5] *= _scale.y, _transform.m[6] *= _scale.y;
		}
		if (_scale.z != 1.f) {
			_transform.m[8] *= _scale.z, _transform.m[9] *= _scale.z, _transform.m[10] *= _scale.z;
		}

		// If skew is needed, apply skew and then anchor point
		if (needsSkewMatrix) {
			Mat4 skewMatrix(1, (float) tanf(_skew.y), 0, 0, (float) tanf(_skew.x), 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

			_transform = _transform * skewMatrix;

			// adjust anchor point
			if (!anchorPointInPoints.isZero()) {
				_transform.m[12] += _transform.m[0] * -anchorPointInPoints.x + _transform.m[4] * -anchorPointInPoints.y;
				_transform.m[13] += _transform.m[1] * -anchorPointInPoints.x + _transform.m[5] * -anchorPointInPoints.y;
			}
		}

		_transformCacheDirty = false;
	}

	return _transform;
}

void Node::setNodeToParentTransform(const Mat4& transform) {
	_transform = transform;
	_transformCacheDirty = false;
	_transformDirty = true;
}

const Mat4 &Node::getParentToNodeTransform() const {
	if (_transformInverseDirty) {
		_inverse = getNodeToParentTransform().getInversed();
		_transformInverseDirty = false;
	}

	return _inverse;
}

Mat4 Node::getNodeToWorldTransform() const {
	Mat4 t(this->getNodeToParentTransform());

	for (Node *p = _parent; p != nullptr; p = p->getParent()) {
		t = p->getNodeToParentTransform() * t;
	}

	return t;
}

Mat4 Node::getWorldToNodeTransform() const {
	return getNodeToWorldTransform().getInversed();
}

Vec2 Node::convertToNodeSpace(const Vec2 &worldPoint) const {
	Mat4 tmp = getWorldToNodeTransform();
	return tmp.transformPoint(worldPoint);
}

Vec2 Node::convertToWorldSpace(const Vec2 &nodePoint) const {
	Mat4 tmp = getNodeToWorldTransform();
	return tmp.transformPoint(nodePoint);
}

Vec2 Node::convertToNodeSpaceAR(const Vec2 &worldPoint) const {
	Vec2 nodePoint(convertToNodeSpace(worldPoint));
	return nodePoint - Vec2(_contentSize.width * _anchorPoint.x, _contentSize.height * _anchorPoint.y);
}

Vec2 Node::convertToWorldSpaceAR(const Vec2 &nodePoint) const {
	return convertToWorldSpace(nodePoint + Vec2(_contentSize.width * _anchorPoint.x, _contentSize.height * _anchorPoint.y));
}

void Node::setCascadeOpacityEnabled(bool cascadeOpacityEnabled) {
	if (_cascadeOpacityEnabled == cascadeOpacityEnabled) {
		return;
	}

	_cascadeOpacityEnabled = cascadeOpacityEnabled;
	if (_cascadeOpacityEnabled) {
		updateCascadeOpacity();
	} else {
		disableCascadeOpacity();
	}
}

void Node::setCascadeColorEnabled(bool cascadeColorEnabled) {
	if (_cascadeColorEnabled == cascadeColorEnabled) {
		return;
	}

	_cascadeColorEnabled = cascadeColorEnabled;
	if (_cascadeColorEnabled) {
		updateCascadeColor();
	} else {
		disableCascadeColor();
	}
}

void Node::setOpacity(float opacity) {
	_displayedColor.a = _realColor.a = opacity;
	updateCascadeOpacity();
}

void Node::setOpacity(OpacityValue value) {
	setOpacity(255.0f / value.get());
}

void Node::updateDisplayedOpacity(float parentOpacity) {
	_displayedColor.a = _realColor.a * parentOpacity;

	updateColor();

	if (_cascadeOpacityEnabled) {
		for (const auto &child : _children) {
			child->updateDisplayedOpacity(_displayedColor.a);
		}
	}
}

void Node::setColor(const Color4F& color, bool withOpacity) {
	if (withOpacity && _realColor.a != color.a) {
		_displayedColor = _realColor = color;

		updateCascadeColor();
		updateCascadeOpacity();
	} else {
		_realColor = Color4F(color.r, color.g, color.b, _realColor.a);
		_displayedColor = Color4F(color.r, color.g, color.b, _displayedColor.a);

		updateCascadeColor();
	}
}

void Node::updateDisplayedColor(const Color4F &parentColor) {
	_displayedColor.r = _realColor.r * parentColor.r;
	_displayedColor.g = _realColor.g * parentColor.g;
	_displayedColor.b = _realColor.b * parentColor.b;
	updateColor();

	if (_cascadeColorEnabled) {
		for (const auto &child : _children) {
			child->updateDisplayedColor(_displayedColor);
		}
	}
}


void Node::updateCascadeOpacity() {
	float parentOpacity = 1.0f;

	if (_parent != nullptr && _parent->isCascadeOpacityEnabled()) {
		parentOpacity = _parent->getDisplayedOpacity();
	}

	updateDisplayedOpacity(parentOpacity);
}

void Node::disableCascadeOpacity() {
	_displayedColor.a = _realColor.a;

	for (const auto &child : _children) {
		child->updateDisplayedOpacity(1.0f);
	}
}

void Node::updateCascadeColor() {
	Color4F parentColor = Color4F::WHITE;
	if (_parent && _parent->isCascadeColorEnabled()) {
		parentColor = _parent->getDisplayedColor();
	}

	updateDisplayedColor(parentColor);
}

void Node::disableCascadeColor() {
	for (const auto &child : _children) {
		child->updateDisplayedColor(Color4F::WHITE);
	}
}

void Node::draw(RenderFrameInfo &info, NodeFlags flags) {

}

bool Node::visitGeometry(RenderFrameInfo &info, NodeFlags parentFlags) {
	if (!_visible) {
		return false;
	}

	NodeFlags flags = processParentFlags(info, parentFlags);

	info.modelTransformStack.push_back(_modelViewTransform);
	info.zPath.push_back(getLocalZOrder());

	for (auto &it : _children) {
		it->visitGeometry(info, flags);
	}

	info.zPath.pop_back();
	info.modelTransformStack.pop_back();

	// on overload, we can update node's geometry after it's childrens

	return true;
}

bool Node::visitDraw(RenderFrameInfo &info, NodeFlags parentFlags) {
	if (!_visible) {
		return false;
	}

	NodeFlags flags = processParentFlags(info, parentFlags);

	bool visibleByCamera = true;

	info.modelTransformStack.push_back(_modelViewTransform);
	info.zPath.push_back(getLocalZOrder());

	size_t i = 0;

	if (!_children.empty()) {
		sortAllChildren();
		// draw children zOrder < 0
		for (; i < _children.size(); i++) {
			auto node = _children.at(i);

			if (node && node->_zOrder < 0)
				node->visitDraw(info, flags);
			else
				break;
		}

		visitSelf(info, flags, visibleByCamera);

		for (auto it = _children.cbegin() + i; it != _children.cend(); ++it) {
			(*it)->visitDraw(info, flags);
		}
	} else {
		visitSelf(info, flags, visibleByCamera);
	}

	info.zPath.pop_back();
	info.modelTransformStack.pop_back();

	return true;
}

void Node::scheduleUpdate() {
	if (!_scheduled) {
		_scheduled = true;
		if (_running) {
			_scheduler->scheduleUpdate(this, 0, _paused);
		}
	}
}

void Node::unscheduleUpdate() {
	if (_scheduled) {
		if (_running) {
			_scheduler->unschedule(this);
		}
		_scheduled = false;
	}
}

bool Node::isTouched(const Vec2 &location, float padding) {
	Vec2 point = convertToNodeSpace(location);
	return isTouchedNodeSpace(point, padding);
}

bool Node::isTouchedNodeSpace(const Vec2 &point, float padding) {
	const Size2 &size = getContentSize();
	if (point.x > -padding && point.y > -padding
			&& point.x < size.width + padding
			&& point.y < size.height + padding) {
		return true;
	} else {
		return false;
	}
}

void Node::setOnEnterCallback(Function<void(Scene *)> &&cb) {
	_onEnterCallback = move(cb);
}

void Node::setOnExitCallback(Function<void()> &&cb) {
	_onExitCallback = move(cb);
}

void Node::setOnContentSizeDirtyCallback(Function<void()> &&cb) {
	_onContentSizeDirtyCallback = move(cb);
}

void Node::setOnTransformDirtyCallback(Function<void(const Mat4 &)> &&cb) {
	_onTransformDirtyCallback = move(cb);
}

void Node::setOnReorderChildDirtyCallback(Function<void()> &&cb) {
	_onReorderChildDirtyCallback = move(cb);
}

Mat4 Node::transform(const Mat4 &parentTransform) {
	return parentTransform * this->getNodeToParentTransform();
}

NodeFlags Node::processParentFlags(RenderFrameInfo &info, NodeFlags parentFlags) {
	NodeFlags flags = parentFlags;

	if (_transformDirty) {
		onTransformDirty(info.modelTransformStack.back());
	}

	if ((flags & NodeFlags::DirtyMask) != NodeFlags::None || _transformDirty || _contentSizeDirty) {
		_modelViewTransform = this->transform(info.modelTransformStack.back());
	}

	if (_transformDirty) {
		_transformDirty = false;
		flags |= NodeFlags::TransformDirty;
	}

	if (_contentSizeDirty) {
		_contentSizeDirty = false;

		onContentSizeDirty();
		flags |= NodeFlags::ContentSizeDirty;
	}

	return flags;
}

void Node::visitSelf(RenderFrameInfo &info, NodeFlags flags, bool visibleByCamera) {
	for (auto &it : _components) {
		it->visit(info, flags);
	}

	for (auto &it : _inputEvents) {
		if (it->isEnabled()) {
			info.input->addListener(it);
		}
	}

	// self draw
	if (visibleByCamera) {
		this->draw(info, flags);
	}
}

}
