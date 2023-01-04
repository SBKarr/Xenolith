/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef COMPONENTS_XENOLITH_NODES_XLCOMPONENT_H_
#define COMPONENTS_XENOLITH_NODES_XLCOMPONENT_H_

#include "XLDefine.h"

namespace stappler::xenolith {

struct RenderFrameInfo;
class Node;
class Scene;

class Component : public Ref {
public:
	static uint64_t GetNextComponentId();

	Component();
	virtual ~Component();
	virtual bool init();

	virtual void onAdded(Node *owner);
	virtual void onRemoved();

	virtual void onEnter(Scene *);
	virtual void onExit();

	virtual void visit(RenderFrameInfo &, NodeFlags parentFlags);

	virtual void onContentSizeDirty();
	virtual void onTransformDirty(const Mat4 &);
	virtual void onReorderChildDirty();

	virtual bool isRunning() const;

	virtual bool isEnabled() const;
	virtual void setEnabled(bool b);

	Node* getOwner() const { return _owner; }

	void setFrameTag(uint64_t);
	uint64_t getFrameTag() const { return _frameTag; }

protected:
	Node *_owner = nullptr;
	bool _enabled = true;
	bool _running = false;
	uint64_t _frameTag = InvalidTag;
};

}

#endif /* COMPONENTS_XENOLITH_NODES_XLCOMPONENT_H_ */
