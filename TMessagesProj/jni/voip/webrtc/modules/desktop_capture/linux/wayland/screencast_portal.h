/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_PORTAL_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_PORTAL_H_

#include <gio/gio.h>

#include <string>

#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/desktop_capture/linux/wayland/screen_capture_portal_interface.h"
#include "modules/portal/pipewire_utils.h"
#include "modules/portal/portal_request_response.h"
#include "modules/portal/xdg_desktop_portal_utils.h"
#include "modules/portal/xdg_session_details.h"

namespace webrtc {

class RTC_EXPORT ScreenCastPortal
    : public xdg_portal::ScreenCapturePortalInterface {
 public:
  using ProxyRequestResponseHandler = void (*)(GObject* object,
                                               GAsyncResult* result,
                                               gpointer user_data);

  using SourcesRequestResponseSignalHandler =
      void (*)(GDBusConnection* connection,
               const char* sender_name,
               const char* object_path,
               const char* interface_name,
               const char* signal_name,
               GVariant* parameters,
               gpointer user_data);

  enum class CursorMode : uint32_t {

    kHidden = 0b01,

    kEmbedded = 0b10,

    kMetadata = 0b100
  };

  enum class PersistMode : uint32_t {

    kDoNotPersist = 0b00,

    kTransient = 0b01,

    kPersistent = 0b10
  };

  class PortalNotifier {
   public:
    virtual void OnScreenCastRequestResult(xdg_portal::RequestResponse result,
                                           uint32_t stream_node_id,
                                           int fd) = 0;
    virtual void OnScreenCastSessionClosed() = 0;

   protected:
    PortalNotifier() = default;
    virtual ~PortalNotifier() = default;
  };

  ScreenCastPortal(CaptureType type, PortalNotifier* notifier);
  ScreenCastPortal(CaptureType type,
                   PortalNotifier* notifier,
                   ProxyRequestResponseHandler proxy_request_response_handler,
                   SourcesRequestResponseSignalHandler
                       sources_request_response_signal_handler,
                   gpointer user_data,

                   bool prefer_cursor_embedded = false);

  ~ScreenCastPortal();

  void Start() override;
  void Stop() override;
  xdg_portal::SessionDetails GetSessionDetails() override;

  void OnPortalDone(xdg_portal::RequestResponse result) override;

  void RequestSession(GDBusProxy* proxy) override;

  void SetSessionDetails(const xdg_portal::SessionDetails& session_details);
  uint32_t pipewire_stream_node_id();
  void SourcesRequest();
  void OpenPipeWireRemote();

  void SetPersistMode(ScreenCastPortal::PersistMode mode);
  void SetRestoreToken(const std::string& token);
  std::string RestoreToken() const;

 private:

  enum class CaptureSourceType : uint32_t {
    kScreen = 0b01,
    kWindow = 0b10,
    kAnyScreenContent = kScreen | kWindow
  };
  static CaptureSourceType ToCaptureSourceType(CaptureType type);

  PortalNotifier* notifier_;

  uint32_t pw_stream_node_id_ = 0;

  int pw_fd_ = kInvalidPipeWireFd;

  std::string restore_token_;

  CaptureSourceType capture_source_type_ =
      ScreenCastPortal::CaptureSourceType::kScreen;

  CursorMode cursor_mode_ = CursorMode::kMetadata;

  PersistMode persist_mode_ = ScreenCastPortal::PersistMode::kDoNotPersist;

  ProxyRequestResponseHandler proxy_request_response_handler_;
  SourcesRequestResponseSignalHandler sources_request_response_signal_handler_;
  gpointer user_data_;

  GDBusConnection* connection_ = nullptr;
  GDBusProxy* proxy_ = nullptr;
  GCancellable* cancellable_ = nullptr;
  std::string portal_handle_;
  std::string session_handle_;
  std::string sources_handle_;
  std::string start_handle_;
  guint session_request_signal_id_ = 0;
  guint sources_request_signal_id_ = 0;
  guint start_request_signal_id_ = 0;
  guint session_closed_signal_id_ = 0;

  void UnsubscribeSignalHandlers();
  static void OnProxyRequested(GObject* object,
                               GAsyncResult* result,
                               gpointer user_data);
  static void OnSessionRequested(GDBusProxy* proxy,
                                 GAsyncResult* result,
                                 gpointer user_data);
  static void OnSessionRequestResponseSignal(GDBusConnection* connection,
                                             const char* sender_name,
                                             const char* object_path,
                                             const char* interface_name,
                                             const char* signal_name,
                                             GVariant* parameters,
                                             gpointer user_data);
  static void OnSessionClosedSignal(GDBusConnection* connection,
                                    const char* sender_name,
                                    const char* object_path,
                                    const char* interface_name,
                                    const char* signal_name,
                                    GVariant* parameters,
                                    gpointer user_data);
  static void OnSourcesRequested(GDBusProxy* proxy,
                                 GAsyncResult* result,
                                 gpointer user_data);
  static void OnSourcesRequestResponseSignal(GDBusConnection* connection,
                                             const char* sender_name,
                                             const char* object_path,
                                             const char* interface_name,
                                             const char* signal_name,
                                             GVariant* parameters,
                                             gpointer user_data);

  void StartRequest();
  static void OnStartRequested(GDBusProxy* proxy,
                               GAsyncResult* result,
                               gpointer user_data);
  static void OnStartRequestResponseSignal(GDBusConnection* connection,
                                           const char* sender_name,
                                           const char* object_path,
                                           const char* interface_name,
                                           const char* signal_name,
                                           GVariant* parameters,
                                           gpointer user_data);

  static void OnOpenPipeWireRemoteRequested(GDBusProxy* proxy,
                                            GAsyncResult* result,
                                            gpointer user_data);
};

}

#endif
