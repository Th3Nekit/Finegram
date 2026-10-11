/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.preferences;

import static org.telegram.messenger.AndroidUtilities.dp;
import static org.telegram.messenger.LocaleController.formatString;
import static org.telegram.messenger.LocaleController.getString;

import android.text.InputType;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.View;

import org.json.JSONArray;
import org.json.JSONObject;
import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.R;
import org.telegram.messenger.browser.Browser;
import org.telegram.ui.ActionBar.AlertDialog;
import android.view.Gravity;
import android.widget.FrameLayout;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.ui.Components.EditTextBoldCursor;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;

import com.th3nekit.finegram.plugins.FGBundledPlugins;
import com.th3nekit.finegram.plugins.FGDexPlugins;
import com.th3nekit.finegram.plugins.FGPluginShare;
import com.th3nekit.finegram.plugins.FGPluginUpdates;
import com.th3nekit.finegram.plugins.FGPluginsController;
import com.th3nekit.finegram.preferences.cells.FGPluginCustomCell;
import com.th3nekit.finegram.preferences.cells.FGPluginHeaderCell;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

public class PluginDetailsPreferencesEntry extends BaseCGPreferencesEntry {

    private static final int DELETE_ROW = 2;
    private static final int SETTINGS_ROW_BASE = 100;

    private interface PluginView {
        String id();

        String name();

        String version();

        String author();

        String description();

        String icon();

        boolean enabled();

        boolean enable();

        void disable();

        void delete();

        String lastError();

        boolean hasSettings();
    }

    private final PluginView plugin;
    private JSONArray settings = new JSONArray();

    private FGPluginHeaderCell header;

    private int screen;

    private final String nestedTitle;

    private int parentScreen = -1;
    private int parentIndex = -1;

    public PluginDetailsPreferencesEntry(FGPluginsController.Plugin source) {
        this.plugin = new PythonPluginView(source);
        this.screen = 0;
        this.nestedTitle = null;
    }

    public PluginDetailsPreferencesEntry(FGDexPlugins.Plugin source) {
        this.plugin = new DexPluginView(source);
        this.screen = 0;
        this.nestedTitle = null;
    }

    @Override
    public boolean onFragmentCreate() {

        return plugin != null && super.onFragmentCreate();
    }

    public PluginDetailsPreferencesEntry(String pluginId) {
        PluginView found = null;
        for (FGPluginsController.Plugin candidate : FGPluginsController.INSTANCE.installed()) {
            if (candidate.getId().equals(pluginId)) {
                found = new PythonPluginView(candidate);
                break;
            }
        }
        if (found == null) {
            for (FGDexPlugins.Plugin candidate : FGDexPlugins.INSTANCE.installed()) {
                if (candidate.getId().equals(pluginId)) {
                    found = new DexPluginView(candidate);
                    break;
                }
            }
        }
        this.plugin = found;
        this.screen = 0;
        this.nestedTitle = null;
    }

    private PluginDetailsPreferencesEntry(PluginView source, int screen, String title, JSONArray items,
                                          int parentScreen, int parentIndex) {
        this.plugin = source;
        this.screen = screen;
        this.nestedTitle = title;
        this.settings = items;
        this.parentScreen = parentScreen;
        this.parentIndex = parentIndex;
    }

    public String getPluginId() {
        return plugin == null ? null : plugin.id();
    }

    private static final class PythonPluginView implements PluginView {
        private final FGPluginsController.Plugin source;

        PythonPluginView(FGPluginsController.Plugin source) {
            this.source = source;
        }

        @Override public String id() { return source.getId(); }
        @Override public String name() { return source.getName(); }
        @Override public String version() { return source.getVersion(); }
        @Override public String author() { return source.getAuthor(); }
        @Override public String description() { return source.getDescription(); }
        @Override public String icon() { return source.getIcon(); }
        @Override public boolean enabled() { return source.getEnabled(); }
        @Override public boolean enable() { return FGPluginsController.INSTANCE.enable(source); }
        @Override public void disable() { FGPluginsController.INSTANCE.disable(source); }
        @Override public void delete() { FGPluginsController.INSTANCE.delete(source); }
        @Override public String lastError() { return FGPluginsController.INSTANCE.lastError(source.getId()); }
        @Override public boolean hasSettings() { return true; }
    }

