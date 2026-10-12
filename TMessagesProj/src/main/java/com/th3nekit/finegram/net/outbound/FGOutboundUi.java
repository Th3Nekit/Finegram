/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.outbound;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.text.TextUtils;

import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.R;
import org.telegram.messenger.SharedConfig;
import org.telegram.proxy.ProxySettings;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.Components.BulletinFactory;

public final class FGOutboundUi {

    private FGOutboundUi() {
    }

    public static boolean offerFromClipboard(BaseFragment fragment) {
        if (fragment == null || fragment.getParentActivity() == null) return false;

        if (!FGOutboundCore.isSupported()) return false;
        final String link = clipboard();
        if (!FGOutboundLink.looksLikeLink(link)) return false;

        final FGOutboundLink.Parsed parsed = FGOutboundLink.parse(link);
        if (parsed == null) {

            BulletinFactory.of(fragment).createErrorBulletin(
                    LocaleController.getString(R.string.FG_Outbound_BadLink)).show();
            return true;
        }

        askForLink(fragment, link);
        return true;
    }

    public static boolean isAvailable() {
        return FGOutboundCore.isSupported();
    }

    public static void askForLink(BaseFragment fragment) {
        askForLink(fragment, clipboard());
    }

    private static void askForLink(BaseFragment fragment, String fromClipboard) {
        if (fragment == null || fragment.getParentActivity() == null) return;

        final android.widget.EditText field = new android.widget.EditText(fragment.getParentActivity());
        field.setTextColor(org.telegram.ui.ActionBar.Theme.getColor(
                org.telegram.ui.ActionBar.Theme.key_dialogTextBlack));
        field.setHintTextColor(org.telegram.ui.ActionBar.Theme.getColor(
                org.telegram.ui.ActionBar.Theme.key_dialogTextHint));
        field.setBackground(org.telegram.ui.ActionBar.Theme.createEditTextDrawable(
                fragment.getParentActivity(), true));
        field.setHint("vless:// · hy2://");
        field.setSingleLine(false);
        field.setMaxLines(4);
        field.setPadding(org.telegram.messenger.AndroidUtilities.dp(12),
                org.telegram.messenger.AndroidUtilities.dp(8),
                org.telegram.messenger.AndroidUtilities.dp(12),
                org.telegram.messenger.AndroidUtilities.dp(8));

        final android.widget.EditText name = nameField(fragment);
        if (FGOutboundLink.looksLikeLink(fromClipboard)) {
            field.setText(fromClipboard);
            field.setSelection(fromClipboard.length());
            FGOutboundLink.Parsed initial = FGOutboundLink.parse(fromClipboard);
            if (initial != null) name.setText(initial.title);
        }

        final android.widget.LinearLayout wrapper = new android.widget.LinearLayout(fragment.getParentActivity());
        wrapper.setOrientation(android.widget.LinearLayout.VERTICAL);
        wrapper.setPadding(org.telegram.messenger.AndroidUtilities.dp(18), 0,
                org.telegram.messenger.AndroidUtilities.dp(18), 0);
        wrapper.addView(field);
        wrapper.addView(name);

        final AlertDialog.Builder builder = new AlertDialog.Builder(fragment.getParentActivity());
        builder.setTitle(LocaleController.getString(R.string.FG_Outbound_AddTitle));
        builder.setView(wrapper);
        builder.setPositiveButton(LocaleController.getString(R.string.Add), (dialog, which) -> {
            final String link = field.getText() == null ? "" : field.getText().toString().trim();
            final FGOutboundLink.Parsed parsed = FGOutboundLink.parse(link);
            if (parsed == null) {
                BulletinFactory.of(fragment).createErrorBulletin(
                        LocaleController.getString(R.string.FG_Outbound_BadLink)).show();
                return;
            }
            add(fragment, parsed, link, name.getText().toString());
        });
        builder.setNegativeButton(LocaleController.getString(R.string.Cancel), null);
        fragment.showDialog(builder.create());
    }

    private static android.widget.EditText nameField(BaseFragment fragment) {
        android.widget.EditText field = new android.widget.EditText(fragment.getParentActivity());
        field.setTextSize(16);
        field.setTextColor(org.telegram.ui.ActionBar.Theme.getColor(org.telegram.ui.ActionBar.Theme.key_dialogTextBlack));
        field.setHintTextColor(org.telegram.ui.ActionBar.Theme.getColor(org.telegram.ui.ActionBar.Theme.key_dialogTextHint));
        field.setBackground(org.telegram.ui.ActionBar.Theme.createEditTextDrawable(fragment.getParentActivity(), true));
        field.setHint(LocaleController.getString(R.string.FG_ProxyName));
        field.setSingleLine(true);
        field.setPadding(org.telegram.messenger.AndroidUtilities.dp(12), org.telegram.messenger.AndroidUtilities.dp(8),
                org.telegram.messenger.AndroidUtilities.dp(12), org.telegram.messenger.AndroidUtilities.dp(8));
        return field;
    }

    public static void editName(BaseFragment fragment, SharedConfig.ProxyInfo info) {
        if (fragment == null || fragment.getParentActivity() == null || info == null) return;
        android.widget.EditText field = nameField(fragment);
        field.setText(com.exteragram.messenger.proxy.ProxyController.getInstance().getName(info));
        field.setSelection(field.length());
        android.widget.FrameLayout wrapper = new android.widget.FrameLayout(fragment.getParentActivity());
        wrapper.setPadding(org.telegram.messenger.AndroidUtilities.dp(18), 0, org.telegram.messenger.AndroidUtilities.dp(18), 0);
        wrapper.addView(field);
        AlertDialog.Builder builder = new AlertDialog.Builder(fragment.getParentActivity());
        builder.setTitle(LocaleController.getString(R.string.ProxyDetails));
        builder.setView(wrapper);
        builder.setPositiveButton(LocaleController.getString(R.string.Save), (dialog, which) ->
                com.exteragram.messenger.proxy.ProxyController.getInstance().setName(info, field.getText().toString()));
        builder.setNegativeButton(LocaleController.getString(R.string.Cancel), null);
        fragment.showDialog(builder.create());
    }

    private static void add(BaseFragment fragment, FGOutboundLink.Parsed parsed, String link, String name) {
        try {
            final ProxySettings settings = ProxySettings.builder()
                    .setType(ProxySettings.Type.OUTBOUND)
                    .setAddress(parsed.host)
                    .setPort(parsed.port)

                    .setSecret(link)
                    .build();
            final SharedConfig.ProxyInfo info = SharedConfig.addProxy(new SharedConfig.ProxyInfo(settings));
            com.exteragram.messenger.proxy.ProxyController.getInstance().setName(info, name);
            BulletinFactory.of(fragment).createSimpleBulletin(R.raw.done,
                    LocaleController.getString(R.string.FG_Outbound_Added)).show();
        } catch (Throwable t) {
            BulletinFactory.of(fragment).createErrorBulletin(
                    LocaleController.getString(R.string.FG_Outbound_BadLink)).show();
        }
    }

    private static String clipboard() {
        try {
            final ClipboardManager manager = (ClipboardManager) ApplicationLoader.applicationContext
                    .getSystemService(Context.CLIPBOARD_SERVICE);
            if (manager == null || !manager.hasPrimaryClip()) return null;
            final ClipData clip = manager.getPrimaryClip();
            if (clip == null || clip.getItemCount() == 0) return null;
            final CharSequence text = clip.getItemAt(0).coerceToText(ApplicationLoader.applicationContext);
            return TextUtils.isEmpty(text) ? null : text.toString().trim();
        } catch (Throwable t) {
            return null;
        }
    }
}
