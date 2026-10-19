/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.preferences.cells;

import static org.telegram.messenger.AndroidUtilities.dp;

import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;

import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.RecyclerListView;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;
import org.telegram.ui.Components.UniversalRecyclerView;

/**
 * Строка настроек, которую нарисовал сам плагин.
 *
 * Плагин отдаёт готовый Android-вид, а мы вставляем его в список как обычную строку —
 * с нажатием и правильными отступами. Вид принадлежит плагину и переживает перерисовку
 * списка, поэтому перед вставкой его отвязывают от прежнего родителя.
 */
public class FGPluginCustomCell extends FrameLayout {

    private View content;
    private Runnable onClick;

    public FGPluginCustomCell(@NonNull Context context) {
        super(context);
        setOnClickListener(v -> {
            if (onClick != null) {
                onClick.run();
            }
        });
    }

    /** Показывает вид плагина; повторная привязка того же вида ничего не перестраивает. */
    public void set(View view, Runnable clickAction) {
        onClick = clickAction;
        setClickable(clickAction != null);
        setMinimumHeight(clickAction != null ? dp(48) : 0);
        if (content == view) {
            return;
        }
        removeAllViews();
        content = view;
        if (view == null) {
            return;
        }
        ViewGroup parent = view.getParent() instanceof ViewGroup ? (ViewGroup) view.getParent() : null;
        if (parent != null) {
            parent.removeView(view);
        }
        addView(view, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, Gravity.CENTER_VERTICAL));
    }

    /** Такая строка живёт в списке настроек наравне с остальными. */
    public static class Factory extends UItem.UItemFactory<FGPluginCustomCell> {
        static { setup(new Factory()); }

        @Override
        public FGPluginCustomCell createView(Context context, RecyclerListView listView, int currentAccount,
                                             int classGuid, Theme.ResourcesProvider resourcesProvider) {
            return new FGPluginCustomCell(context);
        }

        @Override
        public void bindView(View view, UItem item, boolean divider, UniversalAdapter adapter, UniversalRecyclerView listView) {
            Data data = (Data) item.object;
            ((FGPluginCustomCell) view).set(data.view, data.onClick);
        }

        /** Что нужно строке: вид от плагина и что делать по нажатию. */
        public static class Data {
            public final View view;
            public final Runnable onClick;

            public Data(View view, Runnable onClick) {
                this.view = view;
                this.onClick = onClick;
            }
        }

        public static UItem of(int id, Data data) {
            UItem item = UItem.ofFactory(Factory.class);
            item.id = id;
            item.object = data;
            return item;
        }
    }
}
