/*
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.outbound;

import android.content.Context;
import android.content.DialogInterface;
import android.os.Build;
import android.text.InputFilter;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.BulletinFactory;

import java.util.Locale;

public final class FGSubscriptionUi {
    private FGSubscriptionUi() {
    }

    public static String text(String key) {
        return LocaleController.getString(key);
    }

    public static String format(String key, Object... args) {
        Locale locale = LocaleController.getInstance().getCurrentLocale();
        return String.format(locale == null ? Locale.getDefault() : locale, text(key), args);
    }

    public static String name(FGSubscription source) {
        return source.name.isEmpty() ? text("FG_Subscription_DefaultName") : source.name;
    }

    private static boolean alive(BaseFragment fragment) {
        return fragment != null && !fragment.isFinished && fragment.getParentActivity() != null;
    }

    public static void add(BaseFragment fragment, Runnable changed) {
        edit(fragment, null, changed);
    }

    public static void edit(BaseFragment fragment, FGSubscription source, Runnable changed) {
        if (!alive(fragment)) return;
        Context context = fragment.getParentActivity();
        LinearLayout body = new LinearLayout(context);
        body.setOrientation(LinearLayout.VERTICAL);
        body.setPadding(AndroidUtilities.dp(24), 0, AndroidUtilities.dp(24), AndroidUtilities.dp(12));
        body.setLayoutDirection(LocaleController.isRTL ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR);

        EditText name = field(context, "FG_Subscription_Name", 128, false);
        EditText url = field(context, "FG_Subscription_Url", 4096, true);
        url.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        url.setTextDirection(View.TEXT_DIRECTION_LTR);
        name.setText(source == null ? "" : source.name);
        if (source != null) url.setHint(text("FG_Subscription_UrlSaved"));
        body.addView(label(context, "FG_Subscription_Name"));
        body.addView(name, fieldParams());
        body.addView(label(context, "FG_Subscription_Url"));
        body.addView(url, fieldParams());

        FGSubscriptionOptions.IdentityMode[] mode = {source == null ? FGSubscriptionOptions.IdentityMode.NONE : source.options.identityMode};
        if (mode[0] == FGSubscriptionOptions.IdentityMode.LEAN_EXPLICIT) mode[0] = FGSubscriptionOptions.IdentityMode.EXPLICIT;
        body.addView(label(context, "FG_Subscription_Identity"));
        RadioGroup identity = new RadioGroup(context);
        identity.setOrientation(LinearLayout.VERTICAL);
        for (FGSubscriptionOptions.IdentityMode option : new FGSubscriptionOptions.IdentityMode[]{
                FGSubscriptionOptions.IdentityMode.NONE, FGSubscriptionOptions.IdentityMode.OWN_DEVICE, FGSubscriptionOptions.IdentityMode.EXPLICIT}) {
            RadioButton button = new RadioButton(context);
            button.setId(View.generateViewId());
            button.setTag(option);
            button.setText(identityName(option));
            button.setTextSize(15);
            button.setTextColor(Theme.getColor(Theme.key_dialogTextBlack));
            button.setButtonTintList(android.content.res.ColorStateList.valueOf(Theme.getColor(Theme.key_dialogTextBlue)));
            button.setMinHeight(AndroidUtilities.dp(48));
            identity.addView(button, new RadioGroup.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));
            if (option == mode[0]) identity.check(button.getId());
        }
        body.addView(identity, fieldParams());
        TextView explanation = label(context, "FG_Subscription_IdentityNoneInfo");
        explanation.setTextColor(Theme.getColor(Theme.key_dialogTextGray2));
        LinearLayout.LayoutParams explanationParams = fieldParams();
        explanationParams.topMargin = AndroidUtilities.dp(8);
        body.addView(explanation, explanationParams);
        EditText hwid = field(context, "FG_Subscription_LeanHwid", 128, false);
        hwid.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        hwid.setTextDirection(View.TEXT_DIRECTION_LTR);
        hwid.setText(source == null ? "" : source.options.leanHwid);
        body.addView(hwid, fieldParams());

        Runnable showIdentity = () -> {
            boolean lean = mode[0] == FGSubscriptionOptions.IdentityMode.EXPLICIT;
            hwid.setVisibility(lean ? View.VISIBLE : View.GONE);
            explanation.setText(text(lean ? "FG_Subscription_IdentityLeanInfo" : mode[0] == FGSubscriptionOptions.IdentityMode.OWN_DEVICE
                    ? "FG_Subscription_IdentityOwnInfo" : "FG_Subscription_IdentityNoneInfo"));
        };
        showIdentity.run();
        identity.setOnCheckedChangeListener((group, checked) -> {
            RadioButton button = group.findViewById(checked);
            if (button != null) {
                mode[0] = (FGSubscriptionOptions.IdentityMode) button.getTag();
                showIdentity.run();
            }
        });

        FGSubscriptionOptions initialOptions = source == null ? FGSubscriptionOptions.none() : source.options;
        body.addView(label(context, "FG_Subscription_UserAgent"));
        String[] agentNames = {"Finegram", "v2rayNG", "Hiddify", text("FG_Subscription_UserAgentCustom")};
        Spinner agent = new Spinner(context, Spinner.MODE_DROPDOWN);
        agent.setMinimumHeight(AndroidUtilities.dp(48));
        agent.setPopupBackgroundDrawable(Theme.createRoundRectDrawable(AndroidUtilities.dp(12), Theme.getColor(Theme.key_dialogBackground)));
        agent.setAdapter(new ArrayAdapter<String>(context, android.R.layout.simple_spinner_item, agentNames) {
            private View row(int position, View convertView) {
                TextView view = convertView instanceof TextView ? (TextView) convertView : new TextView(context);
                view.setText(getItem(position));
                view.setTextSize(16);
                view.setTextColor(Theme.getColor(Theme.key_dialogTextBlack));
                view.setGravity(Gravity.CENTER_VERTICAL | Gravity.START);
                view.setMinHeight(AndroidUtilities.dp(48));
                view.setPadding(AndroidUtilities.dp(12), AndroidUtilities.dp(10), AndroidUtilities.dp(12), AndroidUtilities.dp(10));
                return view;
            }

            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                return row(position, convertView);
            }

            @Override
            public View getDropDownView(int position, View convertView, ViewGroup parent) {
                return row(position, convertView);
            }
        });
        body.addView(agent, fieldParams());
        EditText customAgent = field(context, "FG_Subscription_UserAgentCustom", 256, false);
        customAgent.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        customAgent.setTextDirection(View.TEXT_DIRECTION_LTR);
        customAgent.setText(initialOptions.customUserAgent);
        customAgent.setVisibility(initialOptions.userAgentMode == FGSubscriptionOptions.UserAgentMode.CUSTOM ? View.VISIBLE : View.GONE);
        body.addView(customAgent, fieldParams());
        agent.setSelection(initialOptions.userAgentMode.ordinal());
        agent.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                customAgent.setVisibility(FGSubscriptionOptions.UserAgentMode.values()[position] == FGSubscriptionOptions.UserAgentMode.CUSTOM ? View.VISIBLE : View.GONE);
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });
        TextView agentInfo = label(context, "FG_Subscription_UserAgentInfo");
        agentInfo.setTextColor(Theme.getColor(Theme.key_dialogTextGray2));
        body.addView(agentInfo, fieldParams());

        TextView error = label(context, "FG_Subscription_ErrorNetwork");
        error.setTextColor(Theme.getColor(Theme.key_text_RedBold));
        error.setVisibility(View.GONE);
        LinearLayout.LayoutParams errorParams = fieldParams();
        errorParams.topMargin = AndroidUtilities.dp(12);
        body.addView(error, errorParams);
        ScrollView scroll = new ScrollView(context);
        scroll.setFillViewport(false);
        scroll.addView(body);
        AlertDialog dialog = new AlertDialog.Builder(context)
                .setTitle(text(source == null ? "FG_Subscription_Add" : "FG_Subscription_Edit"))
                .setView(scroll)
                .setPositiveButton(LocaleController.getString(source == null ? R.string.Add : R.string.Save), null)
                .setNegativeButton(LocaleController.getString(R.string.Cancel), null).create();
        if (fragment.showDialog(dialog, ignored -> AndroidUtilities.hideKeyboard(url)) == null) return;
        View save = dialog.getButton(DialogInterface.BUTTON_POSITIVE);
        if (save == null) return;
        save.setOnClickListener(v -> {
            if (!alive(fragment)) return;
            String value = url.getText().toString().trim();
            if (source != null && value.isEmpty()) value = source.url;
            final String sourceUrl = value;
            String title = name.getText().toString().trim();
            FGSubscriptionOptions identityOptions = mode[0] == FGSubscriptionOptions.IdentityMode.EXPLICIT ? FGSubscriptionOptions.explicit(hwid.getText().toString())
                    : mode[0] == FGSubscriptionOptions.IdentityMode.OWN_DEVICE ? FGSubscriptionOptions.ownDevice() : FGSubscriptionOptions.none();
            if (source != null && source.options.identityMode == FGSubscriptionOptions.IdentityMode.LEAN_EXPLICIT
                    && mode[0] == FGSubscriptionOptions.IdentityMode.EXPLICIT
                    && source.options.leanHwid.equalsIgnoreCase(identityOptions.leanHwid)) {
                identityOptions = FGSubscriptionOptions.lean(identityOptions.leanHwid);
            }
            FGSubscriptionOptions options = identityOptions.withUserAgent(FGSubscriptionOptions.UserAgentMode.values()[agent.getSelectedItemPosition()], customAgent.getText().toString());
            try {
                options.validate();
                FGSubscriptionHttp.source(value, mode[0] == FGSubscriptionOptions.IdentityMode.OWN_DEVICE ? "0" : options.leanHwid);
            } catch (FGSubscriptionError failure) {
                error.setText(errorText(failure.code, failure.httpStatus));
                error.setVisibility(View.VISIBLE);
                return;
            }
            error.setText(text("FG_Subscription_Updating"));
            error.setTextColor(Theme.getColor(Theme.key_dialogTextGray2));
            error.setVisibility(View.VISIBLE);
            save.setEnabled(false);
            name.setEnabled(false);
            url.setEnabled(false);
            hwid.setEnabled(false);
            agent.setEnabled(false);
            customAgent.setEnabled(false);
            for (int i = 0; i < identity.getChildCount(); i++) identity.getChildAt(i).setEnabled(false);
            AndroidUtilities.hideKeyboard(url);
            FGSubscriptionManager.Callback callback = result -> {
                if (changed != null) changed.run();
                if (!alive(fragment) || !dialog.isShowing()) return;
                if (result.success) {
                    dialog.dismiss();
                    report(fragment, result);
                } else {
                    save.setEnabled(true);
                    name.setEnabled(true);
                    url.setEnabled(true);
                    hwid.setEnabled(true);
                    agent.setEnabled(true);
                    customAgent.setEnabled(true);
                    for (int i = 0; i < identity.getChildCount(); i++) identity.getChildAt(i).setEnabled(true);
                    error.setTextColor(Theme.getColor(Theme.key_text_RedBold));
                    error.setText(errorText(result.error, result.httpStatus));
                }
            };
            if (source == null) FGSubscriptionManager.getInstance().add(title, sourceUrl, options, callback);
            else FGSubscriptionManager.getInstance().update(source.id, title, sourceUrl, options, callback);
        });
    }

    public static void settings(BaseFragment fragment, FGSubscription source, Runnable changed) {
        if (!alive(fragment)) return;
        fragment.showDialog(new AlertDialog.Builder(fragment.getParentActivity()).setTitle(name(source))
                .setItems(new CharSequence[]{text("FG_Subscription_Edit"), text("FG_Subscription_Remove")}, (dialog, which) -> {
                    AndroidUtilities.runOnUIThread(() -> {
                        if (which == 0) edit(fragment, source, changed);
                        else confirmRemove(fragment, source, changed);
                    });
                }).setNegativeButton(LocaleController.getString(R.string.Cancel), null).create());
    }

    private static void confirmRemove(BaseFragment fragment, FGSubscription source, Runnable changed) {
        if (!alive(fragment)) return;
        AlertDialog dialog = new AlertDialog.Builder(fragment.getParentActivity()).setTitle(text("FG_Subscription_Remove"))
                .setMessage(text("FG_Subscription_RemoveInfo"))
                .setPositiveButton(LocaleController.getString(R.string.Delete), (ignored, which) ->
                        FGSubscriptionManager.getInstance().delete(source.id, result -> {
                            if (changed != null) changed.run();
                            report(fragment, result);
                        }))
                .setNegativeButton(LocaleController.getString(R.string.Cancel), null).create();
        fragment.showDialog(dialog);
        TextView button = (TextView) dialog.getButton(DialogInterface.BUTTON_POSITIVE);
        if (button != null) button.setTextColor(Theme.getColor(Theme.key_text_RedBold));
    }

    public static void report(BaseFragment fragment, FGSubscriptionManager.Result result) {
        if (!alive(fragment)) return;
        String message = result.success ? text(result.selectionCleared ? "FG_Subscription_SelectionRemoved" : "FG_Subscription_Done")
                : errorText(result.error, result.httpStatus);
        if (result.success) {
            for (FGSubscription source : result.subscriptions) {
                if (source.id.equals(result.subscriptionId) && source.skipped > 0) {
                    message += "\n" + format("FG_Subscription_Skipped", source.skipped);
                    break;
                }
            }
        }
        if (result.success) BulletinFactory.of(fragment).createSuccessBulletin(message).show();
        else BulletinFactory.of(fragment).createErrorBulletin(message).show();
    }

    public static String errorText(FGSubscriptionError.Code code, int httpStatus) {
        if (code == null) return text("FG_Subscription_ErrorNetwork");
        switch (code) {
            case INVALID_URL: return text("FG_Subscription_ErrorUrl");
            case INVALID_IDENTITY: return text("FG_Subscription_ErrorHwid");
            case INVALID_USER_AGENT: return text("FG_Subscription_ErrorUserAgent");
            case HWID_REQUIRED: return text("FG_Subscription_ErrorHwidRequired");
            case APP_NOT_SUPPORTED: return text("FG_Subscription_ErrorAppNotSupported");
            case HTTP_STATUS: return format("FG_Subscription_ErrorHttp", httpStatus);
            case TOO_MANY_REDIRECTS:
            case HTTPS_DOWNGRADE: return text("FG_Subscription_ErrorRedirect");
            case BODY_TOO_LARGE:
            case TOO_MANY_SERVERS: return text("FG_Subscription_ErrorSize");
            case NO_SERVERS: return text("FG_Subscription_ErrorEmpty");
            case SUSPICIOUS_SHRINK: return text("FG_Subscription_ErrorShrink");
            case UNSUPPORTED_FORMAT:
                return text("FG_Subscription_ErrorUnsupportedFormat");
            case UNSUPPORTED_PROTOCOL:
                return text("FG_Subscription_ErrorUnsupportedProtocol");
            case UNSUPPORTED_LINK: return text("FG_Subscription_ErrorUnsupported");
            case INVALID_ENCODING:
            case INVALID_FORMAT:
            case INVALID_LINK: return text("FG_Subscription_ErrorFormat");
            case STORAGE: return text("FG_Subscription_ErrorStorage");
            case NOT_FOUND: return text("FG_Subscription_ErrorMissing");
            case LIMIT: return text("FG_Subscription_ErrorLimit");
            case BUSY: return text("FG_Subscription_ErrorBusy");
            case TIMEOUT: return text("FG_Subscription_ErrorTimeout");
            default: return text("FG_Subscription_ErrorNetwork");
        }
    }

    private static String identityName(FGSubscriptionOptions.IdentityMode mode) {
        return text(mode == FGSubscriptionOptions.IdentityMode.EXPLICIT || mode == FGSubscriptionOptions.IdentityMode.LEAN_EXPLICIT ? "FG_Subscription_IdentityLean"
                : mode == FGSubscriptionOptions.IdentityMode.OWN_DEVICE ? "FG_Subscription_IdentityOwn" : "FG_Subscription_IdentityNone");
    }

    private static TextView label(Context context, String key) {
        TextView label = new TextView(context);
        label.setText(text(key));
        label.setTextSize(14);
        label.setIncludeFontPadding(false);
        label.setTextColor(Theme.getColor(Theme.key_dialogTextBlack));
        label.setGravity(Gravity.START | Gravity.CENTER_VERTICAL);
        return label;
    }

    private static EditText field(Context context, String key, int limit, boolean multiline) {
        EditText field = new EditText(context);
        field.setTextSize(16);
        field.setTextColor(Theme.getColor(Theme.key_dialogTextBlack));
        field.setHintTextColor(Theme.getColor(Theme.key_dialogTextHint));
        field.setBackground(Theme.createEditTextDrawable(context, true));
        field.setHint(text(key));
        field.setSingleLine(!multiline);
        field.setMaxLines(multiline ? 3 : 1);
        field.setFilters(new InputFilter[]{new InputFilter.LengthFilter(limit)});
        field.setPadding(0, AndroidUtilities.dp(12), 0, AndroidUtilities.dp(12));
        if (Build.VERSION.SDK_INT >= 26) field.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO);
        return field;
    }

    private static LinearLayout.LayoutParams fieldParams() {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        params.bottomMargin = AndroidUtilities.dp(12);
        return params;
    }
}
