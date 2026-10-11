/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.preferences;

import com.th3nekit.finegram.core.helpers.DeeplinkHelper;

import static org.telegram.messenger.LocaleController.getString;

import android.os.Build;
import android.view.View;

import org.telegram.messenger.R;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;

import com.th3nekit.finegram.chats.FGMessageMenuInjector;
import com.th3nekit.finegram.core.configs.FinegramMessagesConfig;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

public class MessageMenuPreferencesEntry extends BaseCGPreferencesEntry {

    private final int enableNewMessageMenuRow = 1;
    private final int unifiedScrollRow = 2;
    private final int fixedMessageHeightRow = 4;

    private final int messageMenuItemsRow = 8;
    private final int messageMenuItemsCompactViewRow = 9;
    private final int forwardWoCaptionRow = 10;
    private final int getCustomReactionsRow = 11;
    private final int messageMenuOrderRow = 12;
    private final int messageMenuHapticRow = 13;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.CP_MessageMenu);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            items.add(UItem.asHeader(getString(R.string.AP_Header_Appearance)));
            items.add(SettingsHelper.asSwitchCG(enableNewMessageMenuRow, getString(R.string.CP_BlurMessageMenu), getString(R.string.CP_BlurMessageMenu_Desc))
                    .setChecked(FinegramMessagesConfig.INSTANCE.getBlurMessageMenuBackground())
            .slug("iosMsgMenu"));
            items.add(UItem.asShadow(null));
        }

        items.add(UItem.asHeader(getString(R.string.FG_MessageMenuBehaviour)));
        items.add(SettingsHelper.asSwitchCG(unifiedScrollRow, getString(R.string.FG_MessageMenuUnifiedScroll), getString(R.string.FG_MessageMenuUnifiedScroll_Desc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getMsgMenuUnifiedScroll())
        .slug("unifiedScroll"));
        items.add(SettingsHelper.asSwitchCG(fixedMessageHeightRow, getString(R.string.FG_MessageMenuFixedHeight), getString(R.string.FG_MessageMenuFixedHeight_Desc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getMsgMenuFixedHeight())
        .slug("comfortableHEight"));
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.LocalMiscellaneousCache)));
        items.add(UItem.asButton(messageMenuItemsRow, R.drawable.msg_list, getString(R.string.CP_MessageMenuItems)).slug("menuItems"));
        items.add(
                SettingsHelper.asSwitchCG
                        (
                            messageMenuItemsCompactViewRow,
                            getString(R.string.CP_MessageMenuCompactLayout),
                            getString(R.string.CP_MessageMenuCompactLayout_Desc) + "\n\n" + getString(R.string.CP_MessageMenuCompactLayout_Dot)
                        )
                .setChecked(FinegramMessagesConfig.INSTANCE.getMsgMenuItemsCompactView())
        .slug("compactMsgMenu"));
        items.add(UItem.asShadow(null));

        items.add(UItem.asButton(messageMenuOrderRow, R.drawable.msg_reorder, getString(R.string.FG_Menu_Reorder)));
        items.add(SettingsHelper.asSwitchCG(messageMenuHapticRow, getString(R.string.FG_MessageMenuHaptic))
                .setChecked(FinegramMessagesConfig.INSTANCE.getMessageMenuHaptic()));
        items.add(UItem.asShadow(getString(R.string.FG_Menu_Reorder_Desc)));

        items.add(UItem.asHeader(getString(R.string.FG_MenuExtras)));
        items.add(SettingsHelper.asSwitchCG(forwardWoCaptionRow, getString(R.string.FG_ForwardWoCaption))
                .setChecked(FinegramMessagesConfig.INSTANCE.getForwardWithoutCaption()));
        items.add(SettingsHelper.asSwitchCG(getCustomReactionsRow, getString(R.string.FG_GetCustomReactions))
                .setChecked(FinegramMessagesConfig.INSTANCE.getGetCustomReactions()));
        items.add(UItem.asShadow(getString(R.string.FG_MenuExtras_Desc)));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        var holder = listView.findViewHolderForAdapterPosition(position);
        if (holder == null || !listView.adapter.isEnabled(holder)) {
            return;
        }
        if (item.id == enableNewMessageMenuRow) {
            FinegramMessagesConfig.INSTANCE.setBlurMessageMenuBackground(!FinegramMessagesConfig.INSTANCE.getBlurMessageMenuBackground());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getBlurMessageMenuBackground());

            updateRows(true);
        } else if (item.id == unifiedScrollRow) {
            FinegramMessagesConfig.INSTANCE.setMsgMenuUnifiedScroll(!FinegramMessagesConfig.INSTANCE.getMsgMenuUnifiedScroll());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getMsgMenuUnifiedScroll());
            updateRows(true);
        } else if (item.id == fixedMessageHeightRow) {
            FinegramMessagesConfig.INSTANCE.setMsgMenuFixedHeight(!FinegramMessagesConfig.INSTANCE.getMsgMenuFixedHeight());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getMsgMenuFixedHeight());
        } else if (item.id == messageMenuItemsRow) {
            FGMessageMenuInjector.INSTANCE.showMessageMenuItemsConfigurator(this);
        } else if (item.id == messageMenuOrderRow) {
            presentFragment(new MessageMenuOrderPreferencesEntry());
        } else if (item.id == messageMenuHapticRow) {
            FinegramMessagesConfig.INSTANCE.setMessageMenuHaptic(!FinegramMessagesConfig.INSTANCE.getMessageMenuHaptic());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getMessageMenuHaptic());
        } else if (item.id == forwardWoCaptionRow) {
            FinegramMessagesConfig.INSTANCE.setForwardWithoutCaption(!FinegramMessagesConfig.INSTANCE.getForwardWithoutCaption());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getForwardWithoutCaption());
        } else if (item.id == getCustomReactionsRow) {
            FinegramMessagesConfig.INSTANCE.setGetCustomReactions(!FinegramMessagesConfig.INSTANCE.getGetCustomReactions());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getGetCustomReactions());
        } else if (item.id == messageMenuItemsCompactViewRow) {
            FinegramMessagesConfig.INSTANCE.setMsgMenuItemsCompactView(!FinegramMessagesConfig.INSTANCE.getMsgMenuItemsCompactView());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getMsgMenuItemsCompactView());
        }
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        return false;
    }

    @Override
    protected String getKey() {
        return DeeplinkHelper.DeepLinksRepo.FG_Message_Menu;
    }
}
