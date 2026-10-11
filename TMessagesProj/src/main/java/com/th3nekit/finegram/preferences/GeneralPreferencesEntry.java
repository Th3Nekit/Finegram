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

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.text.format.DateUtils;
import android.view.View;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.DialogObject;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.NotificationsService;
import org.telegram.messenger.R;
import org.telegram.tgnet.TLRPC;
import org.telegram.tgnet.tl.TL_stars;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.AnimatedEmojiDrawable;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.Reactions.ReactionsLayoutInBubble;
import org.telegram.ui.SelectAnimatedEmojiDialog;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;
import org.telegram.ui.UsersSelectActivity;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;

import com.th3nekit.finegram.core.FinegramLogger;
import com.th3nekit.finegram.core.configs.FinegramCoreConfig;
import com.th3nekit.finegram.core.helpers.PushHealth;
import com.th3nekit.finegram.helpers.ui.PopupHelper;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

public class GeneralPreferencesEntry extends BaseCGPreferencesEntry {

    private final int springAnimationRow = 1;
    private final int actionbarCrossfadeRow = 2;
    private final int predictiveBackRow = 3;

    private final int silenceNonContactsRow = 4;

    private final int ignoreMentionsRow = 5;
    private final int ignoreMentionsExclusionsRow = 6;
    private final int ignoreMentionsAutoReadRow = 7;

    private final int defaultNotificationIconRow = 8;
    private final int residentNotificationRow = 9;

    private final int hideStoriesRow = 10;
    private final int archiveStoriesRow = 11;
    private final int archiveStoriesUsersRow = 12;
    private final int archiveStoriesChannelsRow = 13;

    private final int useSystemEmojiRow = 14;
    private final int useSystemFontsRow = 15;
    private final int tabledModeRow = 16;

    private final int downloadSpeedBoostRow = 17;
    private final int uploadSpeedBoostRow = 18;
    private final int slowNetworkMode = 19;

    private final int safeStarsRow = 20;

    private final int notificationReactionsRow = 21;
    private final int notificationReactionEmojiRow = 22;

    private NotificationReactionCell notificationReactionCell;
    private SelectAnimatedEmojiDialog.SelectAnimatedEmojiDialogWindow selectReactionDialog;