    private static final class DexPluginView implements PluginView {
        private final FGDexPlugins.Plugin source;

        DexPluginView(FGDexPlugins.Plugin source) {
            this.source = source;
        }

        @Override public String id() { return source.getId(); }
        @Override public String name() { return source.getName(); }
        @Override public String version() { return source.getVersion(); }
        @Override public String author() { return source.getAuthor(); }
        @Override public String description() { return source.getDescription(); }
        @Override public String icon() { return ""; }
        @Override public boolean enabled() { return source.getEnabled(); }
        @Override public boolean enable() { return FGDexPlugins.INSTANCE.enable(source); }
        @Override public void disable() { FGDexPlugins.INSTANCE.disable(source); }
        @Override public void delete() { FGDexPlugins.INSTANCE.delete(source); }
        @Override public String lastError() { return FGDexPlugins.INSTANCE.lastError(source.getId()); }
        @Override public boolean hasSettings() { return false; }
    }

    @Override
    protected CharSequence getTitle() {
        return nestedTitle == null ? plugin.name() : nestedTitle;
    }

    private boolean isNested() {
        return nestedTitle != null;
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        if (!isNested()) {
            String description = plugin.description().isEmpty()
                    ? formatString(R.string.FG_Plugins_Info, plugin.id())
                    : plugin.description();
            if (header == null) {
                header = new FGPluginHeaderCell(getContext(), getResourceProvider());
            }
            final boolean failed = FGPluginsController.INSTANCE.suspectedOfCrash().contains(plugin.id());
            final ArrayList<FGPluginHeaderCell.Action> actions = new ArrayList<>();
            actions.add(new FGPluginHeaderCell.Action(
                    plugin.enabled() ? R.drawable.msg_block : R.drawable.msg_select,
                    getString(plugin.enabled() ? R.string.FG_Plugins_Disable : R.string.FG_Plugins_Enable),
                    !plugin.enabled(), this::togglePlugin));
            final String update = FGPluginUpdates.knownUpdate(plugin.id());
            if (!update.isEmpty()) {
                actions.add(new FGPluginHeaderCell.Action(R.drawable.msg_download,
                        getString(R.string.FG_Plugins_Update), true, this::updatePlugin));
            }
            actions.add(new FGPluginHeaderCell.Action(R.drawable.msg_share,
                    getString(R.string.FG_Plugins_Share), false, () -> FGPluginShare.share(this, plugin.id())));
            actions.add(new FGPluginHeaderCell.Action(R.drawable.msg_log,
                    getString(R.string.FG_Plugins_LogShort), false, this::showLog));
            header.set(plugin.id(), plugin.name(), plugin.icon(), description, subtitle(),
                    getString(failed ? R.string.FG_Plugins_BadgeCrash
                            : plugin.enabled() ? R.string.FG_Plugins_StateOn : R.string.FG_Plugins_StateOff),
                    failed, plugin.enabled(), actions);
            items.add(UItem.asCustom(header));
            items.add(UItem.asShadow(null));

            if (!plugin.enabled() || !plugin.hasSettings()) {
                settings = new JSONArray();
            } else if (!settingsReady) {

                requestSettings();
            }
        }

        for (int i = 0; i < settings.length(); i++) {
            JSONObject item = settings.optJSONObject(i);
            if (item == null) {
                continue;
            }

            if ("slider".equals(optText(item, "kind", "")) ) {
                String title = optText(item, "text", "");
                if (!title.isEmpty()) {
                    items.add(UItem.asHeader(title));
                }
            }
            UItem row = buildRow(item, SETTINGS_ROW_BASE + i);
            if (row != null) {
                items.add(row);
            }
        }

        if (isNested()) {
            return;
        }

        if (plugin.enabled() && plugin.hasSettings() && settings.length() == 0) {
            items.add(UItem.asCenterShadow(getString(R.string.FG_Plugins_NoSettings)));
        }

        items.add(UItem.asShadow(null));
        if (FGBundledPlugins.isBundled(plugin.id())) {

            items.add(UItem.asShadow(getString(R.string.FG_Plugins_Bundled_Hint)));
        } else {
            items.add(UItem.asButton(DELETE_ROW, R.drawable.msg_delete, getString(R.string.Delete)).red());
        }
    }

