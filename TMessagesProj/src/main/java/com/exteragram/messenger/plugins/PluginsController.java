/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.LaunchActivity;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;

import com.th3nekit.finegram.plugins.FGDexPlugins;
import com.th3nekit.finegram.plugins.FGPluginsController;
import com.th3nekit.finegram.preferences.PluginDetailsPreferencesEntry;

public final class PluginsController {

    private static final PluginsController INSTANCE = new PluginsController();

    public static PluginsController getInstance() {
        return INSTANCE;
    }

    public void invalidatePluginSettings(String pluginId) {
        com.th3nekit.finegram.plugins.FGPluginsController.INSTANCE.forgetSettings(pluginId);
    }

    public void loadPluginSettings(String pluginId) {
        com.th3nekit.finegram.plugins.FGHookBridge.INSTANCE.reloadPluginSettings(pluginId);
    }

    public Object showInstallDialog(Object fragment, String path, boolean replaceExisting) {
        if (!(fragment instanceof BaseFragment) || path == null) {
            return null;
        }
        boolean shown = com.th3nekit.finegram.plugins.FGPluginsHook.promptInstall(
                (BaseFragment) fragment, new java.io.File(path));
        return shown ? Boolean.TRUE : null;
    }

    public static Object registerFileIcon(String extension, Object drawable) {
        return com.th3nekit.finegram.plugins.FGFileIcons.register(extension, drawable);
    }

    public static void unregisterFileIcon(String extension) {
        com.th3nekit.finegram.plugins.FGFileIcons.unregister(extension);
    }

    public static void openPluginSettings(String pluginId) {
        if (pluginId == null || pluginId.isEmpty()) {
            return;
        }
        AndroidUtilities.runOnUIThread(() -> {
            BaseFragment host = LaunchActivity.getLastFragment();
            if (host == null) {
                return;
            }
            BaseFragment screen = settingsFragment(pluginId);
            if (screen != null) {
                host.presentFragment(screen);
            }
        });
    }

    public static void openPluginSettings(Plugin plugin, BaseFragment fragment) {
        if (plugin == null) {
            return;
        }
        final BaseFragment host = fragment != null ? fragment : LaunchActivity.getLastFragment();
        if (host == null) {
            return;
        }
        final BaseFragment screen = settingsFragment(plugin.getId());
        if (screen != null) {
            host.presentFragment(screen);
        }
    }

    public static BaseFragment settingsFragment(String pluginId) {
        for (FGPluginsController.Plugin plugin : FGPluginsController.INSTANCE.installed()) {
            if (plugin.getId().equals(pluginId)) {
                return new PluginDetailsPreferencesEntry(plugin);
            }
        }
        for (FGDexPlugins.Plugin plugin : FGDexPlugins.INSTANCE.installed()) {
            if (plugin.getId().equals(pluginId)) {
                return new PluginDetailsPreferencesEntry(plugin);
            }
        }
        return null;
    }

    public static void runOnPluginsQueue(Runnable action) {
        if (action == null) {
            return;
        }
        FGPluginsController.INSTANCE.getQueue().postRunnable(action);
    }

    public void init() {
        init(false, null);
    }

    public void init(boolean force) {
        init(force, null);
    }

    public void init(Runnable onReady) {
        init(false, onReady);
    }

    public void init(boolean force, Runnable onReady) {
        runOnPluginsQueue(() -> {
            FGPluginsController.INSTANCE.bootRuntime();
            if (onReady != null) {
                AndroidUtilities.runOnUIThread(onReady);
            }
        });
    }

    public boolean isInitialized() {
        return FGPluginsController.INSTANCE.runtimeOrNull() != null;
    }

    public static boolean isPluginEngineAvailable() {
        return true;
    }

    public static boolean isPluginEngineSupported() {
        return true;
    }

    public static PluginsEngine getPluginEngine(java.io.File file) {
        if (file == null) {
            return null;
        }
        final String name = file.getName().toLowerCase();
        final boolean known = name.endsWith(".plugin") || name.endsWith(".eaf")
                || name.endsWith(".dex") || name.endsWith(".jar") || name.endsWith(".py");
        return known ? PythonPluginsEngine.getInstance() : null;
    }

    public PluginsEngine getPluginEngine(String pluginId) {
        return pluginId == null ? null : PythonPluginsEngine.getInstance();
    }

