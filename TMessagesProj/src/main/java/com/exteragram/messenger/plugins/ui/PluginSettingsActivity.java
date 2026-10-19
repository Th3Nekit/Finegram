/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.ui;

import android.content.Context;
import android.view.View;

import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;

import com.th3nekit.finegram.plugins.FGDexPlugins;
import com.th3nekit.finegram.plugins.FGPluginsController;
import com.th3nekit.finegram.preferences.PluginDetailsPreferencesEntry;

/**
 * Переходник для плагинов exteraGram: они создают этот экран сами, передавая свой объект
 * плагина, и показывают его через presentFragment.
 *
 * Это и есть наш экран настроек плагина — класс только принимает чужой вызов и достаёт из
 * переданного объекта имя плагина.
 */
public class PluginSettingsActivity extends PluginDetailsPreferencesEntry {

    public PluginSettingsActivity(Object plugin) {
        super(idOf(plugin));
    }

    /**
     * Сборка списка строк экрана.
     *
     * Метод повторяет тот, что уже есть у предка, и ничего к нему не добавляет — но
     * объявлен именно здесь. Плагины ищут его через getDeclaredMethod, а тот смотрит
     * только в самом классе и не заглядывает в родителей: без этой строки Custom Profile
     * не может повесить свою шапку и пишет в журнал «хук не встал».
     */
    @Override
    public void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        super.fillItems(items, adapter);
    }

    /**
     * Создание корневого вида экрана.
     *
     * Объявлено здесь по той же причине, что и fillItems: плагин ищет метод в самом классе,
     * а не в родителях, и без объявления не может вставить свою шапку.
     */
    @Override
    public View createView(Context context) {
        return super.createView(context);
    }

    /** Плагин может передать сюда что угодно: свой объект, наш объект или просто имя. */
    private static String idOf(Object plugin) {
        if (plugin instanceof FGPluginsController.Plugin) {
            return ((FGPluginsController.Plugin) plugin).getId();
        }
        if (plugin instanceof FGDexPlugins.Plugin) {
            return ((FGDexPlugins.Plugin) plugin).getId();
        }
        if (plugin == null) {
            return "";
        }
        // у чужого объекта спросим имя так, как это делает сам exteraGram
        try {
            return String.valueOf(plugin.getClass().getMethod("getId").invoke(plugin));
        } catch (Throwable ignored) {
        }
        return String.valueOf(plugin);
    }
}