    private String subtitle() {
        String version = plugin.version();
        String author = plugin.author();
        if (version.isEmpty()) {
            return author;
        }
        return author.isEmpty() ? version : version + " · " + author;
    }

    private UItem rowWithValue(int id, CharSequence text, String value) {
        if (value == null || value.isEmpty()) {
            return UItem.asButton(id, text);
        }
        boolean bothLong = text != null && text.length() > 14 && value.length() > 14;
        if (value.length() > 28 || bothLong) {
            return SettingsHelper.asTextDetail(id, 0, text, value);
        }
        return UItem.asButton(id, text, value);
    }

    private void changeSetting(int index, Object value) {
        final String title = titleOf(index);
        FGPluginsController.INSTANCE.settingsChanged(plugin.id(), index, value, screen, title);
        forgetSettings();

        updateRows(false);
    }

    private void clickSetting(int index, View view) {
        final String title = titleOf(index);
        FGPluginsController.INSTANCE.settingsClick(plugin.id(), index, screen, view, title);
        forgetSettings();

        updateRows(false);
    }

    private String titleOf(int index) {
        JSONObject item = settings.optJSONObject(index);
        return item == null ? null : optText(item, "text", null);
    }

    private boolean settingsReady;
    private boolean settingsRequested;

    private void requestSettings() {
        if (settingsRequested) {
            return;
        }
        settingsRequested = true;
        FGPluginsController.INSTANCE.getQueue().postRunnable(() -> {
            String raw;
            try {
                raw = FGPluginsController.INSTANCE.settingsJson(plugin.id());
            } catch (Exception e) {
                raw = "[]";
            }
            final String answer = raw;
            AndroidUtilities.runOnUIThread(() -> {
                settingsRequested = false;
                settingsReady = true;
                settings = parseScreen(answer);
                updateRows(false);
            });
        });
    }

    private void forgetSettings() {
        settingsReady = false;
    }

    public void reloadPluginSettings() {
        if (nestedTitle != null) {
            reloadNestedScreen();
            return;
        }
        forgetSettings();
        updateRows(true);
    }

    private void reloadNestedScreen() {
        if (parentScreen < 0 || parentIndex < 0) {
            updateRows(true);
            return;
        }
        final int fromScreen = parentScreen;
        final int fromIndex = parentIndex;
        FGPluginsController.INSTANCE.getQueue().postRunnable(() -> {
            String raw;
            try {
                raw = FGPluginsController.INSTANCE.subSettingsJson(plugin.id(), fromScreen, fromIndex);
            } catch (Exception e) {
                raw = "";
            }
            final String answer = raw;
            AndroidUtilities.runOnUIThread(() -> {
                if (isFinished || answer == null || answer.isEmpty()) {
                    return;
                }
                try {
                    JSONObject payload = new JSONObject(answer);
                    JSONArray items = payload.optJSONArray("items");
                    if (items == null || items.length() == 0) {
                        return;
                    }
                    screen = payload.optInt("screen", screen);
                    settings = items;
                    updateRows(true);
                } catch (Exception ignored) {
                }
            });
        });
    }

    private JSONArray parseScreen(String raw) {
        if (raw == null || raw.isEmpty()) {
            return new JSONArray();
        }
        try {
            if (raw.trim().startsWith("[")) {
                return new JSONArray(raw);
            }
            JSONObject payload = new JSONObject(raw);
            screen = payload.optInt("screen", screen);
            JSONArray items = payload.optJSONArray("items");
            return items == null ? new JSONArray() : items;
        } catch (Exception e) {
            return new JSONArray();
        }
    }

    private static String optText(JSONObject item, String key, String fallback) {
        if (item == null || item.isNull(key)) {
            return fallback;
        }
        String value = item.optString(key, fallback == null ? "" : fallback);
        return value == null || value.isEmpty() || "null".equals(value) ? fallback : value;
    }

    private int iconOf(String name) {
        if (name == null || name.isEmpty() || getContext() == null) {
            return 0;
        }
        try {
            return getContext().getResources().getIdentifier(name, "drawable", getContext().getPackageName());
        } catch (Throwable e) {
            return 0;
        }
    }

