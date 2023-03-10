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

#ifndef MODULES_MATERIAL_COMPONENTS_APPBAR_MATERIALAPPBAR_H_
#define MODULES_MATERIAL_COMPONENTS_APPBAR_MATERIALAPPBAR_H_

#include "MaterialSurface.h"
#include "MaterialMenuSource.h"
#include "MaterialLabel.h"
#include "XLSubscriptionListener.h"
#include "XLIconNames.h"

namespace stappler::xenolith::material {

enum class AppBarLayout {
	CenterAligned,
	Small,
	Medium,
	Large
};

class AppBar : public Surface {
public:
	static constexpr SurfaceStyle DefaultAppBarStyle = SurfaceStyle(NodeStyle::SurfaceTonal, ColorRole::Primary, Elevation::Level0);

	virtual ~AppBar() { }

	virtual bool init(AppBarLayout = AppBarLayout::Small, const SurfaceStyle & = DefaultAppBarStyle);
	virtual void onContentSizeDirty() override;

	virtual void setLayout(AppBarLayout);
	virtual AppBarLayout getLayout() const { return _layout; }

	virtual void setTitle(StringView);
	virtual StringView getTitle() const;

	virtual void setNavButtonIcon(IconName);
	virtual IconName getNavButtonIcon() const;

	virtual void setMaxActionIcons(size_t value);
	virtual size_t getMaxActionIcons() const;

	virtual void setActionMenuSource(MenuSource *);
	virtual void replaceActionMenuSource(MenuSource *, size_t maxIcons = maxOf<size_t>());
	virtual MenuSource * getActionMenuSource() const;

	virtual void setBasicHeight(float value);
	virtual float getBasicHeight() const;

	virtual void setNavCallback(Function<void()> &&);
	virtual const Function<void()> & getNavCallback() const;

	virtual void setSwallowTouches(bool value);
	virtual bool isSwallowTouches() const;

	virtual void setBarCallback(Function<void()> &&);
	virtual const Function<void()> & getBarCallback() const;

	Button *getNavNode() const;

protected:
	virtual void handleNavTapped();

	virtual void updateProgress();
	virtual float updateMenu(Node *composer, MenuSource *source, size_t maxIcons);
	virtual void layoutSubviews();
	virtual float getBaseLine() const;

	AppBarLayout _layout = AppBarLayout::Small;
	TypescaleLabel *_label = nullptr;
	Button *_navButton = nullptr;

	size_t _maxActionIcons = 3;
	DynamicStateNode *_scissorNode = nullptr;
	Node *_iconsComposer = nullptr;
	Node *_prevComposer = nullptr;

	DataListener<MenuSource> *_actionMenuSourceListener = nullptr;

	Function<void()> _navCallback = nullptr;
	Function<void()> _barCallback = nullptr;

	float _replaceProgress = 1.0f;
	float _basicHeight = 56.0f;
	float _iconWidth = 0.0f;

	InputListener *_inputListener = nullptr;
};

}

#endif /* MODULES_MATERIAL_COMPONENTS_APPBAR_MATERIALAPPBAR_H_ */