    public String getPluginPath(String pluginId) {
        for (FGPluginsController.Plugin plugin : FGPluginsController.INSTANCE.installed()) {
            if (plugin.getId().equals(pluginId)) {
                return plugin.getFile().getAbsolutePath();
            }
        }
        for (FGDexPlugins.Plugin plugin : FGDexPlugins.INSTANCE.installed()) {
            if (plugin.getId().equals(pluginId)) {
                return plugin.getFile().getAbsolutePath();
            }
        }
        return null;
    }

    public java.io.File getPluginsDir() {
        final String path = getPluginPath(firstInstalledId());
        return path != null ? new java.io.File(path).getParentFile()
                : new java.io.File(org.telegram.messenger.ApplicationLoader.getFilesDirFixed(), "plugins");
    }

    private static String firstInstalledId() {
        for (FGPluginsController.Plugin plugin : FGPluginsController.INSTANCE.installed()) {
            return plugin.getId();
        }
        return null;
    }

    public static boolean isPlugin(org.telegram.messenger.MessageObject message) {
        if (message == null) {
            return false;
        }
        final String name = message.getDocumentName();
        if (name == null) {
            return false;
        }
        final String lower = name.toLowerCase();
        return lower.endsWith(".plugin") || lower.endsWith(".eaf") || lower.endsWith(".dex");
    }

    public static boolean isPlugin(java.io.File file, org.telegram.messenger.MessageObject message) {
        if (file != null && getPluginEngine(file) != null) {
            return true;
        }
        return isPlugin(message);
    }

    public static boolean isPluginPinned(String pluginId) {
        return false;
    }

    public static void setPluginPinned(String pluginId, boolean pinned) {
    }

    public static void openPluginSettings(String pluginId, String key) {
        openPluginSettings(pluginId);
    }

    private static Object readSetting(String pluginId, String key, Object fallback) {
        final com.chaquo.python.PyObject module = FGPluginsController.INSTANCE.runtimeOrNull();
        if (module == null || pluginId == null || key == null) {
            return fallback;
        }
        try {
            final com.chaquo.python.PyObject value = module.callAttr("get_setting", pluginId, key, fallback);
            return value == null ? fallback : value.toJava(Object.class);
        } catch (Throwable e) {
            return fallback;
        }
    }

    public boolean getPluginSettingBoolean(String pluginId, String key, boolean fallback) {
        final Object value = readSetting(pluginId, key, fallback);
        return value instanceof Boolean ? (Boolean) value : fallback;
    }

    public int getPluginSettingInt(String pluginId, String key, int fallback) {
        final Object value = readSetting(pluginId, key, fallback);
        return value instanceof Number ? ((Number) value).intValue() : fallback;
    }

    public String getPluginSettingString(String pluginId, String key, String fallback) {
        final Object value = readSetting(pluginId, key, fallback);
        return value == null ? fallback : String.valueOf(value);
    }

    public boolean hasPluginSettings(String pluginId) {
        try {
            final String json = FGPluginsController.INSTANCE.settingsJson(pluginId);
            return json != null && json.length() > 2;
        } catch (Throwable e) {
            return false;
        }
    }

    public boolean hasPluginSettingsPreferences(String pluginId) {
        return !getPluginSettingsPreferences(pluginId).isEmpty();
    }

    public Map<String, Object> getPluginSettingsPreferences(String pluginId) {
        final Map<String, Object> out = new LinkedHashMap<>();
        final com.chaquo.python.PyObject module = FGPluginsController.INSTANCE.runtimeOrNull();
        if (module == null || pluginId == null) {
            return out;
        }
        try {
            for (Map.Entry<com.chaquo.python.PyObject, com.chaquo.python.PyObject> entry :
                    module.callAttr("all_settings", pluginId).asMap().entrySet()) {
                out.put(entry.getKey().toString(),
                        entry.getValue() == null ? null : entry.getValue().toJava(Object.class));
            }
        } catch (Throwable ignored) {
        }
        return out;
    }

    public java.util.concurrent.ConcurrentHashMap<String, Map<String, Object>> getSettings() {
        final java.util.concurrent.ConcurrentHashMap<String, Map<String, Object>> out =
                new java.util.concurrent.ConcurrentHashMap<>();
        for (FGPluginsController.Plugin plugin : FGPluginsController.INSTANCE.installed()) {
            out.put(plugin.getId(), getPluginSettingsPreferences(plugin.getId()));
        }
        return out;
    }

    public void clearPluginSettingsPreferences(String pluginId) {
        clearPluginSettingsPreferences(pluginId, true);
    }

