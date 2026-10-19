/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.ui.components.templates;

import android.content.Context;
import android.view.View;

import org.telegram.messenger.FileLog;
import org.telegram.ui.ActionBar.ActionBar;
import org.telegram.ui.ActionBar.ActionBarMenu;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;

/**
 * Готовый экран-список для плагинов: заголовок, строки, нажатия — всё отдаёт делегат.
 *
 * Плагину не нужно писать свой фрагмент: он описывает строки в fillItems, а мы строим
 * экран тем же списком, что и наши настройки. Делегат обычно написан на Python, и методы,
 * которых плагин не реализовал, мы тихо пропускаем — берём поведение по умолчанию.
 */
public class UniversalFragment extends org.telegram.ui.Components.UniversalFragment {

    /** Что плагин рассказывает об экране. Всё необязательно, кроме fillItems. */
    public interface UniversalFragmentDelegate {
        default CharSequence getTitle() {
            return null;
        }

        void fillItems(ArrayList<UItem> items, UniversalAdapter adapter);

        default void onClick(UItem item, View view, int position, float x, float y) {
        }

        default boolean onLongClick(UItem item, View view, int position, float x, float y) {
            return false;
        }

        /** Свой вид вместо списка; null — строить список. */
        default View beforeCreateView() {
            return null;
        }

        /** Последний шанс подменить или дополнить готовый вид; null — оставить как есть. */
        default View afterCreateView(View view) {
            return view;
        }

        default void onMenuItemClick(int id) {
        }

        /** null — обычное поведение «назад»; true/false — разрешить или запретить. */
        default Boolean onBackPressed() {
            return null;
        }

        default void onFragmentCreate() {
        }

        default void onFragmentDestroy() {
        }
    }

    private UniversalFragmentDelegate delegate;

    public UniversalFragment() {
    }

    public UniversalFragment(UniversalFragmentDelegate delegate) {
        this.delegate = delegate;
    }

    public UniversalFragmentDelegate getDelegate() {
        return delegate;
    }

    public void setDelegate(UniversalFragmentDelegate delegate) {
        this.delegate = delegate;
    }

    private static void report(String where, Throwable error) {
        FileLog.e("экран плагина: " + where, error);
    }

    @Override
    public View createView(Context context) {
        if (delegate != null) {
            try {
                final View custom = delegate.beforeCreateView();
                if (custom != null) {
                    return fragmentView = custom;
                }
            } catch (Throwable e) {
                report("beforeCreateView", e);
            }
        }
        final View built = super.createView(context);
        actionBar.setActionBarMenuOnItemClick(new ActionBar.ActionBarMenuOnItemClick() {
            @Override
            public void onItemClick(int id) {
                if (id == -1) {
                    finishFragment();
                } else if (delegate != null) {
                    try {
                        delegate.onMenuItemClick(id);
                    } catch (Throwable e) {
                        report("onMenuItemClick", e);
                    }
                }
            }
        });
        if (delegate != null) {
            try {
                final View replaced = delegate.afterCreateView(built);
                if (replaced != null && replaced != built) {
                    return fragmentView = replaced;
                }
            } catch (Throwable e) {
                report("afterCreateView", e);
            }
        }
        return built;
    }

    /** Меню справа в шапке: плагин добавляет в него свои пункты. */
    public ActionBarMenu getActionBarMenu() {
        return actionBar == null ? null : actionBar.createMenu();
    }

    /** Поменять заголовок на ходу, с анимацией или без. */
    public void setTitle(CharSequence title, boolean animated, long duration) {
        if (actionBar == null) {
            return;
        }
        if (animated) {
            actionBar.setTitleAnimated(title, true, duration);
        } else {
            actionBar.setTitle(title);
        }
    }

    @Override
    public CharSequence getTitle() {
        if (delegate != null) {
            try {
                final CharSequence title = delegate.getTitle();
                if (title != null) {
                    return title;
                }
            } catch (Throwable e) {
                report("getTitle", e);
            }
        }
        return "";
    }

    @Override
    public void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        if (delegate == null) {
            return;
        }
        try {
            delegate.fillItems(items, adapter);
        } catch (Throwable e) {
            report("fillItems", e);
        }
    }

    @Override
    public void onClick(UItem item, View view, int position, float x, float y) {
        if (delegate == null) {
            return;
        }
        try {
            delegate.onClick(item, view, position, x, y);
        } catch (Throwable e) {
            report("onClick", e);
        }
    }

    @Override
    public boolean onLongClick(UItem item, View view, int position, float x, float y) {
        if (delegate == null) {
            return false;
        }
        try {
            return delegate.onLongClick(item, view, position, x, y);
        } catch (Throwable e) {
            report("onLongClick", e);
            return false;
        }
    }

    @Override
    public boolean onBackPressed(boolean invoked) {
        if (delegate != null) {
            try {
                final Boolean decision = delegate.onBackPressed();
                if (decision != null) {
                    return decision;
                }
            } catch (Throwable e) {
                report("onBackPressed", e);
            }
        }
        return super.onBackPressed(invoked);
    }

    @Override
    public boolean onFragmentCreate() {
        if (delegate != null) {
            try {
                delegate.onFragmentCreate();
            } catch (Throwable e) {
                report("onFragmentCreate", e);
            }
        }
        return super.onFragmentCreate();
    }

    @Override
    public void onFragmentDestroy() {
        if (delegate != null) {
            try {
                delegate.onFragmentDestroy();
            } catch (Throwable e) {
                report("onFragmentDestroy", e);
            }
        }
        super.onFragmentDestroy();
    }
}
