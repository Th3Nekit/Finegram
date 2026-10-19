/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.pillstack.ui.pills;

import android.content.Context;

import org.telegram.ui.ActionBar.Theme;

import com.th3nekit.finegram.profile.pills.FGPill;

/**
 * Переходник для плагинов exteraGram: карточка профиля под тем именем, под которым они её
 * наследуют. Сама карточка — наша.
 */
public class BasePill extends FGPill {

    public BasePill(Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context, resourcesProvider);
    }

    /** У них это называется так; у нас — onShown. */
    public void onPillShown() {
    }

    public void onPillHidden() {
    }

    @Override
    public void setPressed(boolean pressed) {
        super.setPressed(pressed);
    }

    @Override
    public void onShown() {
        onPillShown();
    }

    @Override
    public void onHidden() {
        onPillHidden();
    }
}
