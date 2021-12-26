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

#include "XLNetworkController.h"
#include "XLNetworkHandle.h"
#include "XLApplication.h"

#include <curl/curl.h>

namespace stappler::xenolith::network {

Controller::Controller(Application *app, StringView name) : _application(app), _name(name.str()) {
	_shouldQuit.test_and_set();
	_thread = StdThread(ThreadHandlerInterface::workerThread, this, nullptr);
}

Controller::~Controller() {
	_shouldQuit.clear();
	curl_multi_wakeup((CURLM *)_handle);
	_thread.join();
}

void Controller::onUploadProgress(const Rc<Handle> &handle, int64_t total, int64_t now) {
	_application->performOnMainThread([handle, total, now] {
		handle->notifyOnUploadProgress(total, now);
	});
}

void Controller::onDownloadProgress(const Rc<Handle> &handle, int64_t total, int64_t now) {
	_application->performOnMainThread([handle, total, now] {
		handle->notifyOnDownloadProgress(total, now);
	});
}

bool Controller::onComplete(const Rc<Handle> &handle) {
	_application->performOnMainThread([handle] {
		handle->notifyOnComplete();
	});
	return true;
}

void Controller::threadInit() {
	_handle = curl_multi_init();
}

bool Controller::worker() {
	if (!_shouldQuit.test_and_set()) {
		cancel();
		return false;
	}

	do {
		std::unique_lock<Mutex> lock(_mutex);
		for (auto &it : _pending) {
			auto handle = it.get();
			auto h = curl_easy_init();
			auto i = _handles.emplace(h, pair(move(it), NetworkHandle::Context())).first;

			auto sg = i->second.first->getSharegroup();
			if (!sg.empty()) {
				i->second.second.share = getSharegroup(sg);
			}

			i->second.second.userdata = this;
			i->second.second.curl = h;
			i->second.second.handle = handle;

			i->second.second.handle->setDownloadProgress([this, h = handle] (int64_t total, int64_t now) -> int {
				onDownloadProgress(h, total, now);
				return 0;
			});

			i->second.second.handle->setUploadProgress([this,  h = handle] (int64_t total, int64_t now) -> int {
				onUploadProgress(h, total, now);
				return 0;
			});

			if (i->second.first->shouldSignRequest()) {
				sign(*i->second.first, i->second.second);
			}

			i->second.first->prepare(&i->second.second, nullptr);

			curl_multi_add_handle((CURLM *)_handle, h);
		}
		_pending.clear();
	} while (0);

	int running = 0;
	auto err = curl_multi_perform((CURLM *)_handle, &running);
	if (err != CURLM_OK) {
		log::text("CURL", toString("Fail to perform multi: ", err));
		return false;
	}

	int timeout = 16;
	if (running == 0) {
		timeout = 1000;
	}

	err = curl_multi_poll((CURLM *)_handle, NULL, 0, timeout, nullptr);
	if (err != CURLM_OK) {
		log::text("CURL", toString("Fail to poll multi: ", err));
		return false;
	}

	struct CURLMsg *msg = nullptr;
	do {
		int msgq = 0;
		msg = curl_multi_info_read((CURLM *)_handle, &msgq);
		if (msg && (msg->msg == CURLMSG_DONE)) {
			CURL *e = msg->easy_handle;
			curl_multi_remove_handle((CURLM *)_handle, e);

			auto it = _handles.find(e);
			if (it != _handles.end()) {
				it->second.second.code = msg->data.result;
				it->second.first->finalize(&it->second.second, nullptr);
				if (!onComplete(it->second.first)) {
					_handles.erase(it);
					cancel();
					return false;
				}
				_handles.erase(it);
			}

			curl_easy_cleanup(e);
		}
	} while (msg);

	return true;
}

void Controller::cancel() {
	if (_handle) {
		for (auto &it : _handles) {
			curl_multi_remove_handle((CURLM *)_handle, it.first);
			it.second.second.code = CURLE_FAILED_INIT;
			it.second.first->finalize(&it.second.second, nullptr);
			curl_easy_cleanup(it.first);
		}

		curl_multi_cleanup((CURLM *)_handle);

		for (auto &it : _sharegroups) {
			curl_share_cleanup((CURLSH *)it.second);
		}

		_handles.clear();
		_sharegroups.clear();

		_handle = nullptr;
	}
}

void *Controller::getSharegroup(StringView name) {
	auto it = _sharegroups.find(name);
	if (it != _sharegroups.end()) {
		return it->second;
	}

	auto sharegroup = curl_share_init();
	curl_share_setopt((CURLSH *)sharegroup, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
	curl_share_setopt((CURLSH *)sharegroup, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
	curl_share_setopt((CURLSH *)sharegroup, CURLSHOPT_SHARE, CURL_LOCK_DATA_PSL);

	_sharegroups.emplace(name.str(), sharegroup);
	return sharegroup;
}

void Controller::sign(Handle &handle, NetworkHandle::Context &ctx) const {
	String date = Time::now().toHttp();
	StringStream message;

	message << handle.getUrl() << "\r\n";
	message << "X-ApplicationName: " << _application->getData().bundleName << "\r\n";
	message << "X-ApplicationVersion: " << _application->getApplicationVersionCode() << "\r\n";
	message << "X-ClientDate: " << date << "\r\n";
	message << "User-Agent: " << _application->getUserAgent() << "\r\n";

	auto msg = message.str();
	auto sig = string::Sha512::hmac(msg, _signKey);

	ctx.headers = curl_slist_append(ctx.headers, toString("X-ClientDate: ", date).data());
	ctx.headers = curl_slist_append(ctx.headers, toString("X-Stappler-Sign: ", base64url::encode(sig)).data());

	handle.setUserAgent(_application->getUserAgent());
}

void Controller::run(const Rc<Handle> &handle) {
	std::unique_lock<Mutex> lock(_mutex);
	_pending.emplace_back(handle);
	curl_multi_wakeup((CURLM *)_handle);
}

}
