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

#ifndef XENOLITH_GL_VK_XLVKVIEW_H_
#define XENOLITH_GL_VK_XLVKVIEW_H_

#include "XLVk.h"
#include "XLGlView.h"
#include "XLVkSwapchain.h"

namespace stappler::xenolith::vk {

class View : public gl::View {
public:
	struct EngineOptions {
		// on some systems, we can not acquire next image until queue operations on previous image is finished
		// on this system, we wait on last swapchain pass fence before acquire swapchain image
		// swapchain-independent passes is not affected by this option
		bool waitOnSwapchainPassFence = false;

		// by default, we use vkAcquireNextImageKHR in lockfree manner, but in some cases blocking variant
		// is more preferable. If this option is set, vkAcquireNextImageKHR called with UIN64_MAX timeout
		// be careful not to block whole view's thread operation on this
		bool acquireImageImmediately = false;

		// Компенсировать частоту кадров с учётом предыдущих таймингов кадра
		// Wayland на nvidia странно использует vsync, что может снизить реальный FPS в два раза, если эта опция отключена
		// В некоторых случаях может снизить плавность анимаций
		bool flattenFrameRate = true;

		// Использовать внешний сигнал вертикальной синхронизации (система должна поддерживать)
		// В этом режиме готовые к презентации кадры ожидают сигнала, прежде, чем отправиться
		// Также, по сигналу система запрашиваает новый буфер для отрисовки следующего кадра
		// Если система не успела подготовить новый кадр - обновление пропускается
		bool followDisplayLink = false;

		// По умолчанию, FrameEmitter допускает только один кадр определенной RenderQueue в процессе отправки
		// (между vkQueueSubmit и освобождением соответсвующей VkFence). Исключение составляют проходы, помеченные как асинхронные
		// Если отключить это ограничение, единственным ограничением между vkQueueSubmit останутся внутренние примитивы синхронизации
		// В ряде случаев, неограниченная отправка буферов может привести к некорректной работе vkQueueSubmit и блокированию треда на
		// этой функции. Рекомендуется сохранять это ограничение. а для полной загрузки GPU использовать асинхронные пре- и пост-проходы
		bool enableFrameEmitterBarrier = true;
	};

	virtual ~View();

	virtual bool init(Loop &, Device &, gl::ViewInfo &&);

	virtual void threadInit() override;
	virtual void threadDispose() override;

	virtual void update(bool displayLink) override;

	virtual void run() override;
	virtual void runWithQueue(const Rc<RenderQueue> &) override;

	virtual void onAdded(Device &);
	virtual void onRemoved();

	virtual void deprecateSwapchain(bool fast = false) override;

	virtual bool present(Rc<ImageStorage> &&) override;
	virtual bool presentImmediate(Rc<ImageStorage> &&) override;
	virtual void invalidateTarget(Rc<ImageStorage> &&) override;

	virtual Rc<Ref> getSwapchainHandle() const override;

	void captureImage(StringView, const Rc<gl::ImageObject> &image, AttachmentLayout l) const;

	void scheduleFence(Rc<Fence> &&);

	virtual uint64_t getUpdateInterval() const { return 0; }

	virtual void mapWindow();

	vk::Device *getDevice() const { return _device; }

protected:
	using gl::View::init;

	virtual bool pollInput(bool frameReady);

	virtual gl::SurfaceInfo getSurfaceOptions() const;

	void invalidate();

	// Начать подготовку нового изображения для презентации
	// Создает объект кадра и начинает сбор данных для его рисования
	// Создает объект изображения и начинает цикл его захвата
	// Если режим acquireImageImmediately - блокирует поток до успешного захвата изображения
	// windowOffset - интервал от текущего момента. когда предполагается презентовать изображение
	//   Служит для ограничения частоты кадров
	void scheduleSwapchainImage(uint64_t windowOffset, bool immediate = false);

	// Попытаться захватить изображение для отрисовки кадра. Если задан флаг immediate
	// или включен режим followDisplayLink - блокирует поток до успешного захвата
	// В противном случае, если захват не удался - необходимо поробовать позже
	bool acquireScheduledImage(const Rc<SwapchainImage> &, bool immediate = false);

	virtual bool recreateSwapchain(gl::PresentMode);
	virtual bool createSwapchain(gl::SwapchainConfig &&cfg, gl::PresentMode presentMode);

	bool isImagePresentable(const gl::ImageObject &image, VkFilter &filter) const;

	// Отправляет запрос на кадр для изображения (оно может быть или не быть захваченным для отрисовки)
	// Запрос сперва собирает данные с помощью директора в основном потоке, затем начинает цикл построения кадра в графическом потоке
	void runFrameWithSwapchainImage(Rc<ImageStorage> &&);

	// Презентует отложенное подготовленное (кадр заверш]н) изображение
	void runScheduledPresent(Rc<SwapchainImage> &&);

	void presentWithQueue(DeviceQueue &, Rc<ImageStorage> &&);
	void invalidateSwapchainImage(Rc<ImageStorage> &&);

	Pair<uint64_t, uint64_t> updateFrameInterval();

	void waitForFences(uint64_t min);

	virtual void finalize();

	EngineOptions _options;

	bool _blockDeprecation = false;
	uint64_t _fenceOrder = 0;
	uint64_t _frameOrder = 0;
	Rc<Surface> _surface;
	Rc<Instance> _instance;
	Rc<Device> _device;
	Rc<SwapchainHandle> _swapchain;
	String _threadName;

	Rc<ImageStorage> _initImage;
	Set<Rc<Fence>> _fences;

	Vector<Rc<SwapchainImage>> _fenceImages;
	Vector<Rc<SwapchainImage>> _scheduledImages;
	Vector<Rc<SwapchainImage>> _scheduledPresent;
};

}

#endif /* XENOLITH_GL_VK_XLVKVIEW_H_ */