    private UItem buildRow(JSONObject item, int id) {
        String kind = optText(item, "kind", "text");
        String text = optText(item, "text", "");
        String subtext = optText(item, "subtext", null);
        switch (kind) {
            case "header":
                return UItem.asHeader(text);
            case "divider":
                return UItem.asShadow(text.isEmpty() ? null : text);
            case "switch":

                return SettingsHelper.asSwitchCG(id, text, subtext)
                        .setChecked(item.optBoolean("value", item.optBoolean("default", false)));
            case "selector": {
                JSONArray values = item.optJSONArray("items");
                int selected = item.optInt("value", item.optInt("default", 0));
                String current = values != null && selected >= 0 && selected < values.length()
                        ? values.optString(selected, "")
                        : "";
                return rowWithValue(id, text, current);
            }
            case "slider": {
                int min = item.optInt("minimum", 0);
                int max = Math.max(min + 1, item.optInt("maximum", 100));
                int value = Math.min(max, Math.max(min, item.optInt("value", item.optInt("default", min))));
                final int index = id - SETTINGS_ROW_BASE;
                return UItem.asIntSlideView(0, min, value, max,

                        number -> String.valueOf(number),
                        number -> {
                            changeSetting(index, number);
                            try {
                                item.put("value", number);
                            } catch (Exception ignored) {
                            }
                        });
            }
            case "edittext":
            case "input":
                return rowWithValue(id, text, item.optString("value", item.optString("default", "")));
            case "custom": {

                final int index = id - SETTINGS_ROW_BASE;
                View custom = getContext() == null ? null
                        : FGPluginsController.INSTANCE.settingsView(plugin.id(), screen, index, getContext());
                if (custom == null) {
                    return null;
                }
                return FGPluginCustomCell.Factory.of(id, new FGPluginCustomCell.Factory.Data(
                        custom, item.optBoolean("clickable", false)
                                ? () -> onSettingClick(index, item, null) : null));
            }
            default: {

                int icon = iconOf(optText(item, "icon", null));
                UItem row;
                if (icon != 0) {
                    row = subtext == null
                            ? UItem.asButton(id, icon, text)
                            : UItem.asButton(id, icon, text, subtext);
                } else {
                    row = subtext == null ? UItem.asButton(id, text) : UItem.asButton(id, text, subtext);
                }
                if (item.optBoolean("red", false)) {
                    row.red();
                } else if (item.optBoolean("accent", false)) {
                    row.accent();
                }
                return row;
            }
        }
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == DELETE_ROW) {
            confirmDelete();
            return;
        }

