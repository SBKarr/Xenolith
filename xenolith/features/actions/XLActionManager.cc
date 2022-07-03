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

#include "XLActionManager.h"
#include "XLNode.h"

namespace stappler::xenolith {

ActionContainer::~ActionContainer() {
	if (nactions > ReserveActions) {
		auto target = (Vector<Action *> *)actions.data();
		for (auto &it : *target) {
			it->release(0);
		}
		target->~vector();
	} else {
		auto target = (Vector<Action*>*) actions.data();
		auto it = target->begin();
		while (it != target->end()) {
			(*it)->release(0);
			++it;
		}
	}
}

ActionContainer::ActionContainer(Node *target) : target(target) { }

Action *ActionContainer::getActionByTag(uint32_t tag) const {
	if (nactions <= ReserveActions) {
		auto target = (Action **)actions.data();
		auto end = (Action **)(actions.data() + nactions);
		while (target != end) {
			if ((*target)->getTag() == tag) {
				return *target;
			}
		}
	} else {
		auto target = (Vector<Action *> *)actions.data();
		for (auto &it : *target) {
			if (it->getTag() == tag) {
				return it;
			}
		}
	}
	return nullptr;
}

void ActionContainer::addAction(Action *a) {
	if (nactions < ReserveActions) {
		auto target = actions.data() + sizeof(Action *) * nactions;
		a->retain();
		memcpy(target, &a, sizeof(Action *));
		++ nactions;
	} else if (nactions > ReserveActions) {
		auto target = (Vector<Action *> *)actions.data();
		a->retain();
		target->emplace_back(a);
	} else {
		// update to Vector storage
		Vector<Action *> acts; acts.reserve(nactions + 1);
		foreach([&] (Action *a) {
			acts.emplace_back(a);
		});
		a->retain();
		acts.emplace_back(a);

		new (actions.data()) Vector<Action *>(move(acts));
		++ nactions;
	}
}

void ActionContainer::removeAction(Action *action) {
	if (nactions <= ReserveActions) {
		auto target = (Action **)actions.data();
		auto end = (Action **)(actions.data() + nactions);
		auto it = std::remove(target, end, action);
		if (std::distance(it, end) > 0) {
			action->invalidate();
			action->release(0);
			-- nactions;
		}
	} else {
		auto target = (Vector<Action *> *)actions.data();
		auto it = target->begin();
		while (it != target->end()) {
			if (*it == action) {
				action->invalidate();
				action->release(0);
				it = target->erase(it);
				break;
			} else {
				++ it;
			}
		}
	}
}

bool ActionContainer::invalidateActionByTag(uint32_t tag) {
	if (nactions <= ReserveActions) {
		auto target = (Action **)actions.data();
		auto end = (Action **)(actions.data() + nactions);
		while (target != end) {
			if ((*target)->getTag() == tag) {
				(*target)->invalidate();
				return true;
			}
		}
	} else {
		auto target = (Vector<Action *> *)actions.data();
		for (auto &it : *target) {
			if (it->getTag() == tag) {
				it->invalidate();
				return true;
			}
		}
	}
	return false;
}

void ActionContainer::invalidateAllActionsByTag(uint32_t tag) {
	if (nactions <= ReserveActions) {
		auto target = (Action **)actions.data();
		auto end = (Action **)(actions.data() + nactions);
		while (target != end) {
			if ((*target)->getTag() == tag) {
				(*target)->invalidate();
			}
		}
	} else {
		auto target = (Vector<Action *> *)actions.data();
		for (auto &it : *target) {
			if (it->getTag() == tag) {
				it->invalidate();
			}
		}
	}
}

bool ActionContainer::removeActionByTag(uint32_t tag) {
	if (nactions <= ReserveActions) {
		auto target = (Action **)actions.data();
		auto end = (Action **)(actions.data() + nactions);
		while (target != end) {
			if ((*target)->getTag() == tag) {
				(*target)->invalidate();
				(*target)->release(0);
				if (target + 1 != end) {
					memmove(target, target + 1, (end - (target + 1)) * sizeof(Action *));
				}
				-- nactions;
				return true;
			}
		}
	} else {
		auto target = (Vector<Action *> *)actions.data();
		auto it = target->begin();
		while (it != target->end()) {
			if ((*it)->getTag() == tag) {
				(*it)->invalidate();
				(*it)->release(0);
				it = target->erase(it);
				return true;
			} else {
				++ it;
			}
		}
	}
	return false;
}

void ActionContainer::removeAllActionsByTag(uint32_t tag) {
	if (nactions <= ReserveActions) {
		auto target = (Action **)actions.data();
		auto end = (Action **)(actions.data() + nactions);
		std::remove_if(target, end, [&] (Action *a) {
			if (a->isDone()) {
				a->release(0);
				-- nactions;
				return true;
			}
			return false;
		});
	} else {
		auto target = (Vector<Action *> *)actions.data();
		auto it = target->begin();
		while (it != target->end()) {
			if ((*it)->getTag() == tag) {
				(*it)->invalidate();
				(*it)->release(0);
				it = target->erase(it);
			} else {
				++ it;
			}
		}
	}
}

bool ActionContainer::cleanup() {
	if (nactions <= ReserveActions) {
		auto target = (Action **)actions.data();
		auto end = (Action **)(actions.data() + nactions);
		std::remove_if(target, end, [&] (Action *a) {
			if (a->isDone()) {
				a->release(0);
				-- nactions;
				return true;
			}
			return false;
		});
		return nactions == 0;
	} else {
		auto target = (Vector<Action *> *)actions.data();
		auto it = target->begin();
		while (it != target->end()) {
			if ((*it)->isDone()) {
				(*it)->release(0);
				it = target->erase(it);
			} else {
				++ it;
			}
		}
		return target->empty();
	}
}

bool ActionContainer::empty() const {
	if (nactions <= ReserveActions) {
		return nactions == 0;
	} else {
		auto target = (Vector<Action *> *)actions.data();
		return target->empty();
	}
}

size_t ActionContainer::size() const {
	if (nactions <= ReserveActions) {
		return nactions;
	} else {
		auto target = (Vector<Action *> *)actions.data();
		return target->size();
	}
}

ActionManager::~ActionManager() {
    removeAllActions();
}

ActionManager::ActionManager() { }

bool ActionManager::init() {
	return true;
}

void ActionManager::addAction(Action *action, Node *target, bool paused) {
	if (_current) {
		_pending.emplace_back(PendingAction{action, target, paused});
		return;
	}

	XLASSERT(action != nullptr, "");
	XLASSERT(target != nullptr, "");

	auto it = _actions.find(target);
	if (it == _actions.end()) {
		it = _actions.emplace(target).first;
		it->paused = paused;
	}

	action->setContainer(target);
	it->addAction(action);
	action->startWithTarget(target);
}

void ActionManager::removeAllActions() {
	if (_current) {
		auto it = _actions.begin();
		while (it != _actions.end()) {
			if (&(*it) == _current) {
				_current->foreach([&] (Action *a) {
					a->invalidate();
				});
			} else {
				it = _actions.erase(it);
			}
		}
	} else {
		_actions.clear();
	}
}

void ActionManager::removeAllActionsFromTarget(Node *target) {
	if (target == nullptr) {
		return;
	}

	if (_current && _current->target == target) {
		_current->foreach([&] (Action *a) {
			a->invalidate();
		});
	} else {
		_actions.erase(target);
	}

	auto it = _pending.begin();
	while (it != _pending.end()) {
		if (it->target == target) {
			it = _pending.erase(it);
		} else {
			++ it;
		}
	}
}

void ActionManager::removeAction(Action *action) {
	auto target = action->getContainer();
	if (_current && _current->target == target) {
		action->invalidate();
	} else {
		auto it = _actions.find(target);
		if (it != _actions.end()) {
			it->removeAction(action);
		}
	}

	auto it = _pending.begin();
	while (it != _pending.end()) {
		if (it->action == action) {
			it = _pending.erase(it);
			break;
		} else {
			++ it;
		}
	}
}

void ActionManager::removeActionByTag(uint32_t tag, Node *target) {
	if (_current && _current->target == target) {
		if (_current->invalidateActionByTag(tag)) {
			return;
		}
	} else {
		auto it = _actions.find(target);
		if (it != _actions.end()) {
			if (it->removeActionByTag(tag)) {
				return;
			}
		}
	}

	auto it = _pending.begin();
	while (it != _pending.end()) {
		if (it->target == target && it->action->getTag() == tag) {
			it = _pending.erase(it);
			break;
		} else {
			++ it;
		}
	}
}

void ActionManager::removeAllActionsByTag(uint32_t tag, Node *target) {
	if (_current && _current->target == target) {
		_current->invalidateAllActionsByTag(tag);
	} else {
		auto it = _actions.find(target);
		if (it != _actions.end()) {
			it->removeAllActionsByTag(tag);
		}
	}

	auto it = _pending.begin();
	while (it != _pending.end()) {
		if (it->target == target && it->action->getTag() == tag) {
			it = _pending.erase(it);
		} else {
			++ it;
		}
	}
}

Action *ActionManager::getActionByTag(uint32_t tag, const Node *target) const {
	if (_current && _current->target == target) {
		return _current->getActionByTag(tag);
	} else {
		auto it = _actions.find(target);
		if (it != _actions.end()) {
			return it->getActionByTag(tag);
		}
	}

	auto it = _pending.begin();
	while (it != _pending.end()) {
		if (it->target == target && it->action->getTag() == tag) {
			return it->action;
		} else {
			++ it;
		}
	}

	return nullptr;
}

size_t ActionManager::getNumberOfRunningActionsInTarget(const Node *target) const {
	size_t pending = 0;

	auto it = _pending.begin();
	while (it != _pending.end()) {
		if (it->target == target) {
			++ pending;
		}
		++ it;
	}

	if (_current && _current->target == target) {
		return _current->size() + pending;
	} else {
		auto it = _actions.find(target);
		if (it != _actions.end()) {
			return it->size() + pending;
		}
	}
	return 0;
}

void ActionManager::pauseTarget(Node *target) {
	auto it = _actions.find(target);
	if (it != _actions.end()) {
		it->paused = true;
	}
}

void ActionManager::resumeTarget(Node *target) {
	auto it = _actions.find(target);
	if (it != _actions.end()) {
		it->paused = false;
	}
}

Vector<Node *> ActionManager::pauseAllRunningActions() {
	Vector<Node *> ret;
	for (auto &it : _actions) {
		it.paused = true;
		ret.emplace_back(it.target);
	}
	return ret;
}

void ActionManager::resumeTargets(const Vector<Node*> &targetsToResume) {
	for (auto &target : targetsToResume) {
		auto it = _actions.find(target);
		if (it != _actions.end()) {
			it->paused = false;
		}
	}
}

/** Main loop of ActionManager. */
void ActionManager::update(const UpdateTime &time) {
	float dt = float(time.delta) / 1'000'000.0f;
	auto it = _actions.begin();
	auto end = _actions.end();
	while (it != end) {
		_current = &(*it);
		_current->foreach([&] (Action *a) {
			a->step(dt);
			if (a->isDone()) {
				a->stop();
			}
		});
		_current = nullptr;
		if (it->cleanup()) {
			it = _actions.erase(it);
		} else {
			++ it;
		}
	}

	for (auto &it : _pending) {
		addAction(it.action, it.target, it.paused);
	}
	_pending.clear();
}

}
