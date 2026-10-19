/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.ui;

import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.ActionBar.INavigationLayout;
import org.telegram.ui.LaunchActivity;

/**
 * Экран, поверх которого можно показать окно.
 *
 * {@code getLastFragment} отдаёт самый верхний экран и на этом останавливается. Верхним же
 * бывает слой, в котором показывать уже некому: шторка закрылась, экраны из слоя ушли, а
 * сам слой ещё лежит в стопке. Тогда вызов возвращает пустоту, и всё, что на него
 * опирается, считает, что показывать негде.
 *
 * Для окна установки плагина это кончалось тем, что файл уходил системе: человек нажимал на
 * плагин и получал «открыть с помощью» вместо окна установки — и так до перезапуска.
 * Поэтому здесь мы не верим одному ответу, а спускаемся по стопке до первого живого экрана.
 */
public final class FGHostScreen {

    private FGHostScreen() {
    }

    /** Живой экран или null, если показывать и правда негде. */
    public static BaseFragment find() {
        BaseFragment top = null;
        try {
            // Зовём первым: попутно он вычищает из стопки закрытые и пустые слои.
            top = LaunchActivity.getLastFragment();
        } catch (Throwable ignored) {
        }
        if (alive(top)) return top;

        final LaunchActivity activity = LaunchActivity.instance;
        if (activity == null) return null;

        try {
            for (int i = activity.sheetFragmentsStack.size() - 1; i >= 0; i--) {
                final INavigationLayout layer = activity.sheetFragmentsStack.get(i);
                final BaseFragment candidate = layer == null ? null : layer.getLastFragment();
                if (alive(candidate)) return candidate;
            }
        } catch (Throwable ignored) {
        }

        try {
            final INavigationLayout main = activity.getActionBarLayout();
            final BaseFragment candidate = main == null ? null : main.getLastFragment();
            if (alive(candidate)) return candidate;
        } catch (Throwable ignored) {
        }
        return null;
    }

    private static boolean alive(BaseFragment fragment) {
        return fragment != null && fragment.getParentActivity() != null;
    }
}
