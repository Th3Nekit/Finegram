package com.th3nekit.finegram.ink;

import static org.telegram.messenger.AndroidUtilities.dp;

import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.GradientDrawable;
import android.text.Editable;
import android.text.InputType;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.TypedValue;
import android.view.Gravity;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.tgnet.ConnectionsManager;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.ActionBar.BottomSheet;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.AlertsCreator;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.ui.Components.EditTextBoldCursor;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Stories.recorder.ButtonWithCounterView;

import java.util.function.Consumer;

public final class FGInkSheet {

    private FGInkSheet() {
    }

    public static void show(BaseFragment fragment, int account, long dialogId, int type, String cover, Consumer<String> send) {
        if (fragment == null || fragment.getParentActivity() == null || send == null) {
            return;
        }
        Context context = fragment.getParentActivity();
        Theme.ResourcesProvider resources = fragment.getResourceProvider();
        BottomSheet sheet = new BottomSheet(context, true, resources);
        sheet.fixNavigationBar(Theme.getColor(Theme.key_dialogBackground, resources));

        LinearLayout root = new LinearLayout(context);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(20), dp(18), dp(20), dp(14));

        boolean capsule = type == FGInkCodec.TYPE_CAPSULE;
        int accent = Theme.getColor(Theme.key_featuredStickers_addButton, resources);
        int text = Theme.getColor(Theme.key_dialogTextBlack, resources);
        int gray = Theme.getColor(Theme.key_dialogTextGray3, resources);

