/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREEN_CAPTURE_PORTAL_INTERFACE_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREEN_CAPTURE_PORTAL_INTERFACE_H_

#include <gio/gio.h>

#include <string>

#include "modules/portal/portal_request_response.h"
#include "modules/portal/scoped_glib.h"
#include "modules/portal/xdg_desktop_portal_utils.h"
#include "modules/portal/xdg_session_details.h"

namespace webrtc {
namespace xdg_portal {

using SessionClosedSignalHandler = void (*)(GDBusConnection*,
                                            const char*,
                                            const char*,
                                            const char*,
                                            const char*,
                                            GVariant*,
                                            gpointer);

class RTC_EXPORT ScreenCapturePortalInterface {
 public:
  virtual ~ScreenCapturePortalInterface() {}

  virtual xdg_portal::SessionDetails GetSessionDetails() { return {}; }

  virtual void Start() {}

  virtual void Stop() {}

  virtual void OnPortalDone(xdg_portal::RequestResponse result) {}

  virtual void RequestSession(GDBusProxy* proxy) {}

  void RequestSessionUsingProxy(GAsyncResult* result);

  void OnSessionRequestResult(GDBusProxy* proxy, GAsyncResult* result);

  void RegisterSessionClosedSignalHandler(
      const SessionClosedSignalHandler session_close_signal_handler,
      GVariant* parameters,
      GDBusConnection* connection,
      std::string& session_handle,
      guint& session_closed_signal_id);

  void OnStartRequestResult(GDBusProxy* proxy, GAsyncResult* result);
};

}
}

#endif
