/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.profile.pills;

import android.content.Context;
import android.widget.FrameLayout;

import org.telegram.ui.ActionBar.Theme;

/**
 * Карточка в профиле.
 *
 * Небольшая плитка под шапкой: в ней показывают что угодно — счётчик, погоду, свои
 * кнопки. Клиент знает только, как её расставить и когда она появилась на экране;
 * содержимое рисует тот, кто её объявил.
 */
public class FGPill extends FrameLayout {

    protected final Theme.ResourcesProvider resourcesProvider;

    /** Номер карточки: по нему её находят в списке. */
    private int pillId;

    public FGPill(Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.resourcesProvider = resourcesProvider;
        setWillNotDraw(false);
    }

    public Theme.ResourcesProvider getResourcesProvider() {
        return resourcesProvider;
    }

    public int getPillId() {
        return pillId;
    }

    public void setPillId(int pillId) {
        this.pillId = pillId;
    }

    /** Карточка появилась на экране: самое время обновить содержимое. */
    public void onShown() {
    }

    /** Карточку убрали с экрана: пора отпустить всё, что она держала. */
    public void onHidden() {
    }

    protected int getThemedColor(int key) {
        return Theme.getColor(key, resourcesProvider);
    }
}