        FrameLayout badge = new FrameLayout(context);
        GradientDrawable circle = new GradientDrawable(GradientDrawable.Orientation.TL_BR,
                new int[]{accent, Theme.adaptHSV(accent, 0.12f, -0.12f)});
        circle.setShape(GradientDrawable.OVAL);
        badge.setBackground(circle);
        ImageView icon = new ImageView(context);
        icon.setScaleType(ImageView.ScaleType.CENTER);
        icon.setImageResource(capsule ? R.drawable.msg_stories_timer : R.drawable.msg_spoiler);
        icon.setColorFilter(new PorterDuffColorFilter(0xFFFFFFFF, PorterDuff.Mode.SRC_IN));
        icon.setScaleX(1.6f);
        icon.setScaleY(1.6f);
        badge.addView(icon, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.MATCH_PARENT));
        root.addView(badge, LayoutHelper.createLinear(64, 64, Gravity.CENTER_HORIZONTAL, 0, 0, 0, 12));

        TextView title = new TextView(context);
        title.setText(LocaleController.getString(capsule ? R.string.FG_Ink_Capsule : R.string.FG_Ink_Title));
        title.setTextColor(text);
        title.setTypeface(AndroidUtilities.bold());
        title.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 20);
        title.setGravity(Gravity.CENTER);
        root.addView(title, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 0, 0, 6));

        TextView subtitle = new TextView(context);
        subtitle.setTextColor(gray);
        subtitle.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 14);
        subtitle.setGravity(Gravity.CENTER);
        subtitle.setLineSpacing(dp(2), 1f);
        root.addView(subtitle, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 0, 0, 14));

        ButtonWithCounterView button = new ButtonWithCounterView(context, resources);

        if (!FGInk.exchanged(account, dialogId)) {
            boolean peer = FGInk.hasPeerKey(account, dialogId);
            boolean mine = FGInk.keySent(account, dialogId);
            subtitle.setText(LocaleController.getString(peer ? R.string.FG_Ink_PeerSent : mine ? R.string.FG_Ink_WaitPeer : R.string.FG_Ink_NoKey));
            button.setText(LocaleController.getString(!peer && mine ? R.string.FG_Ink_ResendKey : R.string.FG_Ink_SendKey), false);
            button.setOnClickListener(v -> {
                try {
                    String proposal = FGInk.proposal(account, dialogId);
                    sheet.dismiss();
                    send.accept(proposal);
                } catch (Exception e) {
                    BulletinFactory.of(fragment).createErrorBulletin(LocaleController.getString(R.string.FG_Ink_Failed)).show();
                }
            });
            root.addView(button, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 48, 0, 4, 0, 0));
            sheet.setCustomView(root);
            fragment.showDialog(sheet);
            return;
        }

        subtitle.setText(LocaleController.getString(capsule ? R.string.FG_Ink_CapsuleDesc : R.string.FG_Ink_Desc));

        String code = FGInk.fingerprint(account, dialogId);
        if (code != null) {
            TextView fingerprint = new TextView(context);
            fingerprint.setText(LocaleController.formatString(R.string.FG_Ink_Code, code));
            fingerprint.setTextColor(gray);
            fingerprint.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 13);
            fingerprint.setGravity(Gravity.CENTER);
            root.addView(fingerprint, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 0, 0, 12));
        }

        EditTextBoldCursor visible = field(context, resources, LocaleController.getString(R.string.FG_Ink_Cover), 3);
        visible.setText(cover == null ? "" : cover);
        root.addView(visible, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 0, 0, 10));

        EditTextBoldCursor hidden = field(context, resources, LocaleController.getString(capsule ? R.string.FG_Ink_CapsuleHidden : R.string.FG_Ink_Hidden), 8);
        root.addView(hidden, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 0, 0, 8));

        final int[] unlockAt = new int[1];
        if (capsule) {
            int now = ConnectionsManager.getInstance(account).getCurrentTime();
            unlockAt[0] = ((now + 86400) / 3600 + 1) * 3600;
            TextView pick = new TextView(context);
            pick.setTextColor(accent);
            pick.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 15);
            pick.setTypeface(AndroidUtilities.bold());
            pick.setGravity(Gravity.CENTER);
            pick.setPadding(dp(12), dp(10), dp(12), dp(10));
            pick.setBackground(Theme.createRadSelectorDrawable(Theme.multAlpha(accent, 0.12f), 10, 10));
            pick.setText(LocaleController.formatString(R.string.FG_Ink_OpensAt, LocaleController.formatDateTime(unlockAt[0], false)));
            pick.setOnClickListener(v -> AlertsCreator.createDatePickerDialog(context,
                    LocaleController.getString(R.string.FG_Ink_PickTitle), LocaleController.getString(R.string.FG_Ink_PickButton),
                    unlockAt[0], (notify, date, repeat) -> {
                        unlockAt[0] = date;
                        pick.setText(LocaleController.formatString(R.string.FG_Ink_OpensAt, LocaleController.formatDateTime(date, false)));
                    }).show());
            root.addView(pick, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 2, 0, 8));
        }

        TextView counter = new TextView(context);
        counter.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 12);
        counter.setGravity(Gravity.END);
        counter.setTextColor(gray);
        root.addView(counter, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 0, 0, 8));

        button.setText(LocaleController.getString(capsule ? R.string.FG_Ink_Seal : R.string.FG_Ink_Send), false);
        root.addView(button, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 48));

        Runnable update = () -> {
            String secret = hidden.getText().toString();
            int room = FGInk.room(visible.getText().toString(), type, secret);
            boolean ok = !TextUtils.isEmpty(secret.trim()) && room >= 0;
            button.setEnabled(ok);
            button.setAlpha(ok ? 1f : 0.5f);
            if (room < 0) {
                counter.setTextColor(Theme.getColor(Theme.key_text_RedRegular, resources));
                counter.setText(LocaleController.formatString(R.string.FG_Ink_TooLong, -room));
            } else {
                counter.setTextColor(gray);
                counter.setText(room < 600 ? LocaleController.formatString(R.string.FG_Ink_Left, room) : "");
            }
        };
        TextWatcher watcher = new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
            }

            @Override
            public void afterTextChanged(Editable s) {
                update.run();
            }
        };
        visible.addTextChangedListener(watcher);
        hidden.addTextChangedListener(watcher);
        update.run();

        button.setOnClickListener(v -> {
            String secret = hidden.getText().toString().trim();
            if (secret.isEmpty()) {
                return;
            }
            int unlock = unlockAt[0];
            if (capsule && unlock <= ConnectionsManager.getInstance(account).getCurrentTime()) {
                BulletinFactory.of(sheet.getContainer(), resources).createErrorBulletin(LocaleController.getString(R.string.FG_Ink_PastDate)).show();
                return;
            }
            try {
                String message = FGInk.compose(account, dialogId, visible.getText().toString().trim(), secret, type, unlock);
                sheet.dismiss();
                send.accept(message);
            } catch (Exception e) {
                BulletinFactory.of(sheet.getContainer(), resources).createErrorBulletin(LocaleController.getString(R.string.FG_Ink_Failed)).show();
            }
        });

        sheet.setCustomView(root);
        fragment.showDialog(sheet);
        AndroidUtilities.runOnUIThread(() -> {
            hidden.requestFocus();
            AndroidUtilities.showKeyboard(hidden);
        }, 250);
    }

    private static EditTextBoldCursor field(Context context, Theme.ResourcesProvider resources, String hint, int maxLines) {
        EditTextBoldCursor field = new EditTextBoldCursor(context);
        field.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 16);
        field.setTextColor(Theme.getColor(Theme.key_dialogTextBlack, resources));
        field.setHintText(hint);
        field.setHintColor(Theme.getColor(Theme.key_windowBackgroundWhiteHintText, resources));
        field.setCursorColor(Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resources));
        field.setCursorSize(dp(20));
        field.setCursorWidth(1.5f);
        field.setLineColors(Theme.getColor(Theme.key_windowBackgroundWhiteInputField, resources),
                Theme.getColor(Theme.key_windowBackgroundWhiteInputFieldActivated, resources),
                Theme.getColor(Theme.key_text_RedRegular, resources));
        field.setBackground(null);
        field.setPadding(0, dp(8), 0, dp(8));
        field.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE | InputType.TYPE_TEXT_FLAG_CAP_SENTENCES);
        field.setSingleLine(false);
        field.setMaxLines(maxLines);
        field.setGravity(Gravity.START | Gravity.TOP);
        return field;
    }
}
