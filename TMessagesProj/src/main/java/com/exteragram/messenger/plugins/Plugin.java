/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins;

import com.th3nekit.finegram.plugins.FGPluginsController;

public class Plugin {

    public final String id;
    public final String name;
    public final String version;
    public final String author;
    public final String description;
    public final boolean enabled;
    private String engine = PluginsConstants.PYTHON;

    public final FGPluginsController.Plugin origin;

    public Plugin(FGPluginsController.Plugin origin) {
        this.origin = origin;
        this.id = origin.getId();
        this.name = origin.getName();
        this.version = origin.getVersion();
        this.author = origin.getAuthor();
        this.description = origin.getDescription();
        this.enabled = origin.getEnabled();
    }

    public static Plugin of(String pluginId) {
        if (pluginId == null || pluginId.isEmpty()) {
            return null;
        }
        for (FGPluginsController.Plugin plugin : FGPluginsController.INSTANCE.installed()) {
            if (pluginId.equals(plugin.getId())) {
                return new Plugin(plugin);
            }
        }
        for (com.th3nekit.finegram.plugins.FGDexPlugins.Plugin plugin :
                com.th3nekit.finegram.plugins.FGDexPlugins.INSTANCE.installed()) {
            if (pluginId.equals(plugin.getId())) {
                return new Plugin(plugin);
            }
        }
        return new Plugin(pluginId, pluginId, "", "", "", false);
    }

    private Plugin(String id, String name, String version, String author, String description,
                   boolean enabled) {
        this.origin = null;
        this.id = id;
        this.name = name;
        this.version = version;
        this.author = author;
        this.description = description;
        this.enabled = enabled;
    }

    public Plugin(com.th3nekit.finegram.plugins.FGDexPlugins.Plugin origin) {
        this(origin.getId(), origin.getName(), origin.getVersion(), origin.getAuthor(),
                origin.getDescription(), origin.getEnabled());
        engine = "dex";
    }

    public String getEngine() {
        return engine;
    }

    public Throwable getError() {
        String error = FGPluginsController.INSTANCE.lastError(id);
        return error == null || error.isEmpty() ? null : new IllegalStateException(error);
    }

    public String getId() {
        return id;
    }

    public String getName() {
        return name;
    }

    public String getVersion() {
        return version;
    }

    public String getAuthor() {
        return author;
    }

    public String getDescription() {
        return description;
    }

    public boolean isEnabled() {
        return enabled;
    }

    public String getIcon() {
        return origin != null ? origin.getIcon() : null;
    }

    @Override
    public String toString() {
        return name + " " + version;
    }
}
