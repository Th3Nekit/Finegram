/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.chats.helpers;

import static org.telegram.messenger.LocaleController.formatString;
import static org.telegram.messenger.LocaleController.getString;

import android.app.DatePickerDialog;
import android.app.TimePickerDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.DatePicker;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.BaseController;
import org.telegram.messenger.ChatObject;
import org.telegram.messenger.DialogObject;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.MediaDataController;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.R;
import org.telegram.messenger.UserConfig;
import org.telegram.messenger.Utilities;
import org.telegram.tgnet.ConnectionsManager;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Cells.CheckBoxCell;
import org.telegram.ui.Components.AlertsCreator;
import org.telegram.ui.Components.AvatarDrawable;
import org.telegram.ui.Components.BackupImageView;
import org.telegram.ui.Components.Bulletin;
import org.telegram.ui.Components.Forum.ForumUtilities;
import org.telegram.ui.Components.LayoutHelper;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.concurrent.CountDownLatch;

import com.th3nekit.finegram.core.FinegramLogger;

public class MessageHelper extends BaseController {

    private static final MessageHelper[] Instance = new MessageHelper[UserConfig.MAX_ACCOUNT_COUNT];

    public MessageHelper(int num) {
        super(num);
    }

    public static MessageHelper getInstance(int num) {
        MessageHelper localInstance = Instance[num];
        if (localInstance == null) {
            synchronized (MessageHelper.class) {
                localInstance = Instance[num];
                if (localInstance == null) {
                    Instance[num] = localInstance = new MessageHelper(num);
                }
            }
        }
        return localInstance;
    }

    private AlertDialog progressDialog;
    private OwnMessageHistorySearch ownHistorySearch;
    private int ownHistoryRequest;

    public void createDeleteHistoryAlert(BaseFragment fragment, TLRPC.Chat chat, TLRPC.TL_forumTopic forumTopic, long mergeDialogId, Theme.ResourcesProvider resourcesProvider) {
        createDeleteHistoryAlert(fragment, chat, forumTopic, mergeDialogId, -1, resourcesProvider);
    }

    private void createDeleteHistoryAlert(BaseFragment fragment, TLRPC.Chat chat, TLRPC.TL_forumTopic forumTopic, long mergeDialogId, int before, Theme.ResourcesProvider resourcesProvider) {
        if (fragment == null || fragment.getParentActivity() == null || chat == null) {
            return;
        }

        Context context = fragment.getParentActivity();
        AlertDialog.Builder builder = new AlertDialog.Builder(context, resourcesProvider);

        CheckBoxCell cell = before == -1 && forumTopic == null && ChatObject.isChannel(chat) && ChatObject.canUserDoAction(chat, ChatObject.ACTION_DELETE_MESSAGES) ? new CheckBoxCell(context, 1, resourcesProvider) : null;

        TextView messageTextView = new TextView(context);
        messageTextView.setTextColor(Theme.getColor(Theme.key_dialogTextBlack, resourcesProvider));
        messageTextView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 16);
        messageTextView.setGravity((LocaleController.isRTL ? Gravity.RIGHT : Gravity.LEFT) | Gravity.TOP);