        int index = item.id - SETTINGS_ROW_BASE;
        if (index < 0 || index >= settings.length()) {
            return;
        }
        JSONObject setting = settings.optJSONObject(index);
        if (setting != null) {
            onSettingClick(index, setting, view);
        }
    }

    private void onSettingClick(int index, JSONObject setting, View view) {
        String kind = optText(setting, "kind", "text");
        switch (kind) {
            case "switch": {
                boolean value = !setting.optBoolean("value", setting.optBoolean("default", false));
                SettingsHelper.updateCheckState(view, value);
                changeSetting(index, value);
                try {
                    setting.put("value", value);
                } catch (Exception ignored) {
                }
                break;
            }
            case "selector":
                showSelector(index, setting);
                break;
            case "edittext":
            case "input":
                showInput(index, setting);
                break;
            default:
                if (setting.optBoolean("has_sub_fragment", false)) {
                    openSubScreen(index, setting);
                    return;
                }
                clickSetting(index, view);
                break;
        }
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        int index = item.id - SETTINGS_ROW_BASE;
        if (index < 0 || index >= settings.length()) {
            return false;
        }
        JSONObject setting = settings.optJSONObject(index);
        if (setting == null || !setting.optBoolean("long_clickable", false)) {
            return false;
        }
        FGPluginsController.INSTANCE.settingsLongClick(plugin.id(), index, screen, view);
        return true;
    }

    private void openSubScreen(int index, JSONObject setting) {
        final String title = optText(setting, "text", plugin.name());
        FGPluginsController.INSTANCE.getQueue().postRunnable(() -> {
            String raw;
            try {
                raw = FGPluginsController.INSTANCE.subSettingsJson(plugin.id(), screen, index);
            } catch (Exception e) {
                raw = "";
            }
            final String answer = raw;
            AndroidUtilities.runOnUIThread(() -> showSubScreen(answer, title, index));
        });
    }

    private void showSubScreen(String raw, String title, int index) {
        if (raw == null || raw.isEmpty() || "[]".equals(raw.trim())) {
            BulletinFactory.of(this).createErrorBulletin(getString(R.string.FG_Plugins_NoSettings)).show();
            return;
        }
        try {
            JSONObject payload = new JSONObject(raw);
            JSONArray items = payload.optJSONArray("items");
            if (items == null || items.length() == 0) {
                BulletinFactory.of(this).createErrorBulletin(getString(R.string.FG_Plugins_NoSettings)).show();
                return;
            }
            presentFragment(new PluginDetailsPreferencesEntry(plugin,
                    payload.optInt("screen", 0), title, items, screen, index));
        } catch (Exception e) {
            BulletinFactory.of(this).createErrorBulletin(getString(R.string.FG_Plugins_NoSettings)).show();
        }
    }

    private void updatePlugin() {
        for (FGPluginsController.Plugin candidate : FGPluginsController.INSTANCE.installed()) {
            if (!candidate.getId().equals(plugin.id())) {
                continue;
            }
            FGPluginUpdates.update(candidate, ok -> {
                forgetSettings();
                updateRows(true);
                BulletinFactory.of(this).createSimpleBulletin(ok ? R.raw.contact_check : R.raw.error,
                        getString(ok ? R.string.FG_Plugins_Updated : R.string.FG_Plugins_UpdateFailed)).show();
                return kotlin.Unit.INSTANCE;
            });
            return;
        }
    }

    private void togglePlugin() {
        if (plugin.enabled()) {
            forgetSettings();
            FGPluginsController.INSTANCE.getQueue().postRunnable(() -> {
                plugin.disable();
                AndroidUtilities.runOnUIThread(() -> updateRows(false));
            });
            return;
        }

        forgetSettings();
        FGPluginsController.INSTANCE.getQueue().postRunnable(() -> {
            boolean started = plugin.enable();
            AndroidUtilities.runOnUIThread(() -> {
                if (!started) {
                    showFailure();
                }
                updateRows(true);
            });
        });
    }

    private void showFailure() {
        if (getParentActivity() == null) {
            return;
        }
        String reason = plugin.lastError();
        if (reason.length() > 1200) {
            reason = reason.substring(reason.length() - 1200);
        }
        final String report = buildReport(reason);

        AlertDialog.Builder builder = new AlertDialog.Builder(getParentActivity(), getResourceProvider());
        builder.setTitle(formatString(R.string.FG_Plugins_Failed, plugin.name()));
        builder.setMessage(reason.isEmpty() ? getString(R.string.FG_Plugins_NoReason) : reason);
        builder.setPositiveButton(getString(R.string.FG_Plugins_AskAuthor), (dialog, which) -> {
            AndroidUtilities.addToClipboard(report);
            BulletinFactory.of(this).createSimpleBulletin(R.raw.copy, getString(R.string.FG_Plugins_ReportCopied)).show();
            String author = plugin.author().trim();
            if (author.startsWith("@") && author.length() > 1 && getParentActivity() != null) {
                Browser.openUrl(getParentActivity(), "https://t.me/" + author.substring(1));
            }
        });
        builder.setNeutralButton(getString(R.string.Copy), (dialog, which) -> {
            AndroidUtilities.addToClipboard(report);
            BulletinFactory.of(this).createSimpleBulletin(R.raw.copy, getString(R.string.FG_Plugins_ReportCopied)).show();
        });
        builder.setNegativeButton(getString(R.string.Close), null);
        showDialog(builder.create());
    }

    private void showLog() {
        if (getParentActivity() == null) {
            return;
        }
        String text = FGPluginsController.recentLog();
        if (text.length() > 4000) {
            text = text.substring(text.length() - 4000);
        }
        final String shown = text.trim().isEmpty() ? getString(R.string.FG_Plugins_LogEmpty) : text;
        final String copied = shown;

        AlertDialog.Builder builder = new AlertDialog.Builder(getParentActivity(), getResourceProvider());
        builder.setTitle(getString(R.string.FG_Plugins_Log));
        builder.setMessage(shown);
        builder.setPositiveButton(getString(R.string.Copy), (dialog, which) -> {
            AndroidUtilities.addToClipboard(copied);
            BulletinFactory.of(this).createSimpleBulletin(R.raw.copy,
                    getString(R.string.FG_Plugins_ReportCopied)).show();
        });
        builder.setNegativeButton(getString(R.string.Close), null);
        showDialog(builder.create());
    }

    private String buildReport(String reason) {
        String header = formatString(R.string.FG_Plugins_ReportHeader, plugin.name(),
                plugin.version().isEmpty() ? "?" : plugin.version());
        return header + System.lineSeparator() + System.lineSeparator() + reason;
    }

    private void confirmDelete() {
        if (getParentActivity() == null) {
            return;
        }
        AlertDialog.Builder builder = new AlertDialog.Builder(getParentActivity(), getResourceProvider());
        builder.setTitle(plugin.name());
        builder.setMessage(getString(R.string.FG_Plugins_DeleteConfirm));
        builder.setPositiveButton(getString(R.string.Delete), (dialog, which) -> {
            plugin.delete();
            finishFragment();
        });
        builder.setNegativeButton(getString(R.string.Cancel), null);
        showDialog(builder.create());
    }

    private void showSelector(int index, JSONObject setting) {
        if (getParentActivity() == null) {
            return;
        }
        JSONArray values = setting.optJSONArray("items");
        if (values == null || values.length() == 0) {
            return;
        }
        CharSequence[] titles = new CharSequence[values.length()];
        for (int i = 0; i < values.length(); i++) {
            titles[i] = values.optString(i, "");
        }
        AlertDialog.Builder builder = new AlertDialog.Builder(getParentActivity(), getResourceProvider());
        builder.setTitle(optText(setting, "text", ""));
        builder.setItems(titles, (dialog, which) -> {
            changeSetting(index, which);
        });
        builder.setNegativeButton(getString(R.string.Cancel), null);
        showDialog(builder.create());
    }

    private void showInput(int index, JSONObject setting) {
        if (getParentActivity() == null) {
            return;
        }
        AlertDialog.Builder builder = new AlertDialog.Builder(getParentActivity(), getResourceProvider());
        builder.setTitle(optText(setting, "text", ""));

        EditTextBoldCursor editText = new EditTextBoldCursor(getParentActivity());
        editText.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 16);
        editText.setTextColor(getThemedColor(Theme.key_dialogTextBlack));
        editText.setHintText(setting.optString("hint", ""));
        boolean multiline = setting.optBoolean("multiline", false);
        editText.setSingleLine(!multiline);
        editText.setInputType(multiline
                ? InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE
                : InputType.TYPE_CLASS_TEXT);

        editText.setPadding(0, 0, 0, 0);
        editText.setHintColor(getThemedColor(Theme.key_dialogTextHint));
        editText.setCursorColor(getThemedColor(Theme.key_chats_actionBackground));
        editText.setCursorSize(dp(20));
        editText.setCursorWidth(1.5f);
        editText.setBackground(null);
        editText.setText(setting.optString("value", setting.optString("default", "")));
        editText.setSelection(editText.getText().length());

        FrameLayout field = new FrameLayout(getParentActivity());
        field.setBackground(Theme.createRoundRectDrawable(dp(10),
                getThemedColor(Theme.key_dialogSearchBackground)));
        field.addView(editText, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                Gravity.LEFT | Gravity.TOP, 16, 12, 16, 12));

        FrameLayout wrapper = new FrameLayout(getParentActivity());
        wrapper.addView(field, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                Gravity.LEFT | Gravity.TOP, 22, 4, 22, 4));
        builder.setView(wrapper);

        builder.setPositiveButton(getString(R.string.Save), (dialog, which) -> {
            changeSetting(index, editText.getText().toString());
        });
        builder.setNegativeButton(getString(R.string.Cancel), null);
        showDialog(builder.create());
    }
}
