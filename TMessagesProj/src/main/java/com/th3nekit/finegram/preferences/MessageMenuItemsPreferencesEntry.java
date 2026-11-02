/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.preferences;

import static org.telegram.messenger.LocaleController.getString;

import android.view.View;

import org.telegram.messenger.R;
import org.telegram.ui.ChatActivity;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

import com.th3nekit.finegram.chats.MessageInfoDialog;
import com.th3nekit.finegram.chats.helpers.ChatActivityHelper;
import com.th3nekit.finegram.chats.ui.MessageMenuHold;
import com.th3nekit.finegram.core.configs.FinegramMessagesConfig;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

public class MessageMenuItemsPreferencesEntry extends BaseCGPreferencesEntry {

    private static final int ROW_BASE = 100;

    private final ArrayList<Row> rows = new ArrayList<>();

    private static final class Row {
        final int header;
        final CharSequence title;
        final BooleanSupplier visible;
        final Consumer<Boolean> setVisible;

        Row(int header, CharSequence title, BooleanSupplier visible, Consumer<Boolean> setVisible) {
            this.header = header;
            this.title = title;
            this.visible = visible;
            this.setVisible = setVisible;
        }
    }

    public MessageMenuItemsPreferencesEntry() {
        FinegramMessagesConfig config = FinegramMessagesConfig.INSTANCE;
        int main = R.string.FG_MenuItems_Main;
        int media = R.string.FG_MenuItems_Media;
        int extra = R.string.FG_MenuItems_Finegram;

        flag(main, getString(R.string.Reply), config::getShowReply, config::setShowReply);
        option(main, getString(R.string.Copy), ChatActivity.OPTION_COPY);
        flag(main, getString(R.string.Forward), config::getShowForward, config::setShowForward);
        option(main, getString(R.string.Edit), ChatActivity.OPTION_EDIT);
        option(main, getString(R.string.PinMessage), ChatActivity.OPTION_PIN);
        option(main, getString(R.string.TranslateMessage), ChatActivity.OPTION_TRANSLATE);
        option(main, getString(R.string.CopyLink), ChatActivity.OPTION_COPY_LINK);
        option(main, getString(R.string.ViewThread), ChatActivity.OPTION_VIEW_REPLIES_OR_THREAD);
        option(main, getString(R.string.ViewStatistics), ChatActivity.OPTION_STATISTICS);
        option(main, getString(R.string.Select), MessageMenuHold.OPTION_SELECT);
        flag(main, getString(R.string.ReportChat), config::getShowReport, config::setShowReport);
        option(main, getString(R.string.Delete), ChatActivity.OPTION_DELETE);

        flag(media, getString(R.string.SaveToGallery), config::getShowSaveToGallery, config::setShowSaveToGallery);
        flag(media, getString(R.string.SaveToDownloads), config::getShowSaveToDownloads, config::setShowSaveToDownloads);
        flag(media, getString(R.string.ShareFile), config::getShowShare, config::setShowShare);
        flag(media, getString(R.string.FG_CopyPhoto), config::getShowCopyPhoto, config::setShowCopyPhoto);
        flag(media, getString(R.string.FG_CopyPhotoAsSticker), config::getShowCopyPhotoAsSticker, config::setShowCopyPhotoAsSticker);
        option(media, getString(R.string.SaveToGIFs), ChatActivity.OPTION_ADD_TO_GIFS);
        option(media, getString(R.string.AddToStickers), ChatActivity.OPTION_ADD_TO_STICKERS_OR_MASKS);
        option(media, getString(R.string.AddToFavorites), ChatActivity.OPTION_ADD_STICKER_TO_FAVORITES);
        option(media, getString(R.string.FG_SaveSticker), ChatActivityHelper.OPTION_DOWNLOAD_STICKER);
        flag(media, getString(R.string.OpenInExternalApp), config::getOpenInExternalApp, config::setOpenInExternalApp);
        flag(media, getString(R.string.FG_ClearFromCache), config::getShowClearFromCache, config::setShowClearFromCache);

        option(extra, getString(R.string.FG_MessageInfoTitle), MessageInfoDialog.OPTION_MESSAGE_INFO);
        flag(extra, getString(R.string.FG_Quote_Create), config::getCreateQuoteImage, config::setCreateQuoteImage);
        option(extra, getString(R.string.FG_EditMessageLocal), ChatActivity.OPTION_EDIT_LOCAL);
        flag(extra, getString(R.string.Forward) + " " + getString(R.string.FG_Without_Authorship),
                config::getShowForwardWoAuthorship, config::setShowForwardWoAuthorship);
        flag(extra, getString(R.string.FG_ForwardWoCaption), config::getForwardWithoutCaption, config::setForwardWithoutCaption);
        flag(extra, getString(R.string.FG_GetCustomReactions), config::getGetCustomReactions, config::setGetCustomReactions);
        flag(extra, getString(R.string.AvatarPreviewSearchMessages), config::getShowViewHistory, config::setShowViewHistory);
        flag(extra, getString(R.string.FG_ToSaved), config::getShowSaveMessage, config::setShowSaveMessage);
        flag(extra, getString(R.string.SaveForNotifications), config::getShowSaveForNotifications, config::setShowSaveForNotifications);
        flag(extra, getString(R.string.CP_GeminiAI_Header), config::getShowGemini, config::setShowGemini);
        flag(extra, "JSON", config::getShowJSON, config::setShowJSON);
    }

    private void flag(int header, CharSequence title, BooleanSupplier visible, Consumer<Boolean> setVisible) {
        rows.add(new Row(header, title, visible, setVisible));
    }

    private void option(int header, CharSequence title, int option) {
        rows.add(new Row(header, title,
                () -> !FinegramMessagesConfig.isMenuOptionHidden(option),
                show -> FinegramMessagesConfig.setMenuOptionHidden(option, !show)));
    }

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.CP_MessageMenuItems);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        int header = 0;
        for (int i = 0; i < rows.size(); i++) {
            Row row = rows.get(i);
            if (row.header != header) {
                if (header != 0) {
                    items.add(UItem.asShadow(null));
                }
                header = row.header;
                items.add(UItem.asHeader(getString(header)));
            }
            items.add(SettingsHelper.asSwitchCG(ROW_BASE + i, row.title).setChecked(row.visible.getAsBoolean()));
        }
        items.add(UItem.asShadow(getString(R.string.FG_MenuItems_Hint)));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        int index = item.id - ROW_BASE;
        if (index < 0 || index >= rows.size()) {
            return;
        }
        Row row = rows.get(index);
        boolean visible = !row.visible.getAsBoolean();
        row.setVisible.accept(visible);
        SettingsHelper.updateCheckState(view, visible);
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        return false;
    }
}
