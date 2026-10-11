import os

from java import dynamic_proxy as _java_dynamic_proxy
from org.telegram.tgnet import RequestDelegate

from finegram import runtime

PLUGINS_QUEUE = "plugins"

GLOBAL_QUEUE = "global"

EXTERNAL_NETWORK_QUEUE = "externalNetwork"

STAGE_QUEUE = "stage"

CACHE_CLEAR_QUEUE = "cacheClear"

SEARCH_QUEUE = "search"

PHONE_BOOK_QUEUE = "phoneBook"

THEME_QUEUE = "theme"

FILE_QUEUE = "file"

class RequestCallback(_java_dynamic_proxy(RequestDelegate)):

    def __init__(self, callback=None):
        super().__init__()
        self._callback = callback

    def run(self, response, error):
        self.onResponse(response, error)

    def onResponse(self, response, error):
        if self._callback is None:
            return
        try:
            self._callback(response, error)
        except Exception as failure:
            runtime.log("обработчик ответа упал: " + str(failure))

def send_request(request, callback=None, account=None):

    return runtime.send_request(request, callback, account)

def run_on_queue(func, queue_name=PLUGINS_QUEUE, delay=0):

    runtime.run_on_queue(func, delay, queue_name)

def _account(account=None):
    from org.telegram.messenger import UserConfig

    return UserConfig.selectedAccount if account is None else int(account)

def get_last_fragment():

    return runtime.get_last_fragment()

def get_account_instance(account=None):
    from org.telegram.messenger import AccountInstance

    return AccountInstance.getInstance(_account(account))

def get_user_config(account=None):
    return runtime.get_user_config(account)

def get_messages_controller(account=None):
    return runtime.get_messages_controller(account)

def get_contacts_controller(account=None):
    from org.telegram.messenger import ContactsController

    return ContactsController.getInstance(_account(account))

def get_media_data_controller(account=None):
    from org.telegram.messenger import MediaDataController

    return MediaDataController.getInstance(_account(account))

def get_connections_manager(account=None):
    from org.telegram.tgnet import ConnectionsManager

    return ConnectionsManager.getInstance(_account(account))

def get_current_datacenter_id(account=None):
    try:
        return int(get_connections_manager(account).getCurrentDatacenterId())
    except Exception:
        return 0

def get_messages_storage(account=None):
    from org.telegram.messenger import MessagesStorage

    return MessagesStorage.getInstance(_account(account))

def get_send_messages_helper(account=None):
    from org.telegram.messenger import SendMessagesHelper

    return SendMessagesHelper.getInstance(_account(account))

def get_file_loader(account=None):
    from org.telegram.messenger import FileLoader

    return FileLoader.getInstance(_account(account))

def get_download_controller(account=None):
    from org.telegram.messenger import DownloadController

    return DownloadController.getInstance(_account(account))

def get_notifications_controller(account=None):
    from org.telegram.messenger import NotificationsController

    return NotificationsController.getInstance(_account(account))

def get_notification_center(account=None):
    from org.telegram.messenger import NotificationCenter

    return NotificationCenter.getInstance(_account(account))

def get_media_controller():
    from org.telegram.messenger import MediaController

    return MediaController.getInstance()

def get_location_controller(account=None):
    from org.telegram.messenger import LocationController

    return LocationController.getInstance(_account(account))

def get_secret_chat_helper(account=None):
    from org.telegram.messenger import SecretChatHelper

    return SecretChatHelper.getInstance(_account(account))

def send_message(params, account=None):

    if not params or "peer" not in params:
        return False
    path = params.get("path")
    if path:
        return _send_file(int(params["peer"]), path, params, account)
    if params.get("message") is None:
        return False

    def send():
        try:
            from org.telegram.messenger import SendMessagesHelper

            send_params = SendMessagesHelper.SendMessageParams.of(
                str(params["message"]), int(params["peer"]), None, None, None,
                True, _java_entities(params.get("entities")), None, None,
                bool(params.get("notify", True)), 0, None, False)
            if "silent" in params:
                send_params.notify = not bool(params["silent"])
            if "reply_to" in params and params["reply_to"] is not None:
                send_params.replyToMsg = params["reply_to"]
            get_send_messages_helper(account).sendMessage(send_params)
            return True
        except Exception as error:
            runtime.log("сообщение не отправлено: " + str(error))
            return False

    return _on_ui_thread(send)

