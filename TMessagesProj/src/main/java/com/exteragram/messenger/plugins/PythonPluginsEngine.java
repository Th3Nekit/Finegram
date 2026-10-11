/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins;

import com.chaquo.python.PyObject;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.MessageObject;
import org.telegram.messenger.Utilities;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.LaunchActivity;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

import com.exteragram.messenger.plugins.ui.components.InstallPluginBottomSheet;
import com.th3nekit.finegram.plugins.FGPluginShare;
import com.th3nekit.finegram.plugins.FGPluginsController;
import com.th3nekit.finegram.plugins.FGPluginsHook;

public class PythonPluginsEngine implements PluginsController.PluginsEngine {

    private static final PythonPluginsEngine INSTANCE = new PythonPluginsEngine();

    public static PythonPluginsEngine getInstance() {
        return INSTANCE;
    }

    @Override
    public void openPluginSettings(Plugin plugin, BaseFragment fragment) {
        if (plugin == null) {
            return;
        }
        final BaseFragment host = fragment != null ? fragment : LaunchActivity.getLastFragment();
        if (host == null) {
            return;
        }
        final BaseFragment screen = PluginsController.settingsFragment(plugin.getId());
        if (screen != null) {
            host.presentFragment(screen);
        }
    }

    public final Map<String, Object> pluginInstances = new ConcurrentHashMap<>();

    public ConcurrentHashMap<String, Object> getPluginInstances() {
        refresh();
        return new ConcurrentHashMap<>(pluginInstances);
    }

    public Map<String, Object> refresh() {
        pluginInstances.clear();
        try {
            for (FGPluginsController.Plugin plugin : FGPluginsController.INSTANCE.installed()) {
                if (!plugin.getEnabled()) {
                    continue;
                }
                PyObject instance = FGPluginsController.INSTANCE.pluginInstance(plugin.getId());
                if (instance != null) {
                    pluginInstances.put(plugin.getId(), instance);
                }
            }
        } catch (Throwable ignored) {
        }
        return pluginInstances;
    }

    public void loadPluginFromFile(String path, Object unused, Utilities.Callback<Boolean> callback) {
        boolean installed = false;
        try {
            installed = FGPluginsController.INSTANCE.install(new File(path)) != null;
        } catch (Throwable ignored) {
        }
        if (callback != null) {
            final boolean result = installed;
            AndroidUtilities.runOnUIThread(() -> callback.run(result));
        }
    }

    @Override
    public void init(Runnable onReady) {
        PluginsController.getInstance().init(onReady);
    }

    @Override
    public boolean isEngineAvailable() {
        return true;
    }

    @Override
    public boolean isPlugin(File file, MessageObject message) {
        return PluginsController.isPlugin(file, message);
    }

    @Override
    public String getPluginPath(String pluginId) {
        return PluginsController.getInstance().getPluginPath(pluginId);
    }

    @Override
    public Object getPluginSetting(String pluginId, String key, Object fallback) {
        final Map<String, Object> all = PluginsController.getInstance().getPluginSettingsPreferences(pluginId);
        return all.containsKey(key) ? all.get(key) : fallback;
    }

    @Override
    public void setPluginSetting(String pluginId, String key, Object value) {
        PluginsController.getInstance().setPluginSetting(pluginId, key, value);
    }

    @Override
    public Map<String, Object> getAllPluginSettings(String pluginId) {
        return PluginsController.getInstance().getPluginSettingsPreferences(pluginId);
    }

    @Override
    public void clearPluginSettings(String pluginId) {
        PluginsController.getInstance().clearPluginSettingsPreferences(pluginId);
    }

    @Override
    public List<Object> loadPluginSettings(String pluginId) {
        final List<Object> out = new ArrayList<>();
        if (PluginsController.getInstance().hasPluginSettings(pluginId)) {
            out.add(pluginId);
        }
        return out;
    }

    @Override
    public void openPluginSettings(String pluginId, BaseFragment fragment) {
        openPluginSettings(Plugin.of(pluginId), fragment);
    }

    @Override
    public void openPluginSetting(String pluginId, String key, BaseFragment fragment) {
        openPluginSettings(Plugin.of(pluginId), fragment);
    }

    @Override
    public void openPluginSetting(Plugin plugin, String key, BaseFragment fragment) {
        openPluginSettings(plugin, fragment);
    }

    @Override
    public void setPluginEnabled(String pluginId, boolean enabled, Utilities.Callback<Object> callback) {
        PluginsController.getInstance().setPluginEnabled(pluginId, enabled, callback);
    }

    @Override
    public void deletePlugin(String pluginId, Utilities.Callback<Object> callback) {
        PluginsController.getInstance().deletePlugin(pluginId, callback);
    }

    @Override
    public void sharePlugin(String pluginId) {
        final BaseFragment host = LaunchActivity.getLastFragment();
        if (host != null && pluginId != null) {
            AndroidUtilities.runOnUIThread(() -> FGPluginShare.share(host, pluginId));
        }
    }

    @Override
    public void showInstallDialog(BaseFragment fragment, InstallPluginBottomSheet.PluginInstallParams params) {
        if (params == null || params.getFilePath() == null) {
            return;
        }
        final BaseFragment host = fragment != null ? fragment : LaunchActivity.getLastFragment();
        if (host != null) {
            FGPluginsHook.promptInstall(host, new File(params.getFilePath()));
        }
    }

    @Override
    public boolean canOpenInExternalApp() {
        return false;
    }

    @Override
    public void openInExternalApp(String pluginId) {
    }

    @Override
    public void executeOnAppEvent(String event) {
        com.th3nekit.finegram.plugins.FGPluginsDispatcher.onAppEvent(event);
    }

    @Override
    public void checkDevServer() {
    }

    @Override
    public void shutdown(Runnable onDone) {
        if (onDone != null) {
            AndroidUtilities.runOnUIThread(onDone);
        }
    }

    private PythonPluginsEngine() {
    }
}