    public void clearPluginSettingsPreferences(String pluginId, boolean notify) {
        final com.chaquo.python.PyObject module = FGPluginsController.INSTANCE.runtimeOrNull();
        if (module == null || pluginId == null) {
            return;
        }
        try {
            module.callAttr("clear_settings", pluginId);
        } catch (Throwable ignored) {
        }
        if (notify) {
            invalidatePluginSettings(pluginId);
        }
    }

    public void setPluginEnabled(String pluginId, boolean enabled,
                                 org.telegram.messenger.Utilities.Callback<Object> callback) {
        runOnPluginsQueue(() -> {
            boolean ok = false;
            try {
                for (FGPluginsController.Plugin plugin : FGPluginsController.INSTANCE.installed()) {
                    if (plugin.getId().equals(pluginId)) {
                        if (enabled) {
                            ok = FGPluginsController.INSTANCE.enable(plugin);
                        } else {
                            FGPluginsController.INSTANCE.disable(plugin);
                            ok = true;
                        }
                    }
                }
                for (FGDexPlugins.Plugin plugin : FGDexPlugins.INSTANCE.installed()) {
                    if (plugin.getId().equals(pluginId)) {
                        if (enabled) {
                            ok = FGDexPlugins.INSTANCE.enable(plugin);
                        } else {
                            FGDexPlugins.INSTANCE.disable(plugin);
                            ok = true;
                        }
                    }
                }
            } catch (Throwable ignored) {
            }
            reply(callback, ok);
        });
    }

    public void deletePlugin(String pluginId) {
        deletePlugin(pluginId, null);
    }

    public void deletePlugin(String pluginId, org.telegram.messenger.Utilities.Callback<Object> callback) {
        runOnPluginsQueue(() -> {
            boolean ok = false;
            try {
                for (FGPluginsController.Plugin plugin : FGPluginsController.INSTANCE.installed()) {
                    if (plugin.getId().equals(pluginId)) {
                        FGPluginsController.INSTANCE.delete(plugin);
                        ok = true;
                    }
                }
                for (FGDexPlugins.Plugin plugin : FGDexPlugins.INSTANCE.installed()) {
                    if (plugin.getId().equals(pluginId)) {
                        FGDexPlugins.INSTANCE.delete(plugin);
                        ok = true;
                    }
                }
                if (ok) {
                    clearPluginSettingsPreferences(pluginId, false);
                }
            } catch (Throwable ignored) {
            }
            reply(callback, ok);
        });
    }

    private static void reply(org.telegram.messenger.Utilities.Callback<Object> callback, boolean ok) {
        if (callback == null) {
            return;
        }
        AndroidUtilities.runOnUIThread(() -> {
            try {
                callback.run(ok);
            } catch (Throwable ignored) {
            }
        });
    }

    public void notifyPluginsChanged() {
    }

    public void showInstallDialog(BaseFragment fragment, org.telegram.messenger.MessageObject message) {
        if (message == null) {
            return;
        }
        com.th3nekit.finegram.plugins.FGPluginsHook.handleMessage(message, message.getDocumentName());
    }

    public org.telegram.messenger.SendMessagesHelper.SendMessageParams executeSendMessageHook(
            int account, org.telegram.messenger.SendMessagesHelper.SendMessageParams params) {
        try {
            final com.th3nekit.finegram.plugins.FGPluginsDispatcher.Verdict verdict =
                    com.th3nekit.finegram.plugins.FGPluginsDispatcher.onSendMessageHooked(account, params);
            if (verdict != null && verdict.cancel) {
                return null;
            }
            if (verdict != null && verdict.replacement instanceof org.telegram.messenger.SendMessagesHelper.SendMessageParams) {
                return (org.telegram.messenger.SendMessagesHelper.SendMessageParams) verdict.replacement;
            }
        } catch (Throwable ignored) {
        }
        return params;
    }

    public static final class HookResult {
        private Object result;
        private boolean cancel;
        private boolean isFinal;

        public HookResult(Object result, boolean cancel, boolean isFinal) {
            this.result = result;
            this.cancel = cancel;
            this.isFinal = isFinal;
        }

        public Object getResult() {
            return result;
        }

        public void setResult(Object result) {
            this.result = result;
        }

        public boolean getCancel() {
            return cancel;
        }

        public void setCancel(boolean cancel) {
            this.cancel = cancel;
        }

        public boolean isFinal() {
            return isFinal;
        }

        public void setFinal(boolean isFinal) {
            this.isFinal = isFinal;
        }
    }

    public interface PluginsEngine {
        void init(Runnable onReady);

        boolean isEngineAvailable();