def _on_ui_thread(func):

    try:
        from android.os import Looper

        if Looper.myLooper() == Looper.getMainLooper():
            return func()
    except Exception:
        return func()
    runtime.run_on_ui_thread(func, 0)
    return True

def _java_entities(entities):

    if entities is None or not isinstance(entities, (list, tuple)):
        return entities
    from java.util import ArrayList

    result = ArrayList()
    for entity in entities:
        to_tl = getattr(entity, "to_tl", None)
        if callable(to_tl):
            entity = to_tl()
        if entity is not None:
            result.add(entity)
    return result

def _send_file(peer, path, params, account):

    if not os.path.exists(path):
        runtime.log("файла нет: " + str(path))
        return False

    def send():
        try:
            from org.telegram.messenger import SendMessagesHelper
            from java.util import ArrayList

            paths = ArrayList()
            paths.add(str(path))
            SendMessagesHelper.prepareSendingDocuments(
                get_account_instance(account), paths, paths, None,
                params.get("caption"), None, peer, params.get("reply_to"), None, None, None,
                bool(params.get("notify", True)), 0, None, None, 0, False, None
            )
            return True
        except Exception as error:
            runtime.log("файл не отправлен: " + str(error))
            return False

    return _on_ui_thread(send)

def send_text(peer, text, account=None, **extra):

    params = {"peer": peer, "message": text}
    params.update(extra)
    return send_message(params, account)

def send_document(peer, file_path, caption="", account=None, **extra):
    params = {"peer": peer, "path": file_path, "caption": caption}
    params.update(extra)
    return send_message(params, account)

def send_photo(peer, file_path, caption="", account=None, **extra):
    return send_document(peer, file_path, caption, account, **extra)

def send_video(peer, file_path, caption="", account=None, **extra):
    return send_document(peer, file_path, caption, account, **extra)

def send_audio(peer, file_path, caption="", account=None, **extra):
    return send_document(peer, file_path, caption, account, **extra)

def edit_message(message_object, text, account=None):

    try:
        from org.telegram.tgnet import TLRPC

        request = TLRPC.TL_messages_editMessage()
        request.peer = get_messages_controller(account).getInputPeer(message_object.getDialogId())
        request.id = int(message_object.getId())
        request.message = str(text)
        request.flags |= 2048
        send_request(request, None, account)
        return True
    except Exception as error:
        runtime.log("сообщение не изменено: " + str(error))
        return False

_delegate_class = None

def _get_delegate_class(dynamic_proxy, NotificationCenter):

    global _delegate_class
    if _delegate_class is not None:
        return _delegate_class

    class _Delegate(dynamic_proxy(NotificationCenter.NotificationCenterDelegate)):
        def __init__(self, work):
            super().__init__()
            self._work = work

        def didReceivedNotification(self, notification_id, account_index, args):
            try:
                self._work(notification_id, account_index, args)
            except Exception as error:
                runtime.log("обработчик события упал: " + str(error))

    _delegate_class = _Delegate
    return _delegate_class

def observe_notifications(handler, *notification_ids, **kwargs):

    account = kwargs.get("account")
    try:
        from java import dynamic_proxy
        from org.telegram.messenger import NotificationCenter

        center = get_notification_center(account)

        delegate = _get_delegate_class(dynamic_proxy, NotificationCenter)(handler)
        for notification_id in notification_ids:
            center.addObserver(delegate, notification_id)

        class _Subscription:
            def remove(self):
                for one in notification_ids:
                    try:
                        center.removeObserver(delegate, one)
                    except Exception:
                        pass

        return _Subscription()
    except Exception as error:
        runtime.log("не подписаться на события: " + str(error))
        return None

def get_queue_by_name(queue_name):

    return runtime.get_queue(queue_name)

def get_notifications_settings(account=None):

    return get_messages_controller(account).notificationsPreferences

def run_on_ui_thread(func, delay=0):

    from android_utils import run_on_ui_thread as _run

    _run(func, delay)

class NotificationCenterDelegate:

    def did_received_notification(self, notification_id, account, args):
        pass

    def didReceivedNotification(self, notification_id, account, args):
        return self.did_received_notification(notification_id, account, args)

def log(message):

    from android_utils import log as _log

    _log(message)

def get_current_fragment():
    return get_last_fragment()

def get_selected_account():
    from org.telegram.messenger import UserConfig

    return UserConfig.selectedAccount

def dynamic_proxy(*interfaces):
    from java import dynamic_proxy as _dynamic_proxy

    return _dynamic_proxy(*interfaces)
