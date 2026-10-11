/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.preferences;

import static org.telegram.messenger.LocaleController.formatString;
import static org.telegram.messenger.LocaleController.getString;

import android.app.Activity;
import android.content.Intent;
import android.view.View;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.FileLog;
import org.telegram.messenger.R;
import org.telegram.messenger.LocaleController;
import com.th3nekit.finegram.plugins.FGPluginUpdates;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.ui.Components.ItemOptions;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;
import java.util.List;

import com.th3nekit.finegram.plugins.FGBundledPlugins;
import com.th3nekit.finegram.plugins.FGDexPlugins;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.ActionBar.BaseFragment;

import com.exteragram.messenger.plugins.Plugin;
import com.exteragram.messenger.plugins.PluginsController;
import com.exteragram.messenger.plugins.PythonPluginsEngine;
import com.exteragram.messenger.plugins.ui.components.InstallPluginBottomSheet;
import com.th3nekit.finegram.plugins.FGPluginShare;
import com.th3nekit.finegram.plugins.FGPluginsController;
import com.th3nekit.finegram.preferences.cells.FGPluginCell;
import com.th3nekit.finegram.preferences.cells.FGPluginsHeroCell;

public class PluginsPreferencesEntry extends BaseCGPreferencesEntry {

    private static final int REQUEST_PLUGIN_FILE = 5391;
    private static final int PLUGIN_ROW_BASE = 100;
    private static final int SAFE_MODE_ROW = 90;
    private static final int DEX_ROW_BASE = 500;

    private List<FGPluginsController.Plugin> plugins = new ArrayList<>();
    private List<FGDexPlugins.Plugin> dexPlugins = new ArrayList<>();

    private final List<String> order = new ArrayList<>();