        FrameLayout frameLayout = new FrameLayout(context) {
            @Override
            protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
                super.onMeasure(widthMeasureSpec, heightMeasureSpec);
                if (cell != null) {
                    setMeasuredDimension(getMeasuredWidth(), getMeasuredHeight() + cell.getMeasuredHeight() + AndroidUtilities.dp(7));
                }
            }
        };
        builder.setView(frameLayout);

        AvatarDrawable avatarDrawable = new AvatarDrawable();
        avatarDrawable.setTextSize(AndroidUtilities.dp(12));
        avatarDrawable.setInfo(chat);

        BackupImageView imageView = new BackupImageView(context);
        imageView.setRoundRadius(AndroidUtilities.dp(20));
        if (forumTopic != null) {
            if (forumTopic.id == 1) {
                imageView.setImageDrawable(ForumUtilities.createGeneralTopicDrawable(context, 0.75f, Theme.getColor(Theme.key_chat_inMenu, resourcesProvider), false));
            } else {
                ForumUtilities.setTopicIcon(imageView, forumTopic, false, true, resourcesProvider);
            }
        } else {
            imageView.setForUserOrChat(chat, avatarDrawable);
        }
        frameLayout.addView(imageView, LayoutHelper.createFrame(40, 40, (LocaleController.isRTL ? Gravity.RIGHT : Gravity.LEFT) | Gravity.TOP, 22, 5, 22, 0));

        TextView textView = new TextView(context);
        textView.setTextColor(Theme.getColor(Theme.key_actionBarDefaultSubmenuItem, resourcesProvider));
        textView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 20);
        textView.setTypeface(AndroidUtilities.bold());
        textView.setLines(1);
        textView.setMaxLines(1);
        textView.setSingleLine(true);
        textView.setGravity((LocaleController.isRTL ? Gravity.RIGHT : Gravity.LEFT) | Gravity.CENTER_VERTICAL);
        textView.setEllipsize(TextUtils.TruncateAt.END);
        textView.setText(getString(R.string.FG_DeleteAllFromSelf));

        frameLayout.addView(textView, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, (LocaleController.isRTL ? Gravity.RIGHT : Gravity.LEFT) | Gravity.TOP, (LocaleController.isRTL ? 21 : 76), 11, (LocaleController.isRTL ? 76 : 21), 0));
        frameLayout.addView(messageTextView, LayoutHelper.createFrame(LayoutHelper.WRAP_CONTENT, LayoutHelper.WRAP_CONTENT, (LocaleController.isRTL ? Gravity.RIGHT : Gravity.LEFT) | Gravity.TOP, 24, 57, 24, 9));

        if (cell != null) {
            boolean sendAs = ChatObject.getSendAsPeerId(chat, getMessagesController().getChatFull(chat.id), true) != getUserConfig().getClientUserId();
            cell.setBackground(Theme.getSelectorDrawable(false));
            cell.setText(getString(R.string.FG_DeleteAllFromSelfAdmin), "", !ChatObject.shouldSendAnonymously(chat) && !sendAs, false);
            cell.setPadding(LocaleController.isRTL ? AndroidUtilities.dp(16) : AndroidUtilities.dp(8), 0, LocaleController.isRTL ? AndroidUtilities.dp(8) : AndroidUtilities.dp(16), 0);
            frameLayout.addView(cell, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, 48, Gravity.BOTTOM | Gravity.LEFT, 0, 0, 0, 0));
            cell.setOnClickListener(v -> {
                CheckBoxCell cell1 = (CheckBoxCell) v;
                cell1.setChecked(!cell1.isChecked(), true);
            });
        }

        if (before > 0) {
            messageTextView.setText(AndroidUtilities.replaceTags(formatString(R.string.FG_DeleteAllFromSelfAlertBefore, LocaleController.formatDateForBan(before))));
        } else {
            messageTextView.setText(AndroidUtilities.replaceTags(getString(R.string.FG_DeleteAllFromSelfAlert)));
        }

        progressDialog = new AlertDialog(fragment.getParentActivity(), AlertDialog.ALERT_TYPE_SPINNER);
        progressDialog.setCanCancel(false);

        builder.setNeutralButton(getString(R.string.FG_DeleteAllFromSelfBefore), (dialog, which) -> showBeforeDatePickerAlert(fragment, before1 -> createDeleteHistoryAlert(fragment, chat, forumTopic, mergeDialogId, before1, resourcesProvider)));
        builder.setPositiveButton(getString(R.string.DeleteAll), (dialogInterface, i) -> {
            if (cell != null && cell.isChecked()) {
                showDeleteHistoryBulletin(fragment, 0, false, () -> getMessagesController().deleteUserChannelHistory(chat, getUserConfig().getCurrentUser(), null, 0), resourcesProvider);
            } else {
                AndroidUtilities.runOnUIThread(() -> {
                    try {
                        progressDialog.show();
                    } catch (Exception e) {
                        FinegramLogger.e(e);
                    }
                });
                deleteUserHistoryWithSearch(fragment, -chat.id, forumTopic != null ? forumTopic.id : 0, forumTopic != null ? forumTopic.id : 0, mergeDialogId, before == -1 ? getConnectionsManager().getCurrentTime() : before, (count, deleteAction) -> {
                    showDeleteHistoryBulletin(fragment, count, true, deleteAction, resourcesProvider);
                    AndroidUtilities.runOnUIThread(() -> {
                        try {
                            if (progressDialog != null && progressDialog.isShowing()) progressDialog.dismiss();
                        } catch (Exception e) {
                            FinegramLogger.e(e);
                        }
                    });
                });
            }
        });
        builder.setNegativeButton(getString(R.string.Cancel), null);
        AlertDialog alertDialog = builder.create();
        fragment.showDialog(alertDialog);
        TextView button = (TextView) alertDialog.getButton(DialogInterface.BUTTON_POSITIVE);
        if (button != null) {
            button.setTextColor(Theme.getColor(Theme.key_text_RedBold, resourcesProvider));
        }
    }

    public void createDeleteOwnMessagesAlert(BaseFragment fragment, long dialogId, int topicId, long mergeDialogId, int before) {
        if (fragment == null || fragment.getParentActivity() == null || dialogId == 0) return;
        Theme.ResourcesProvider provider = fragment.getResourceProvider();
        AlertDialog.Builder builder = new AlertDialog.Builder(fragment.getParentActivity(), provider);
        builder.setTitle(getString(R.string.FG_DeleteAllFromSelf));
        builder.setMessage(before > 0
                ? AndroidUtilities.replaceTags(formatString(R.string.FG_DeleteAllFromSelfAlertBefore, LocaleController.formatDateForBan(before)))
                : AndroidUtilities.replaceTags(getString(R.string.FG_DeleteAllFromSelfAlert)));
        builder.setNeutralButton(getString(R.string.FG_OlderThanDays), (dialog, which) ->
                showOlderDaysPicker(fragment, days -> createDeleteOwnMessagesAlert(fragment, dialogId, topicId, mergeDialogId,
                        OwnMessageHistorySearch.cutoff(getConnectionsManager().getCurrentTime(), days))));
        builder.setPositiveButton(getString(R.string.DeleteAll), (dialog, which) -> {
            progressDialog = new AlertDialog(fragment.getParentActivity(), AlertDialog.ALERT_TYPE_SPINNER);
            progressDialog.show();
            deleteUserHistoryWithSearch(fragment, dialogId, topicId, topicId, mergeDialogId, before,
                    (count, action) -> showDeleteHistoryBulletin(fragment, count, true, action, provider));
        });
        builder.setNegativeButton(getString(R.string.Cancel), null);
        AlertDialog alert = builder.create();
        fragment.showDialog(alert);
        TextView button = (TextView) alert.getButton(DialogInterface.BUTTON_POSITIVE);
        if (button != null) button.setTextColor(Theme.getColor(Theme.key_text_RedBold, provider));
    }

    private void showOlderDaysPicker(BaseFragment fragment, Utilities.Callback<Integer> callback) {
        if (fragment.getParentActivity() == null) return;
        org.telegram.ui.Components.NumberPicker picker = new org.telegram.ui.Components.NumberPicker(fragment.getParentActivity(), fragment.getResourceProvider());
        picker.setMinValue(1);
        picker.setMaxValue(3650);
        picker.setValue(30);
        picker.setWrapSelectorWheel(false);
        picker.setFormatter(value -> LocaleController.formatPluralString("Days", value));
        FrameLayout frame = new FrameLayout(fragment.getParentActivity());
        frame.addView(picker, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, 180, Gravity.CENTER, 24, 8, 24, 8));
        AlertDialog.Builder builder = new AlertDialog.Builder(fragment.getParentActivity(), fragment.getResourceProvider());
        builder.setTitle(getString(R.string.FG_OlderThanDays));
        builder.setView(frame);
        builder.setPositiveButton(getString(R.string.Set), (dialog, which) -> callback.run(picker.getValue()));
        builder.setNegativeButton(getString(R.string.Cancel), null);
        fragment.showDialog(builder.create());
    }

    public static class ReplyMarkupButtonsTexts {
        private final ArrayList<ArrayList<String>> texts = new ArrayList<>();

        public ArrayList<ArrayList<String>> getTexts() {
            return texts;
        }
    }

    public static class PollTexts {
        private final ArrayList<String> texts = new ArrayList<>();

        public ArrayList<String> getTexts() {
            return texts;
        }
    }

    private void showBeforeDatePickerAlert(BaseFragment fragment, Utilities.Callback<Integer> callback) {
        Context context = fragment.getParentActivity();
        AlertDialog.Builder builder = new AlertDialog.Builder(context);
        builder.setTitle(getString(R.string.FG_DeleteAllFromSelfBefore));
        builder.setItems(new CharSequence[]{
                LocaleController.formatPluralString("Days", 1),
                LocaleController.formatPluralString("Weeks", 1),
                LocaleController.formatPluralString("Months", 1),
                getString(R.string.UserRestrictionsCustom)
        }, (dialog1, which) -> {
            switch (which) {
                case 0:
                    callback.run(getConnectionsManager().getCurrentTime() - 60 * 60 * 24);
                    break;
                case 1:
                    callback.run(getConnectionsManager().getCurrentTime() - 60 * 60 * 24 * 7);
                    break;
                case 2:
                    callback.run(getConnectionsManager().getCurrentTime() - 60 * 60 * 24 * 30);
                    break;
                case 3: {
                    Calendar calendar = Calendar.getInstance();
                    DatePickerDialog dateDialog = new DatePickerDialog(context, (view1, year1, month, dayOfMonth1) -> {
                        TimePickerDialog timeDialog = new TimePickerDialog(context, (view11, hourOfDay, minute) -> {
                            calendar.set(year1, month, dayOfMonth1, hourOfDay, minute);
                            callback.run((int) (calendar.getTimeInMillis() / 1000));
                        }, calendar.get(Calendar.HOUR_OF_DAY), calendar.get(Calendar.MINUTE), true);
                        timeDialog.setButton(DialogInterface.BUTTON_POSITIVE, getString(R.string.Set), timeDialog);
                        timeDialog.setButton(DialogInterface.BUTTON_NEGATIVE, getString(R.string.Cancel), (dialog3, which3) -> {
                        });
                        fragment.showDialog(timeDialog);
                    }, calendar.get(Calendar.YEAR), calendar.get(Calendar.MONTH), calendar.get(Calendar.DAY_OF_MONTH));

                    final DatePicker datePicker = dateDialog.getDatePicker();

                    datePicker.setMinDate(1375315200000L);
                    datePicker.setMaxDate(System.currentTimeMillis());

                    dateDialog.setButton(DialogInterface.BUTTON_POSITIVE, getString(R.string.Set), dateDialog);
                    dateDialog.setButton(DialogInterface.BUTTON_NEGATIVE, getString(R.string.Cancel), (dialog2, which2) -> {
                    });
                    dateDialog.setOnShowListener(dialog12 -> {
                        int count = datePicker.getChildCount();
                        for (int b = 0; b < count; b++) {
                            View child = datePicker.getChildAt(b);
                            ViewGroup.LayoutParams layoutParams = child.getLayoutParams();
                            layoutParams.width = LayoutHelper.MATCH_PARENT;
                            child.setLayoutParams(layoutParams);
                        }
                    });
                    fragment.showDialog(dateDialog);
                    break;
                }
            }
            builder.getDismissRunnable().run();
        });
        fragment.showDialog(builder.create());
    }

    public static void showDeleteHistoryBulletin(BaseFragment fragment, int count, boolean search, Runnable delayedAction, Theme.ResourcesProvider resourcesProvider) {
        if (fragment.getParentActivity() == null) {
            if (delayedAction != null) {
                delayedAction.run();
            }
            return;
        }
        Bulletin.ButtonLayout buttonLayout;
        if (search) {
            final Bulletin.TwoLineLottieLayout layout = new Bulletin.TwoLineLottieLayout(fragment.getParentActivity(), resourcesProvider);
            layout.titleTextView.setText(getString(R.string.FG_DeleteAllFromSelfDone));
            layout.subtitleTextView.setText(LocaleController.formatPluralString("MessagesDeletedHint", count));
            layout.setTimer();
            buttonLayout = layout;
        } else {
            final Bulletin.LottieLayout layout = new Bulletin.LottieLayout(fragment.getParentActivity(), resourcesProvider);
            layout.textView.setText(getString(R.string.FG_DeleteAllFromSelfDone));
            layout.setTimer();
            buttonLayout = layout;
        }
        buttonLayout.setButton(new Bulletin.UndoButton(fragment.getParentActivity(), true, resourcesProvider).setDelayedAction(delayedAction));
        Bulletin.make(fragment, buttonLayout, Bulletin.DURATION_PROLONG).show();
    }

    public void deleteUserHistoryWithSearch(BaseFragment fragment, final long dialogId, final int topicId) {
        deleteUserHistoryWithSearch(fragment, dialogId, topicId, 0, 0, -1, null);
    }

    private void deleteUserHistoryWithSearch(BaseFragment fragment, final long dialogId, final int topicId, int replyMessageId, final long mergeDialogId, int before, SearchMessagesResultCallback callback) {
        if (ownHistorySearch != null) ownHistorySearch.cancel();
        if (ownHistoryRequest != 0) getConnectionsManager().cancelRequest(ownHistoryRequest, true);
        final int cutoff = before < 0 ? OwnMessageHistorySearch.cutoff(getConnectionsManager().getCurrentTime(), 0) : before;
        final TLRPC.InputPeer myself = MessagesController.getInputPeer(getUserConfig().getCurrentUser());
        final OwnMessageHistorySearch[] job = new OwnMessageHistorySearch[1];
        job[0] = new OwnMessageHistorySearch((peerId, topic, date, offset, complete) -> {
            TLRPC.TL_messages_search request = new TLRPC.TL_messages_search();
            request.peer = getMessagesController().getInputPeer(peerId);
            request.q = "";
            request.limit = 100;
            request.offset_id = offset;
            request.filter = new TLRPC.TL_inputMessagesFilterEmpty();
            request.max_date = Math.max(0, date - 1);
            boolean monoForum = getMessagesStorage().isMonoForum(peerId);
            if (!monoForum) {
                request.from_id = myself;
                request.flags |= 1;
            }
            if (topic != 0) {
                if (monoForum) {
                    request.saved_peer_id = getMessagesController().getInputPeer(topic);
                    request.flags |= 4;
                } else {
                    request.top_msg_id = topic;
                    request.flags |= 2;
                }
            }
            ownHistoryRequest = getConnectionsManager().sendRequest(request, (response, error) -> AndroidUtilities.runOnUIThread(() -> {
                if (ownHistorySearch != job[0]) return;
                ownHistoryRequest = 0;
                if (error != null) {
                    complete.complete(null, error.text);
                } else if (response instanceof TLRPC.messages_Messages page && !(page instanceof TLRPC.TL_messages_messagesNotModified)) {
                    ArrayList<OwnMessageHistorySearch.Entry> entries = new ArrayList<>();
                    for (TLRPC.Message message : page.messages) {
                        entries.add(new OwnMessageHistorySearch.Entry(message.id, message.date, message.out, message.post));
                    }
                    complete.complete(entries, null);
                } else {
                    complete.complete(null, "SEARCH_FAILED");
                }
            }), ConnectionsManager.RequestFlagFailOnServerErrors);
        }, (found, error) -> {
            if (ownHistorySearch != job[0]) return;
            ownHistorySearch = null;
            ownHistoryRequest = 0;
            if (progressDialog != null && progressDialog.isShowing()) progressDialog.dismiss();
            if (error != null) {
                AlertsCreator.showSimpleAlert(fragment, getString(R.string.ErrorOccurred) + "\n" + error);
                return;
            }
            final int[] count = {0};
            ArrayList<Runnable> actions = new ArrayList<>();
            for (var entry : found.entrySet()) {
                ArrayList<Integer> ids = entry.getValue();
                count[0] += ids.size();
                for (int i = 0; i < ids.size(); i += 100) {
                    ArrayList<Integer> batch = new ArrayList<>(ids.subList(i, Math.min(ids.size(), i + 100)));
                    long target = entry.getKey();
                    actions.add(() -> getMessagesController().deleteMessages(batch, null, null, target,
                            target == dialogId ? topicId : 0, true, 0));
                }
            }
            if (count[0] == 0) {
                AlertsCreator.showSimpleAlert(fragment, getString(R.string.FG_NoOwnMessages));
                return;
            }
            Runnable deleteAction = () -> { for (Runnable action : actions) action.run(); };
            if (callback != null) callback.run(count[0], deleteAction);
            else deleteAction.run();
        }, dialogId, mergeDialogId, topicId, cutoff);
        ownHistorySearch = job[0];
        if (progressDialog != null) {
            progressDialog.setCanCancel(true);
            progressDialog.setOnCancelListener(dialog -> {
                if (ownHistorySearch == job[0]) {
                    ownHistorySearch.cancel();
                    ownHistorySearch = null;
                    if (ownHistoryRequest != 0) getConnectionsManager().cancelRequest(ownHistoryRequest, true);
                    ownHistoryRequest = 0;
                }
            });
        }
        job[0].start();
    }

    private interface SearchMessagesResultCallback {
        void run(int count, Runnable deleteAction);
    }

}
