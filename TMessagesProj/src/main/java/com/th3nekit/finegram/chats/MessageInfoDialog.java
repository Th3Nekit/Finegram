package com.th3nekit.finegram.chats;

import org.telegram.messenger.DialogObject;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.MessageObject;
import org.telegram.messenger.R;
import org.telegram.messenger.UserObject;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.ActionBar.Theme;

import java.util.ArrayList;
import java.util.Date;

public final class MessageInfoDialog {
    public static final int OPTION_MESSAGE_INFO = 2063;

    private MessageInfoDialog() {
    }

    public static void addMenuItem(MessageObject message, boolean restricted, boolean scheduled,
                                   ArrayList<CharSequence> items, ArrayList<Integer> options, ArrayList<Integer> icons) {
        if (!canShow(message, restricted, scheduled)) {
            return;
        }
        items.add(LocaleController.getString(R.string.FG_MessageInfoTitle));
        options.add(OPTION_MESSAGE_INFO);
        icons.add(R.drawable.msg_info);
    }

    public static void show(BaseFragment fragment, MessageObject message, boolean restricted, boolean scheduled,
                            Theme.ResourcesProvider resourcesProvider) {
        if (fragment == null || fragment.getParentActivity() == null || !canShow(message, restricted, scheduled)) {
            return;
        }
        StringBuilder text = new StringBuilder();
        appendDate(text, R.string.FG_MessageInfoSent, message.messageOwner.date);
        if (message.isEdited()) {
            appendDate(text, R.string.FG_MessageInfoEdited, message.messageOwner.edit_date);
        }
        if (message.messageOwner.fwd_from != null) {
            appendDate(text, R.string.FG_MessageInfoOriginal, message.messageOwner.fwd_from.date);
        }
        fragment.showDialog(new AlertDialog.Builder(fragment.getParentActivity(), resourcesProvider)
                .setTitle(LocaleController.getString(R.string.FG_MessageInfoTitle))
                .setMessage(text)
                .setPositiveButton(LocaleController.getString(R.string.OK), null)
                .create());
    }

    private static boolean canShow(MessageObject message, boolean restricted, boolean scheduled) {
        if (restricted || scheduled || message == null || message.messageOwner == null || message.getId() <= 0 ||
                message.scheduled || message.isSponsored() || message.isEphemeral() || message.isSecret() ||
                DialogObject.isEncryptedDialog(message.getDialogId()) || message.getDialogId() == UserObject.VERIFY ||
                message.hasProtectedContent() || message.isSecretMedia() || message.isVoiceOnce() || message.isRoundOnce() ||
                message.needDrawBluredPreview() || message.type == MessageObject.TYPE_PAID_MEDIA ||
                MessageObject.getMedia(message) instanceof TLRPC.TL_messageMediaPaidMedia || message.isPaidSuggestedPostProtected() ||
                message.messageOwner instanceof TLRPC.TL_messageService ||
                message.messageOwner.action != null && !(message.messageOwner.action instanceof TLRPC.TL_messageActionEmpty)) {
            return false;
        }
        return message.messageOwner.date > 0 || message.isEdited() && message.messageOwner.edit_date > 0 ||
                message.messageOwner.fwd_from != null && message.messageOwner.fwd_from.date > 0;
    }

    private static void appendDate(StringBuilder text, int label, int seconds) {
        if (seconds <= 0) {
            return;
        }
        if (text.length() > 0) {
            text.append("\n\n");
        }
        Date date = new Date(seconds * 1000L);
        LocaleController locale = LocaleController.getInstance();
        text.append(LocaleController.getString(label)).append('\n');
        text.append(LocaleController.formatString(R.string.formatDateAtTime,
                locale.getFormatterYearMax().format(date), locale.getFormatterDayWithSeconds().format(date)));
    }
}