    private FGPluginsHeroCell hero;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.FG_Plugins);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        plugins = FGPluginsController.INSTANCE.installed();
        dexPlugins = FGDexPlugins.INSTANCE.installed();
        rememberOrder();

        final int total = plugins.size() + dexPlugins.size();
        int enabled = 0;
        final ArrayList<FGPluginsHeroCell.AvatarSpec> avatars = new ArrayList<>();
        for (FGPluginsController.Plugin plugin : plugins) {
            if (plugin.getEnabled()) {
                enabled++;
                if (avatars.size() < 4) avatars.add(new FGPluginsHeroCell.AvatarSpec(plugin.getId(), plugin.getIcon()));
            }
        }
        for (FGDexPlugins.Plugin plugin : dexPlugins) {
            if (plugin.getEnabled()) {
                enabled++;
                if (avatars.size() < 4) avatars.add(new FGPluginsHeroCell.AvatarSpec(plugin.getId(), null));
            }
        }
        int updates = 0;
        for (FGPluginsController.Plugin plugin : plugins) {
            if (!FGPluginUpdates.knownUpdate(plugin.getId()).isEmpty()) updates++;
        }

        if (hero == null) {
            hero = new FGPluginsHeroCell(getContext(), getResourceProvider());
            hero.setDelegate(new FGPluginsHeroCell.Delegate() {
                @Override
                public void onStore() {
                    presentFragment(new com.th3nekit.finegram.store.FGStoreActivity());
                }

                @Override
                public void onInstall() {
                    pickPluginFile();
                }

                @Override
                public void onUpdates() {
                    checkUpdates();
                }
            });
        }
        final String stats = total == 0
                ? getString(R.string.FG_Plugins_HeroEmpty)
                : updates > 0
                    ? formatString(R.string.FG_Plugins_HeroStatsUpdates, enabled, updates)
                    : formatString(R.string.FG_Plugins_HeroStats, enabled);
        final CharSequence updatesText = checkingUpdates
                ? getString(R.string.FG_Plugins_Checking)
                : updates > 0 ? formatString(R.string.FG_Plugins_HeroUpdateCount, updates)
                : getString(R.string.FG_Plugins_HeroUpdate);
        hero.set(total, LocaleController.formatPluralString("FG_Plugins_Word", total).replace(String.valueOf(total), "").trim(),
                stats,
                getString(R.string.FG_Store), R.drawable.msg_stories_saved,
                getString(R.string.FG_Plugins_HeroFile), R.drawable.msg_add,
                updatesText, R.drawable.msg_download, updates > 0,
                avatars);
        items.add(UItem.asCustom(hero));
        items.add(UItem.asShadow(null));

        if (com.th3nekit.finegram.plugins.FGSafeMode.isActive()) {
            items.add(UItem.asHeader(getString(R.string.FG_SafeMode_Title)));
            items.add(UItem.asButton(SAFE_MODE_ROW, R.drawable.msg_retry, getString(R.string.FG_SafeMode_TurnOff)));
            items.add(UItem.asShadow(getString(R.string.FG_SafeMode_Desc)));
        }

        if (total == 0) {
            items.add(UItem.asCenterShadow(getString(R.string.FG_Plugins_Empty)));
            return;
        }

        items.add(UItem.asHeader(formatString(R.string.FG_Plugins_InstalledCount, total)));

        final java.util.Map<String, Integer> slowPlugins =
                FGPluginsController.INSTANCE.slowPlugins();

        final ArrayList<UItem> rows = new ArrayList<>();
        for (String id : order) {
            for (int i = 0; i < plugins.size(); i++) {
                final FGPluginsController.Plugin plugin = plugins.get(i);
                if (!plugin.getId().equals(id)) continue;
                final String update = FGPluginUpdates.knownUpdate(plugin.getId());
                final boolean crashed = FGPluginsController.INSTANCE.suspectedOfCrash().contains(plugin.getId());
                final int slow = slowPlugins.getOrDefault(plugin.getId(), 0);
                final boolean clash = FGPluginsController.blockedByBypass(plugin.getId());
                final FGPluginCell.Factory.Data data = new FGPluginCell.Factory.Data(
                        plugin.getId(), plugin.getName(),
                        subtitle(plugin.getDescription(), plugin.getVersion(), plugin.getAuthor(), false,
                                update, crashed, slow, clash, FGBundledPlugins.isBundled(plugin.getId())),
                        plugin.getIcon(), plugin.getEnabled(), () -> togglePython(plugin));
                if (clash || crashed) {
                    data.badge = getString(clash ? R.string.FG_Plugins_BadgeConflict : R.string.FG_Plugins_BadgeCrash);
                    data.badgeAlert = true;
                } else if (!update.isEmpty()) {
                    data.badge = getString(R.string.FG_Plugins_BadgeUpdate);
                } else if (FGBundledPlugins.isBundled(plugin.getId())) {
                    data.badge = getString(R.string.FG_Plugins_Bundled);
                }
                rows.add(FGPluginCell.Factory.of(PLUGIN_ROW_BASE + i, data));
            }
            for (int i = 0; i < dexPlugins.size(); i++) {
                final FGDexPlugins.Plugin plugin = dexPlugins.get(i);
                if (!plugin.getId().equals(id)) continue;
                final FGPluginCell.Factory.Data data = new FGPluginCell.Factory.Data(
                        plugin.getId(), plugin.getName(),
                        subtitle(plugin.getDescription(), plugin.getVersion(), plugin.getAuthor()),
                        null, plugin.getEnabled(), () -> toggleDex(plugin));
                data.badge = "dex";
                rows.add(FGPluginCell.Factory.of(DEX_ROW_BASE + i, data));
            }
        }
        for (int i = 0; i < rows.size(); i++) {
            final FGPluginCell.Factory.Data data = (FGPluginCell.Factory.Data) rows.get(i).object;
            data.first = false;
            data.last = i == rows.size() - 1;
        }
        items.addAll(rows);
        items.add(UItem.asShadow(getString(R.string.FG_Plugins_Hint)));
    }

    private void rememberOrder() {
        final ArrayList<String> present = new ArrayList<>();
        final ArrayList<String> enabledIds = new ArrayList<>();
        final ArrayList<String> disabledIds = new ArrayList<>();
        final java.util.Comparator<String> byName = (a, b) -> nameOf(a).compareToIgnoreCase(nameOf(b));
        for (FGPluginsController.Plugin plugin : plugins) {
            present.add(plugin.getId());
            (plugin.getEnabled() ? enabledIds : disabledIds).add(plugin.getId());
        }
        for (FGDexPlugins.Plugin plugin : dexPlugins) {
            present.add(plugin.getId());
            (plugin.getEnabled() ? enabledIds : disabledIds).add(plugin.getId());
        }
        order.retainAll(present);
        if (order.isEmpty()) {
            enabledIds.sort(byName);
            disabledIds.sort(byName);
            order.addAll(enabledIds);
            order.addAll(disabledIds);
            return;
        }
        for (String id : present) {
            if (!order.contains(id)) order.add(id);
        }
    }

    private String nameOf(String id) {
        for (FGPluginsController.Plugin plugin : plugins) {
            if (plugin.getId().equals(id)) return plugin.getName();
        }
        for (FGDexPlugins.Plugin plugin : dexPlugins) {
            if (plugin.getId().equals(id)) return plugin.getName();
        }
        return id;
    }

    private void openSettings(String pluginId) {
        final Plugin plugin = Plugin.of(pluginId);

        try {
            PythonPluginsEngine.class
                    .getDeclaredMethod("openPluginSettings", Plugin.class, BaseFragment.class)
                    .invoke(null, plugin, this);
        } catch (Throwable e) {
            PythonPluginsEngine.getInstance().openPluginSettings(plugin, this);
        }
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

        AlertDialog.Builder builder = new AlertDialog.Builder(getParentActivity(), getResourceProvider());
        builder.setTitle(getString(R.string.FG_Plugins_Log));
        builder.setMessage(shown);
        builder.setPositiveButton(getString(R.string.Copy), (dialog, which) -> {
            AndroidUtilities.addToClipboard(shown);
            BulletinFactory.of(this).createSimpleBulletin(R.raw.copy,
                    getString(R.string.FG_Plugins_ReportCopied)).show();
        });
        builder.setNegativeButton(getString(R.string.Close), null);
        showDialog(builder.create());
    }

    private void showPythonMenu(FGPluginsController.Plugin plugin, View anchor) {
        ItemOptions options = ItemOptions.makeOptions(this, anchor);
        options.add(R.drawable.msg_settings, getString(R.string.FG_Plugins_Open),
                () -> openSettings(plugin.getId()));
        options.add(plugin.getEnabled() ? R.drawable.msg_block : R.drawable.msg_select,
                getString(plugin.getEnabled() ? R.string.FG_Plugins_Disable : R.string.FG_Plugins_Enable),
                () -> togglePython(plugin));
        if (!FGPluginUpdates.knownUpdate(plugin.getId()).isEmpty()) {
            options.add(R.drawable.msg_download, getString(R.string.FG_Plugins_Update),
                    () -> updateOne(plugin));
        }
        options.add(R.drawable.msg_share, getString(R.string.FG_Plugins_Share),
                () -> FGPluginShare.share(this, plugin.getId()));
        options.add(R.drawable.msg_log, getString(R.string.FG_Plugins_Log), this::showLog);
        if (!FGBundledPlugins.isBundled(plugin.getId())) {
            options.add(R.drawable.msg_delete, getString(R.string.Delete), true, () -> {
                FGPluginsController.INSTANCE.delete(plugin);
                FGPluginUpdates.forget(plugin.getId());
                updateRows(true);
            });
        }
        options.setScrimViewBackground(listView.getClipBackground(anchor)).show();
    }

    private void showDexMenu(FGDexPlugins.Plugin plugin, View anchor) {
        ItemOptions options = ItemOptions.makeOptions(this, anchor);
        options.add(R.drawable.msg_settings, getString(R.string.FG_Plugins_Open),
                () -> openSettings(plugin.getId()));
        options.add(plugin.getEnabled() ? R.drawable.msg_block : R.drawable.msg_select,
                getString(plugin.getEnabled() ? R.string.FG_Plugins_Disable : R.string.FG_Plugins_Enable),
                () -> toggleDex(plugin));
        options.add(R.drawable.msg_share, getString(R.string.FG_Plugins_Share),
                () -> FGPluginShare.share(this, plugin.getId()));
        options.add(R.drawable.msg_delete, getString(R.string.Delete), true, () -> {
            FGDexPlugins.INSTANCE.delete(plugin);
            updateRows(true);
        });
        options.setScrimViewBackground(listView.getClipBackground(anchor)).show();
    }

    private CharSequence subtitle(String description, String version, String author) {
        return subtitle(description, version, author, true, "", false, 0, false, false);
    }

    private CharSequence subtitle(String description, String version, String author, boolean dex,
                                  String update, boolean crashed, int slowestMs, boolean bypassClash,
                                  boolean bundled) {
        if (bypassClash) {
            return getString(R.string.FG_Plugins_BypassClash_Short);
        }
        if (crashed) {
            return getString(R.string.FG_Plugins_Crashed);
        }
        if (slowestMs > 0) {
            return LocaleController.formatString(R.string.FG_Plugins_Slow, slowestMs);
        }
        if (update != null && !update.isEmpty()) {
            return LocaleController.formatString(R.string.FG_Plugins_UpdateAvailable, update);
        }
        if (description != null && !description.trim().isEmpty()) {
            return description.trim();
        }

        StringBuilder text = new StringBuilder();
        if (!version.isEmpty()) {
            text.append(version);
        }
        if (!author.isEmpty()) {
            if (text.length() > 0) {
                text.append(" · ");
            }
            text.append(author);
        }
        if (dex) {
            if (text.length() > 0) {
                text.append(" · ");
            }
            text.append("dex");
        }
        if (bundled) {
            if (text.length() > 0) {
                text.append(" · ");
            }
            text.append(getString(R.string.FG_Plugins_Bundled));
        }
        return text.toString();
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        if (item.id >= DEX_ROW_BASE) {
            final int index = item.id - DEX_ROW_BASE;
            if (index >= 0 && index < dexPlugins.size()) {
                showDexMenu(dexPlugins.get(index), view);
                return true;
            }
            return false;
        }
        final int index = item.id - PLUGIN_ROW_BASE;
        if (index >= 0 && index < plugins.size()) {
            showPythonMenu(plugins.get(index), view);
            return true;
        }
        return false;
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == SAFE_MODE_ROW) {
            com.th3nekit.finegram.plugins.FGSafeMode.turnOff();
            BulletinFactory.of(this).createSimpleBulletin(R.raw.contact_check,
                    getString(R.string.FG_SafeMode_Started)).show();
            updateRows(true);
            return;
        }

        if (view instanceof FGPluginCell && ((FGPluginCell) view).isOnSwitch(x)) {
            FGPluginCell cell = (FGPluginCell) view;
            cell.setChecked(!cell.isChecked());
            cell.toggle();
            return;
        }

        if (item.id >= DEX_ROW_BASE) {
            int index = item.id - DEX_ROW_BASE;
            if (index >= 0 && index < dexPlugins.size()) {
                openSettings(dexPlugins.get(index).getId());
            }
            return;
        }

        int index = item.id - PLUGIN_ROW_BASE;
        if (index >= 0 && index < plugins.size()) {
            openSettings(plugins.get(index).getId());
        }
    }

    private void togglePython(FGPluginsController.Plugin plugin) {
        boolean turningOn = !plugin.getEnabled();
        if (turningOn) {

            FGPluginsController.INSTANCE.forgetCrashSuspicion(plugin.getId());
        }
        FGPluginsController.INSTANCE.getQueue().postRunnable(() -> {
            boolean enabled;
            if (turningOn) {
                enabled = FGPluginsController.INSTANCE.enable(plugin);
            } else {
                FGPluginsController.INSTANCE.disable(plugin);
                enabled = false;
            }
            AndroidUtilities.runOnUIThread(() -> {
                if (turningOn && !enabled) {
                    BulletinFactory.of(this).createErrorBulletin(
                            FGPluginsController.blockedByBypass(plugin.getId())
                                    ? getString(R.string.FG_Plugins_BypassClash)
                                    : formatString(R.string.FG_Plugins_Failed, plugin.getName())).show();
                }

                updateRows(false);
            });
        });
    }

    private void toggleDex(FGDexPlugins.Plugin plugin) {
        boolean turningOn = !plugin.getEnabled();
        FGPluginsController.INSTANCE.getQueue().postRunnable(() -> {
            boolean enabled;
            if (turningOn) {
                enabled = FGDexPlugins.INSTANCE.enable(plugin);
            } else {
                FGDexPlugins.INSTANCE.disable(plugin);
                enabled = false;
            }
            AndroidUtilities.runOnUIThread(() -> {
                if (turningOn && !enabled) {
                    BulletinFactory.of(this).createErrorBulletin(
                            formatString(R.string.FG_Plugins_Failed, plugin.getName())).show();
                }

                updateRows(false);
            });
        });
    }

    private void pickPluginFile() {
        try {
            Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
            intent.setType("*/*");
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            startActivityForResult(Intent.createChooser(intent, getString(R.string.FG_Plugins_Install)), REQUEST_PLUGIN_FILE);
        } catch (Throwable e) {
            FileLog.e(e);
        }
    }

    @Override
    public void onActivityResultFragment(int requestCode, int resultCode, Intent data) {
        if (requestCode != REQUEST_PLUGIN_FILE || resultCode != Activity.RESULT_OK || data == null || data.getData() == null) {
            return;
        }
        FGPluginsController.Plugin preview = FGPluginsController.INSTANCE.readPreview(data.getData());
        askAndInstall(data.getData(), preview);
    }

    private void askAndInstall(android.net.Uri uri, FGPluginsController.Plugin preview) {
        if (getParentActivity() == null) {
            install(uri);
            return;
        }
        InstallPluginBottomSheet.InstallParams params =
                new InstallPluginBottomSheet.InstallParams(uri.toString());
        if (preview != null) {
            params.id = preview.getId();
            params.name = preview.getName();
            params.version = preview.getVersion();
            params.author = preview.getAuthor();
            params.description = preview.getDescription();
            params.icon = preview.getIcon();
        }
        InstallPluginBottomSheet sheet = new InstallPluginBottomSheet(
                getParentActivity(), getResourceProvider(), params, accepted -> install(uri));
        showDialog(sheet);
    }

    private void install(android.net.Uri uri) {

        final org.telegram.ui.ActionBar.AlertDialog progress =
                new org.telegram.ui.ActionBar.AlertDialog(getContext(), org.telegram.ui.ActionBar.AlertDialog.ALERT_TYPE_SPINNER);
        final boolean[] shown = new boolean[1];
        com.th3nekit.finegram.plugins.FGLibraryProgress.INSTANCE.listen((name, done, total) -> {
            if (!shown[0]) {
                shown[0] = true;
                progress.show();
            }
            progress.setMessage(total > 0
                    ? formatString(R.string.FG_Plugins_Library_Progress, name,
                            Math.min(100, (int) (done * 100 / total)))
                    : formatString(R.string.FG_Plugins_Library, name));
        });

        final Runnable finish = () -> {
            com.th3nekit.finegram.plugins.FGLibraryProgress.INSTANCE.listen(null);
            if (shown[0]) {
                try { progress.dismiss(); } catch (Throwable ignored) {}
            }
        };

        FGPluginsController.INSTANCE.getQueue().postRunnable(() -> {
            final FGPluginsController.Plugin installed = FGPluginsController.INSTANCE.install(uri, null);
            if (installed != null) {
                FGPluginsController.INSTANCE.enable(installed);
                AndroidUtilities.runOnUIThread(() -> {
                    finish.run();
                    reportInstalled(installed.getName());
                });
                return;
            }
            final FGDexPlugins.Plugin dex = FGDexPlugins.INSTANCE.install(uri, null);
            if (dex != null) {
                FGDexPlugins.INSTANCE.enable(dex);
                AndroidUtilities.runOnUIThread(() -> {
                    finish.run();
                    reportInstalled(dex.getName());
                });
                return;
            }
            AndroidUtilities.runOnUIThread(() -> {
                finish.run();
                BulletinFactory.of(this)
                        .createErrorBulletin(getString(R.string.FG_Plugins_BadFile)).show();
            });
        });
    }

    private void reportInstalled(String name) {
        updateRows(true);
        BulletinFactory.of(this).createSimpleBulletin(
                R.raw.done, formatString(R.string.FG_Plugins_Installed_Toast, name)).show();
    }

    private boolean checkingUpdates;

    private void checkUpdates() {
        if (checkingUpdates) return;
        checkingUpdates = true;
        updateRows(false);
        FGPluginUpdates.checkAll(true, found -> {
            checkingUpdates = false;
            updateRows(true);
            if (getParentActivity() != null) {
                BulletinFactory.of(this)
                        .createSimpleBulletin(R.raw.info,
                                found == 0
                                        ? getString(R.string.FG_Plugins_NoUpdates)
                                        : LocaleController.formatPluralString("FG_Plugins_UpdatesFound", found))
                        .show();
            }
            return kotlin.Unit.INSTANCE;
        });
    }

    private void updateOne(FGPluginsController.Plugin plugin) {
        FGPluginUpdates.update(plugin, ok -> {
            updateRows(true);
            BulletinFactory.of(this)
                    .createSimpleBulletin(ok ? R.raw.contact_check : R.raw.error,
                            getString(ok ? R.string.FG_Plugins_Updated : R.string.FG_Plugins_UpdateFailed))
                    .show();
            return kotlin.Unit.INSTANCE;
        });
    }

}