    private boolean expandedArchiveStoriesSection = false;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.AP_Header_General);
    }

    private String pushStatus() {
        if (!ApplicationLoader.checkPlayServices()) {
            return getString(R.string.FG_PushStatusNoServices);
        }
        long last = PushHealth.lastPushAt();
        if (last == 0) {
            return getString(R.string.FG_PushStatusNever);
        }
        CharSequence ago = DateUtils.getRelativeTimeSpanString(last, System.currentTimeMillis(), DateUtils.MINUTE_IN_MILLIS);
        return LocaleController.formatString(R.string.FG_PushStatusLast, ago);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        items.add(UItem.asHeader(getString(R.string.LiteMode)));
        items.add(UItem.asButton(springAnimationRow, getString(R.string.EP_NavigationAnimation), getSpringValue()).slug("navigation"));
        if (FinegramCoreConfig.INSTANCE.getSpringAnimation() == FinegramCoreConfig.ANIMATION_SPRING) {
            items.add(SettingsHelper.asSwitchCG(actionbarCrossfadeRow, getString(R.string.EP_NavigationAnimationCrossfading))
                    .setChecked(FinegramCoreConfig.INSTANCE.getActionbarCrossfade())
            .slug("actionbarCrossfade"));
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            items.add(SettingsHelper.asSwitchCG(predictiveBackRow, getString(R.string.FG_PredictiveBackAnimation))
                    .setChecked(FinegramCoreConfig.INSTANCE.getPredictiveBack())
            .slug("predictiveBack"));
        }
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.SettingsNotifications)));
        items.add(SettingsHelper.asSwitchCG(silenceNonContactsRow, getString(R.string.CP_SilenceNonContacts), getString(R.string.CP_SilenceNonContacts_Desc))
                .setChecked(FinegramCoreConfig.INSTANCE.getSilenceNonContacts())
        .slug("silenceNonContacts"));
        items.add(SettingsHelper.asSwitchCG(notificationReactionsRow,
                        getString(R.string.FG_NotificationReactions),
                        getString(R.string.FG_NotificationReactions_Desc))
                .setChecked(FinegramCoreConfig.INSTANCE.getNotificationReactions())
        );
        if (FinegramCoreConfig.INSTANCE.getNotificationReactions()) {
            if (notificationReactionCell == null) {
                notificationReactionCell = new NotificationReactionCell(getContext());
            }
            notificationReactionCell.update(false);
            items.add(UItem.asCustom(notificationReactionEmojiRow, notificationReactionCell));
        }
        items.add(SettingsHelper.asSwitchCG(defaultNotificationIconRow, getString(R.string.AP_Old_Notification_Icon))
                .setChecked(FinegramCoreConfig.INSTANCE.getOldNotificationIcon())
        .slug("oldNotifIcon"));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            items.add(SettingsHelper.asSwitchCG(residentNotificationRow, getString(R.string.FG_ResidentNotification), pushStatus())
                    .setChecked(FinegramCoreConfig.INSTANCE.getResidentNotification())
            .slug("residentNotification"));
        }
        items.add(UItem.asShadow(null));

        items.add(
                SettingsHelper.asSwitchCG(
                        ignoreMentionsRow,
                        SettingsHelper.applyNewSpan(getString(R.string.FG_IgnoreMentions)),
                        getString(R.string.FG_IgnoreMentionsDesc)
                )
                .setChecked(FinegramCoreConfig.INSTANCE.getIgnoreMentions())
        .slug("ignoreMentions"));

        if (FinegramCoreConfig.INSTANCE.getIgnoreMentions()) {
            items.add(UItem.asButton(ignoreMentionsExclusionsRow, R.drawable.msg_mention, getString(R.string.FG_IgnoreMentionsIgnoredChats), String.valueOf(getChatsNotificationHelper().getIgnoredChatsCount())));

            items.add(
                    SettingsHelper.asSwitchCG(
                            ignoreMentionsAutoReadRow,
                            getString(R.string.FG_IgnoreMentionsAutoRead),
                            getString(R.string.FG_IgnoreMentionsAutoReadDesc)
                    )
                    .setChecked(FinegramCoreConfig.INSTANCE.getIgnoreMentionsMarkAsRead())
            );
        }

        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.FilterStories)));
        items.add(SettingsHelper.asSwitchCG(hideStoriesRow, getString(R.string.CP_HideStories), getString(R.string.CP_HideStories_Desc))
                .setChecked(FinegramCoreConfig.INSTANCE.getHideStories())
        .slug("hideStories"));

        items.add(
                SettingsHelper.asExpandableSwitch(
                        archiveStoriesRow,
                        R.drawable.msg_archive,
                        getString(R.string.CP_ArchiveStories),
                        getArchiveStoriesCountText()
                )
                .setChecked(FinegramCoreConfig.INSTANCE.getArchiveStoriesFromUsers() || FinegramCoreConfig.INSTANCE.getArchiveStoriesFromChannels())
                .setCollapsed(!expandedArchiveStoriesSection)
                .setClickCallback(v -> {
                    boolean newValue = !(FinegramCoreConfig.INSTANCE.getArchiveStoriesFromUsers() || FinegramCoreConfig.INSTANCE.getArchiveStoriesFromChannels());

                    FinegramCoreConfig.INSTANCE.setArchiveStoriesFromUsers(newValue);
                    FinegramCoreConfig.INSTANCE.setArchiveStoriesFromChannels(newValue);

                    expandedArchiveStoriesSection = !expandedArchiveStoriesSection;
                    updateRows(true);
                })
        .slug("archiveStories"));
        if (expandedArchiveStoriesSection) {
            items.add(UItem.asRoundCheckbox(archiveStoriesUsersRow, getString(R.string.FilterContacts))
                    .setChecked(FinegramCoreConfig.INSTANCE.getArchiveStoriesFromUsers())
                    .setPad(1)
            );

            items.add(UItem.asRoundCheckbox(archiveStoriesChannelsRow, getString(R.string.FilterChannels))
                    .setChecked(FinegramCoreConfig.INSTANCE.getArchiveStoriesFromChannels())
                    .setPad(1)
            );
        }

        items.add(UItem.asShadow(getString(R.string.CP_ArchiveStories_Desc)));

        items.add(UItem.asHeader(getString(R.string.LocalMiscellaneousCache)));
        items.add(SettingsHelper.asSwitchCG(useSystemEmojiRow, getString(R.string.AP_SystemEmoji))
                .setChecked(FinegramCoreConfig.INSTANCE.getSystemEmoji())
        .slug("systemEmoji"));
        items.add(SettingsHelper.asSwitchCG(useSystemFontsRow, getString(R.string.AP_SystemFonts))
                .setChecked(FinegramCoreConfig.INSTANCE.getSystemFonts())
        .slug("systemFonts"));
        items.add(UItem.asButton(tabledModeRow, getString(R.string.AP_Tablet_Mode), getTabletModeValue()).slug("tabletMode"));
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.EP_Network)));
        items.add(UItem.asButton(downloadSpeedBoostRow, getString(R.string.EP_DownloadSpeedBoost), getDownloadSpeedBoostText()).slug("downloadSpeedBoost"));
        items.add(SettingsHelper.asSwitchCG(uploadSpeedBoostRow, getString(R.string.EP_UploadloadSpeedBoost))
                .setChecked(FinegramCoreConfig.INSTANCE.getUploadSpeedBoost())
        .slug("uploadSpeedBoost"));
        items.add(SettingsHelper.asSwitchCG(slowNetworkMode, getString(R.string.EP_SlowNetworkMode))
                .setChecked(FinegramCoreConfig.INSTANCE.getSlowNetworkMode())
        .slug("slowNetworkMode"));
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.PaymentCheckout)));
        items.add(SettingsHelper.asSwitchCG(safeStarsRow, getString(R.string.FG_SafeStars_Switch))
                .setChecked(FinegramCoreConfig.INSTANCE.getAllowSafeStars())
        );
        items.add(UItem.asShadow(getString(R.string.FG_SafeStars_Switch_Desc)));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == springAnimationRow) {
            ArrayList<String> configStringKeys = new ArrayList<>();
            ArrayList<Integer> configValues = new ArrayList<>();

            configStringKeys.add(getString(R.string.EP_NavigationAnimationSpring));
            configValues.add(FinegramCoreConfig.ANIMATION_SPRING);

            configStringKeys.add(getString(R.string.EP_NavigationAnimationBezier));
            configValues.add(FinegramCoreConfig.ANIMATION_CLASSIC);

            PopupHelper.show(configStringKeys, getString(R.string.EP_NavigationAnimation), configValues.indexOf(FinegramCoreConfig.INSTANCE.getSpringAnimation()), getContext(), i -> {
                FinegramCoreConfig.INSTANCE.setSpringAnimation(configValues.get(i));
                SettingsHelper.updateButtonValue(view, getSpringValue());

                updateRows(true);

                showRestartBulletin();
            });
        } else if (item.id == actionbarCrossfadeRow) {
            FinegramCoreConfig.INSTANCE.setActionbarCrossfade(!FinegramCoreConfig.INSTANCE.getActionbarCrossfade());
            SettingsHelper.updateCheckState(view, FinegramCoreConfig.INSTANCE.getActionbarCrossfade());

            if (FinegramCoreConfig.INSTANCE.getActionbarCrossfade() && FinegramCoreConfig.INSTANCE.getPredictiveBack()) {
                FinegramCoreConfig.INSTANCE.setPredictiveBack(false);
                updateRows(true);
            }

            showRestartBulletin();
        } else if (item.id == predictiveBackRow) {
            FinegramCoreConfig.INSTANCE.setPredictiveBack(!FinegramCoreConfig.INSTANCE.getPredictiveBack());
            SettingsHelper.updateCheckState(view, FinegramCoreConfig.INSTANCE.getPredictiveBack());

            if (FinegramCoreConfig.INSTANCE.getPredictiveBack() && FinegramCoreConfig.INSTANCE.getActionbarCrossfade()) {
                FinegramCoreConfig.INSTANCE.setActionbarCrossfade(false);
                updateRows(true);
            }

            showRestartBulletin();
        } else if (item.id == notificationReactionsRow) {
            FinegramCoreConfig.INSTANCE.setNotificationReactions(!FinegramCoreConfig.INSTANCE.getNotificationReactions());
            SettingsHelper.updateCheckState(view, FinegramCoreConfig.INSTANCE.getNotificationReactions());
            updateRows(true);
        } else if (item.id == safeStarsRow) {
            FinegramCoreConfig.INSTANCE.setAllowSafeStars(!FinegramCoreConfig.INSTANCE.getAllowSafeStars());
            SettingsHelper.updateCheckState(view, FinegramCoreConfig.INSTANCE.getAllowSafeStars());
        } else if (item.id == silenceNonContactsRow) {
            FinegramCoreConfig.INSTANCE.setSilenceNonContacts(!FinegramCoreConfig.INSTANCE.getSilenceNonContacts());
            SettingsHelper.updateCheckState(view, FinegramCoreConfig.INSTANCE.getSilenceNonContacts());
        } else if (item.id == ignoreMentionsRow) {
            FinegramCoreConfig.INSTANCE.setIgnoreMentions(!FinegramCoreConfig.INSTANCE.getIgnoreMentions());
            SettingsHelper.updateCheckState(view, FinegramCoreConfig.INSTANCE.getIgnoreMentions());

            updateRows(true);
        } else if (item.id == ignoreMentionsExclusionsRow) {
            createUsersSelectActivity(view);
        } else if (item.id == ignoreMentionsAutoReadRow) {
            FinegramCoreConfig.INSTANCE.setIgnoreMentionsMarkAsRead(!FinegramCoreConfig.INSTANCE.getIgnoreMentionsMarkAsRead());
            SettingsHelper.updateCheckState(view, FinegramCoreConfig.INSTANCE.getIgnoreMentionsMarkAsRead());
        } else if (item.id == defaultNotificationIconRow) {
            FinegramCoreConfig.INSTANCE.setOldNotificationIcon(!FinegramCoreConfig.INSTANCE.getOldNotificationIcon());
            SettingsHelper.updateCheckState(view, FinegramCoreConfig.INSTANCE.getOldNotificationIcon());

            showRestartBulletin();
        } else if (item.id == residentNotificationRow) {
            FinegramCoreConfig.setResidentByUser(!FinegramCoreConfig.INSTANCE.getResidentNotification());
            SettingsHelper.updateCheckState(view, FinegramCoreConfig.INSTANCE.getResidentNotification());

            ApplicationLoader.applicationContext.stopService(new Intent(ApplicationLoader.applicationContext, NotificationsService.class));
            ApplicationLoader.startPushService();
            PushHealth.refreshPushConnection();
        } else if (item.id == hideStoriesRow) {
            FinegramCoreConfig.INSTANCE.setHideStories(!FinegramCoreConfig.INSTANCE.getHideStories());
            SettingsHelper.updateCheckState(view, FinegramCoreConfig.INSTANCE.getHideStories());

            showRestartBulletin();
        } else if (item.id == archiveStoriesRow) {
            expandedArchiveStoriesSection = !expandedArchiveStoriesSection;
            item.collapsed = !item.collapsed;

            updateRows(true);
        } else if (item.id == archiveStoriesUsersRow) {
            FinegramCoreConfig.INSTANCE.setArchiveStoriesFromUsers(!FinegramCoreConfig.INSTANCE.getArchiveStoriesFromUsers());
            SettingsHelper.updateCheckState(view, FinegramCoreConfig.INSTANCE.getArchiveStoriesFromUsers());

            updateRows(true);
        } else if (item.id == archiveStoriesChannelsRow) {
            FinegramCoreConfig.INSTANCE.setArchiveStoriesFromChannels(!FinegramCoreConfig.INSTANCE.getArchiveStoriesFromChannels());
            SettingsHelper.updateCheckState(view, FinegramCoreConfig.INSTANCE.getArchiveStoriesFromChannels());

            updateRows(true);
        } else if (item.id == useSystemEmojiRow) {
            FinegramCoreConfig.INSTANCE.setSystemEmoji(!FinegramCoreConfig.INSTANCE.getSystemEmoji());
            SettingsHelper.updateCheckState(view, FinegramCoreConfig.INSTANCE.getSystemEmoji());
        } else if (item.id == useSystemFontsRow) {
            FinegramCoreConfig.INSTANCE.setSystemFonts(!FinegramCoreConfig.INSTANCE.getSystemFonts());
            SettingsHelper.updateCheckState(view, FinegramCoreConfig.INSTANCE.getSystemFonts());

            showRestartBulletin();
        } else if (item.id == tabledModeRow) {
            showTabletModeSelector(() -> {
                SettingsHelper.updateButtonValue(view, getTabletModeValue());
                showRestartBulletin();
            });
        } else if (item.id == downloadSpeedBoostRow) {
            ArrayList<String> configStringKeys = new ArrayList<>();
            ArrayList<Integer> configValues = new ArrayList<>();

            configStringKeys.add(getString(R.string.LiteBatteryDisabled));
            configValues.add(FinegramCoreConfig.BOOST_NONE);

            configStringKeys.add(getString(R.string.LiteBatteryEnabled));
            configValues.add(FinegramCoreConfig.BOOST_AVERAGE);

            configStringKeys.add(getString(R.string.EP_DownloadSpeedBoostExtreme));
            configValues.add(FinegramCoreConfig.BOOST_EXTREME);

            PopupHelper.show(configStringKeys, getString(R.string.EP_DownloadSpeedBoost), configValues.indexOf(FinegramCoreConfig.INSTANCE.getDownloadSpeedBoost()), getContext(), i -> {
                FinegramCoreConfig.INSTANCE.setDownloadSpeedBoost(configValues.get(i));
                SettingsHelper.updateButtonValue(view, getDownloadSpeedBoostText());

                showRestartBulletin();
            });
        } else if (item.id == uploadSpeedBoostRow) {
            FinegramCoreConfig.INSTANCE.setUploadSpeedBoost(!FinegramCoreConfig.INSTANCE.getUploadSpeedBoost());
            SettingsHelper.updateCheckState(view, FinegramCoreConfig.INSTANCE.getUploadSpeedBoost());

            showRestartBulletin();
        } else if (item.id == slowNetworkMode) {
            FinegramCoreConfig.INSTANCE.setSlowNetworkMode(!FinegramCoreConfig.INSTANCE.getSlowNetworkMode());
            SettingsHelper.updateCheckState(view, FinegramCoreConfig.INSTANCE.getSlowNetworkMode());

            showRestartBulletin();
        }
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        return false;
    }

    private String getSpringValue()  {
        return switch (FinegramCoreConfig.INSTANCE.getSpringAnimation()) {
            case FinegramCoreConfig.ANIMATION_CLASSIC -> getString(R.string.EP_NavigationAnimationBezier);
            default -> getString(R.string.EP_NavigationAnimationSpring);
        };
    }

    private String getArchiveStoriesCountText() {
        int count = 0;

        if (FinegramCoreConfig.INSTANCE.getArchiveStoriesFromUsers()) count++;
        if (FinegramCoreConfig.INSTANCE.getArchiveStoriesFromChannels()) count++;

        return count + "/2";
    }

    private String getDownloadSpeedBoostText()  {
        return switch (FinegramCoreConfig.INSTANCE.getDownloadSpeedBoost()) {
            case FinegramCoreConfig.BOOST_NONE -> getString(R.string.LiteBatteryDisabled);
            case FinegramCoreConfig.BOOST_AVERAGE -> getString(R.string.LiteBatteryEnabled);
            default -> getString(R.string.EP_DownloadSpeedBoostExtreme);
        };
    }

    private void showTabletModeSelector(Runnable runnable) {
        ArrayList<String> configStringKeys = new ArrayList<>();
        ArrayList<Integer> configValues = new ArrayList<>();

        configStringKeys.add(getString(R.string.QualityAuto));
        configValues.add(FinegramCoreConfig.TABLET_MODE_AUTO);

        configStringKeys.add(getString(R.string.LiteBatteryEnabled));
        configValues.add(FinegramCoreConfig.TABLET_MODE_ENABLE);

        configStringKeys.add(getString(R.string.LiteBatteryDisabled));
        configValues.add(FinegramCoreConfig.TABLET_MODE_DISABLE);

        PopupHelper.show(configStringKeys, getString(R.string.AP_Tablet_Mode), configValues.indexOf(FinegramCoreConfig.INSTANCE.getTabletMode()), getContext(), i -> {
            FinegramCoreConfig.INSTANCE.setTabletMode(configValues.get(i));
            if (runnable != null) runnable.run();
        });
    }

    private String getTabletModeValue()  {
        return switch (FinegramCoreConfig.INSTANCE.getTabletMode()) {
            case FinegramCoreConfig.TABLET_MODE_ENABLE -> getString(R.string.LiteBatteryEnabled);
            case FinegramCoreConfig.TABLET_MODE_DISABLE -> getString(R.string.LiteBatteryDisabled);
            default -> getString(R.string.QualityAuto);
        };
    }

    private void createUsersSelectActivity(View view) {
        AndroidUtilities.runOnUIThread(() -> {
            UsersSelectActivity activity = getUsersSelectActivity();
            activity.setDelegate((ids, type) -> {
                Set<Long> chatIds = new HashSet<>(ids);

                Set<String> ignoredChats = new HashSet<>(getChatsNotificationHelper().getArrayList(getChatsNotificationHelper().getIgnoredArray()));

                FinegramLogger.d(() -> "old ignored chats array: " + ignoredChats);

                ignoredChats.clear();

                if (!chatIds.isEmpty()) {
                    for (Long id : chatIds) {
                        if (                                     DialogObject.isChatDialog(id)) {
                            ignoredChats.add(String.valueOf(id));
                        }
                    }
                }

                getChatsNotificationHelper().saveArrayList(
                        new ArrayList<>(ignoredChats),
                        getChatsNotificationHelper().getIgnoredArray()
                );

                FinegramLogger.d(() -> "new ignored chats array: " + ignoredChats);

                SettingsHelper.updateButtonValue(view, String.valueOf(getChatsNotificationHelper().getIgnoredChatsCount()));
            });

            presentFragment(activity);
        }, 300);
    }

    private UsersSelectActivity getUsersSelectActivity() {
        ArrayList<Long> chatsList = new ArrayList<>();
        ArrayList<String> ignoredChatsIds = getChatsNotificationHelper().getArrayList(getChatsNotificationHelper().getIgnoredArray());

        for (String chatIdStr : ignoredChatsIds) {
            long chatId = Long.parseLong(chatIdStr);

            TLRPC.Chat chat = getMessagesController().getChat(-chatId);

                     if (chat != null) {
                chatsList.add(-chat.id);
            }
        }

        UsersSelectActivity activity = new UsersSelectActivity(true, chatsList, 0);
        activity.asIgnoredChats();
        return activity;
    }

    private class NotificationReactionCell extends android.widget.FrameLayout {
        private final android.widget.TextView textView;
        private final AnimatedEmojiDrawable.SwapAnimatedEmojiDrawable emojiDrawable;

        NotificationReactionCell(Context context) {
            super(context);
            setBackground(Theme.getSelectorDrawable(false));
            textView = new android.widget.TextView(context);
            textView.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, 16);
            textView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteBlackText));
            textView.setText(getString(R.string.FG_NotificationReactionEmoji));
            addView(textView, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                    android.view.Gravity.CENTER_VERTICAL | android.view.Gravity.FILL_HORIZONTAL, 21, 0, 56, 0));
            emojiDrawable = new AnimatedEmojiDrawable.SwapAnimatedEmojiDrawable(this, AndroidUtilities.dp(24));
            setOnClickListener(v -> showNotificationReactionDialog(NotificationReactionCell.this));
        }

        void update(boolean animated) {
            String reaction = FinegramCoreConfig.notificationReaction(currentAccount);
            if (reaction.isEmpty()) {
                reaction = getMediaDataController().getDoubleTapReaction();
            }
            if (reaction != null && reaction.startsWith("animated_")) {
                try {
                    emojiDrawable.set(Long.parseLong(reaction.substring("animated_".length())), animated);
                    return;
                } catch (Exception ignore) {
                }
            }
            if (reaction != null) {
                TLRPC.TL_availableReaction available = getMediaDataController().getReactionsMap().get(reaction);
                if (available != null) {
                    emojiDrawable.set(available.static_icon, animated);
                    return;
                }
            }
            emojiDrawable.set((TLRPC.Document) null, animated);
        }

        @Override
        protected void dispatchDraw(android.graphics.Canvas canvas) {
            super.dispatchDraw(canvas);
            emojiDrawable.setBounds(
                    getWidth() - emojiDrawable.getIntrinsicWidth() - AndroidUtilities.dp(21),
                    (getHeight() - emojiDrawable.getIntrinsicHeight()) / 2,
                    getWidth() - AndroidUtilities.dp(21),
                    (getHeight() + emojiDrawable.getIntrinsicHeight()) / 2
            );
            emojiDrawable.draw(canvas);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            super.onMeasure(
                    MeasureSpec.makeMeasureSpec(MeasureSpec.getSize(widthMeasureSpec), MeasureSpec.EXACTLY),
                    MeasureSpec.makeMeasureSpec(AndroidUtilities.dp(50), MeasureSpec.EXACTLY)
            );
        }

        @Override
        protected void onDetachedFromWindow() {
            super.onDetachedFromWindow();
            emojiDrawable.detach();
        }

        @Override
        protected void onAttachedToWindow() {
            super.onAttachedToWindow();
            emojiDrawable.attach();
        }
    }

    private void showNotificationReactionDialog(View anchor) {
        if (selectReactionDialog != null) {
            return;
        }
        final SelectAnimatedEmojiDialog.SelectAnimatedEmojiDialogWindow[] popup =
                new SelectAnimatedEmojiDialog.SelectAnimatedEmojiDialogWindow[1];
        SelectAnimatedEmojiDialog layout = new SelectAnimatedEmojiDialog(this, getContext(), false, 0,
                SelectAnimatedEmojiDialog.TYPE_SET_DEFAULT_REACTION, null) {
            @Override
            protected void onEmojiSelected(View emojiView, Long documentId, TLRPC.Document document,
                                           TL_stars.TL_starGiftUnique gift, Integer until) {
                if (documentId == null) {
                    return;
                }

                if (!org.telegram.messenger.UserConfig.getInstance(currentAccount).isPremium()) {
                    if (popup[0] != null) {
                        selectReactionDialog = null;
                        popup[0].dismiss();
                    }
                    BulletinFactory.of(GeneralPreferencesEntry.this)
                            .createSimpleBulletin(R.raw.info, getString(R.string.FG_PremiumReactionRequired))
                            .show();
                    return;
                }
                FinegramCoreConfig.setNotificationReaction(currentAccount, "animated_" + documentId,
                        document != null ? org.telegram.messenger.MessageObject.getEmoji(document) : null);
                if (notificationReactionCell != null) {
                    notificationReactionCell.update(true);
                }
                if (popup[0] != null) {
                    selectReactionDialog = null;
                    popup[0].dismiss();
                }
            }

            @Override
            protected void onReactionClick(ImageViewEmoji emoji, ReactionsLayoutInBubble.VisibleReaction reaction) {
                FinegramCoreConfig.setNotificationReaction(currentAccount, reaction.emojicon, reaction.emojicon);
                if (notificationReactionCell != null) {
                    notificationReactionCell.update(true);
                }
                if (popup[0] != null) {
                    selectReactionDialog = null;
                    popup[0].dismiss();
                }
            }
        };
        String selected = FinegramCoreConfig.notificationReaction(currentAccount);
        if (selected.startsWith("animated_")) {
            try {
                layout.setSelected(Long.parseLong(selected.substring("animated_".length())));
            } catch (Exception ignored) {
            }
        }
        java.util.List<TLRPC.TL_availableReaction> available = getMediaDataController().getReactionsList();
        ArrayList<ReactionsLayoutInBubble.VisibleReaction> reactions = new ArrayList<>(available.size());
        for (int i = 0; i < available.size(); ++i) {
            ReactionsLayoutInBubble.VisibleReaction reaction = new ReactionsLayoutInBubble.VisibleReaction();
            reaction.emojicon = available.get(i).reaction;
            reactions.add(reaction);
        }
        layout.setRecentReactions(reactions);
        layout.setSaveState(3);
        popup[0] = selectReactionDialog = new SelectAnimatedEmojiDialog.SelectAnimatedEmojiDialogWindow(
                layout, LayoutHelper.WRAP_CONTENT, LayoutHelper.WRAP_CONTENT) {
            @Override
            public void dismiss() {
                super.dismiss();
                selectReactionDialog = null;
            }
        };
        popup[0].showAsDropDown(anchor, 0, -anchor.getMeasuredHeight() / 2,
                android.view.Gravity.TOP | android.view.Gravity.RIGHT);
        popup[0].dimBehind();
    }

    @Override
    protected String getKey() {
        return DeeplinkHelper.DeepLinksRepo.FG_General;
    }
}
