/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.models;

/**
 * Строка настроек, созданная плагином на стороне Java.
 *
 * Плагины exteraGram собирают свои экраны двумя способами: из Python — тогда работают наши
 * питоновские классы, — и из своего кода в dex, где строки создаются как объекты вот этих
 * моделей. Второй путь у нас раньше обрывался: рантайм не понимал такие объекты и просто
 * пропускал строку, отчего экран плагина выглядел полупустым.
 *
 * Поля публичные и названы так же, как у exteraGram: рантайм читает их по именам.
 */
public class SettingItem {

    /** Что это за строка: header, divider, switch, text, selector, input, edit_text. */
    public String type;

    /** Подпись под строкой; у части типов её нет. */
    public String subtext;

    /** Имя настройки в хранилище плагина; null у строк без значения. */
    public String key;

    /** Куда ведёт строка, если она открывает ссылку. */
    public String linkAlias;

    public SettingItem(String type) {
        this.type = type;
    }

    public SettingItem(String type, String subtext, Object onLongClick, String linkAlias) {
        this.type = type;
        this.subtext = subtext;
        this.linkAlias = linkAlias;
    }

    public String getType() {
        return type;
    }

    /** Плагин может освободить ссылки на Python перед выгрузкой; нам хранить нечего. */
    public void clearPythonReferences() {
    }
}
