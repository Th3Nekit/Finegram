/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins;

public final class PluginsConstants {

    public static final String PYTHON = "python";

    public static final class Xposed {
        public static final String HOOK = "hook";
        public static final String REPLACE = "replace";
        public static final String BEFORE = "before";
        public static final String AFTER = "after";

        public static final String BEFORE_HOOKED_METHOD = "before_hooked_method";
        public static final String AFTER_HOOKED_METHOD = "after_hooked_method";
        public static final String REPLACE_HOOKED_METHOD = "replace_hooked_method";

        public static final String HOOK_FILTERS = "__hook_filters__";

        private Xposed() {
        }
    }

    public static final class MenuItemProperties {
        public static final String MENU_TYPE = "menu_type";
        public static final String ITEM_ID = "item_id";
        public static final String TEXT = "text";
        public static final String ICON = "icon";
        public static final String PRIORITY = "priority";
        public static final String ON_CLICK = "on_click";

        private MenuItemProperties() {
        }
    }

    public static final class MenuItemTypes {
        public static final String DRAWER_MENU = "drawer_menu";
        public static final String CHAT_MENU = "chat_menu";
        public static final String PROFILE_MENU = "profile_menu";

        private MenuItemTypes() {
        }
    }

    private PluginsConstants() {
    }
}