        boolean isPlugin(java.io.File file, org.telegram.messenger.MessageObject message);

        String getPluginPath(String pluginId);

        Object getPluginSetting(String pluginId, String key, Object fallback);

        void setPluginSetting(String pluginId, String key, Object value);

        Map<String, Object> getAllPluginSettings(String pluginId);

        void clearPluginSettings(String pluginId);

        java.util.List<Object> loadPluginSettings(String pluginId);

        void openPluginSettings(String pluginId, BaseFragment fragment);

        void openPluginSettings(Plugin plugin, BaseFragment fragment);

        void openPluginSetting(String pluginId, String key, BaseFragment fragment);

        void openPluginSetting(Plugin plugin, String key, BaseFragment fragment);

        void setPluginEnabled(String pluginId, boolean enabled, org.telegram.messenger.Utilities.Callback<Object> callback);

        void deletePlugin(String pluginId, org.telegram.messenger.Utilities.Callback<Object> callback);

        void sharePlugin(String pluginId);

        void showInstallDialog(BaseFragment fragment,
                               com.exteragram.messenger.plugins.ui.components.InstallPluginBottomSheet.PluginInstallParams params);

        boolean canOpenInExternalApp();

        void openInExternalApp(String pluginId);

        void executeOnAppEvent(String event);

        void checkDevServer();

        void shutdown(Runnable onDone);
    }

    public void setPluginSetting(String pluginId, String key, Object value) {
        if (pluginId == null || key == null) {
            return;
        }
        FGPluginsController.INSTANCE.setSetting(pluginId, key, value);
    }

    public static Map<String, Object> engines = new LinkedHashMap<>();

    public static Map<String, Object> getEngines() {
        Map<String, Object> out = new LinkedHashMap<>();
        out.put(PluginsConstants.PYTHON, PythonPluginsEngine.getInstance());
        engines = out;
        return out;
    }

    private static final Map<String, java.util.List<Object>> ownHooks = new java.util.HashMap<>();

    public void addXposedHook(String pluginId, Object unhook) {
        if (pluginId == null || unhook == null) {
            return;
        }
        synchronized (ownHooks) {
            java.util.List<Object> list = ownHooks.get(pluginId);
            if (list == null) {
                list = new java.util.ArrayList<>();
                ownHooks.put(pluginId, list);
            }
            list.add(unhook);
        }
    }

    public void addXposedHook(String pluginId, de.robv.android.xposed.XC_MethodHook.Unhook unhook) {
        addXposedHook(pluginId, (Object) unhook);
    }

    public void removeXposedHook(String pluginId, de.robv.android.xposed.XC_MethodHook.Unhook unhook) {
        removeXposedHook(pluginId, (Object) unhook);
    }

    public void addXposedHooks(String pluginId, java.util.ArrayList<?> unhooks) {
        if (unhooks == null) {
            return;
        }
        for (Object unhook : unhooks) {
            addXposedHook(pluginId, unhook);
        }
    }

    public void removeXposedHook(String pluginId, Object unhook) {
        if (unhook == null) {
            return;
        }
        synchronized (ownHooks) {
            java.util.List<Object> list = ownHooks.get(pluginId);
            if (list != null) {
                list.remove(unhook);
            }
        }
        undo(unhook);
    }

    public static boolean removeMenuItem(String pluginId, String itemId) {
        return false;
    }

    public static void removeAllXposedHooks(String pluginId) {
        java.util.List<Object> list;
        synchronized (ownHooks) {
            list = ownHooks.remove(pluginId);
        }
        if (list == null) {
            return;
        }
        for (Object unhook : list) {
            undo(unhook);
        }
    }

    private static void undo(Object unhook) {
        try {
            unhook.getClass().getMethod("unhook").invoke(unhook);
        } catch (Throwable ignored) {
        }
    }

    public Map<String, Object> plugins = Collections.emptyMap();

    public Map<String, Object> getPlugins() {
        Map<String, Object> out = new LinkedHashMap<>();
        for (FGPluginsController.Plugin plugin : FGPluginsController.INSTANCE.installed()) {
            out.put(plugin.getId(), plugin);
        }
        for (FGDexPlugins.Plugin plugin : FGDexPlugins.INSTANCE.installed()) {
            out.put(plugin.getId(), plugin);
        }
        plugins = out;
        return out;
    }

    private PluginsController() {
        try {
            plugins = getPlugins();
        } catch (Throwable ignored) {
            plugins = Collections.emptyMap();
        }
    }
}
