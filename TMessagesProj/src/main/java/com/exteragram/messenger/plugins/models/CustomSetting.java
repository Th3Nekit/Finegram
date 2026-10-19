/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.models;

import android.view.View;

import org.telegram.ui.Components.UItem;

import com.exteragram.messenger.plugins.Plugin;

/**
 * Своя строка настроек, которую плагин рисует сам.
 *
 * Плагин наследует {@link Factory} и возвращает оттуда готовый вид — так он вставляет в
 * наши настройки что угодно, от карточки до целого мини-экрана.
 */
public class CustomSetting {

    /** Что показать: плагин кладёт сюда всё, что нужно его фабрике. */
    public Object args;

    public CustomSetting() {
    }

    public CustomSetting(Object args) {
        this.args = args;
    }

    public Object getArgs() {
        return args;
    }

    /**
     * Фабрика вида: её пишет сам плагин.
     *
     * Это наша обычная фабрика строк, только под именем, по которому её ищут чужие
     * плагины.
     */
    public static abstract class Factory extends UItem.UItemFactory<View> {

        /** Строка для списка: плагин собирает её из своих данных. */
        public abstract UItem create(Plugin plugin, CustomSetting setting, Object args);
    }
}
