/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.badges;

import android.content.Context;
import android.widget.FrameLayout;

import com.exteragram.messenger.api.dto.BadgeDTO;
import com.exteragram.messenger.badges.source.ApiBadgeSource;

import org.telegram.messenger.UserConfig;
import org.telegram.tgnet.TLObject;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.BulletinFactory;

import java.util.HashMap;
import java.util.Map;
import java.util.function.Consumer;

public final class BadgesController {

    public static final BadgesController INSTANCE = new BadgesController();

    private static final ApiBadgeSource apiBadgeSource = new ApiBadgeSource();

    private final Map<Long, PluginBadge> pluginBadges = new HashMap<>();

    private BadgesController() {
    }

    public static BadgesController getInstance() {
        return INSTANCE;
    }

    public static final class PluginBadge {
        public final long emojiDocumentId;
        public final String text;

        PluginBadge(long emojiDocumentId, String text) {
            this.emojiDocumentId = emojiDocumentId;
            this.text = text;
        }
    }

    public void init(Context context) {
    }

    public void putPluginBadge(long userId, long emojiDocumentId, String text) {
        synchronized (pluginBadges) {
            pluginBadges.put(userId, new PluginBadge(emojiDocumentId, text));
        }
    }

    public PluginBadge getPluginBadge(long userId) {
        synchronized (pluginBadges) {
            return pluginBadges.get(userId);
        }
    }

    public BadgeDTO getPluginBadgeDto(long userId) {
        final PluginBadge badge = getPluginBadge(userId);
        return badge == null ? null : new BadgeDTO(badge.emojiDocumentId, badge.text);
    }

    private static long selfId() {
        return UserConfig.getInstance(UserConfig.selectedAccount).getClientUserId();
    }

    private static long idOf(TLObject object) {
        if (object instanceof TLRPC.User) {
            return ((TLRPC.User) object).id;
        }
        if (object instanceof TLRPC.Chat) {
            return -((TLRPC.Chat) object).id;
        }
        return 0;
    }

    public boolean hasBadge() {
        return getBadge() != null;
    }

    public boolean hasBadge(TLObject object) {
        return getBadge(object) != null;
    }

    public BadgeDTO getBadge() {
        return getPluginBadgeDto(selfId());
    }

    public BadgeDTO getBadge(TLObject object) {
        final long id = idOf(object);
        return id == 0 ? null : getPluginBadgeDto(id);
    }

    public BadgeDTO getDefaultBadge() {
        return null;
    }

    public BadgeDTO getDefaultBadge(TLRPC.User user) {
        return null;
    }

    public BadgeDTO getSecondaryBadge(TLRPC.User user) {
        return null;
    }

    public boolean shouldUseSecondaryBadgeSlot(TLRPC.User user, BadgeDTO badge) {
        return false;
    }

    public void updateBadge(BadgeDTO badge, Consumer<Boolean> onDone) {
        final long self = selfId();
        synchronized (pluginBadges) {
            if (badge == null) {
                pluginBadges.remove(self);
            } else {
                pluginBadges.put(self, new PluginBadge(badge.getDocumentId(), badge.getText()));
            }
        }
        if (onDone != null) {
            onDone.accept(true);
        }
    }

    public boolean canChangeBadge() {
        return canChangeBadge(null);
    }

    public boolean canChangeBadge(TLRPC.User user) {
        return false;
    }

    public boolean isDeveloper() {
        return false;
    }

    public boolean isDeveloper(TLRPC.User user) {
        return false;
    }

    public boolean isExtera(long dialogId) {
        return false;
    }

    public boolean isExtera(TLRPC.Chat chat) {
        return false;
    }

    public boolean isTrusted(long dialogId) {
        return false;
    }

    public void showBadgeBulletin(BaseFragment fragment, BadgeDTO badge, TLRPC.User user,
                                  Theme.ResourcesProvider resourcesProvider, int color) {
        showBadgeBulletin(fragment, badge, user, resourcesProvider, color, null, null);
    }

    public void showBadgeBulletin(BaseFragment fragment, BadgeDTO badge, TLRPC.User user,
                                  Theme.ResourcesProvider resourcesProvider, int color,
                                  FrameLayout container, Boolean top) {
        showText(fragment, badge);
    }

    public void showBadgeBulletin(BaseFragment fragment, BadgeDTO badge, TLRPC.Chat chat,
                                  Theme.ResourcesProvider resourcesProvider, int color) {
        showText(fragment, badge);
    }

    public void showBadgeBulletin(BaseFragment fragment, BadgeDTO badge, TLRPC.Chat chat,
                                  Theme.ResourcesProvider resourcesProvider, int color,
                                  FrameLayout container, Boolean top) {
        showText(fragment, badge);
    }

    public void showBadgeBulletin(BaseFragment fragment, TLRPC.User user,
                                  Theme.ResourcesProvider resourcesProvider, int color,
                                  FrameLayout container, Boolean top) {
        showText(fragment, getBadge(user));
    }

    private static void showText(BaseFragment fragment, BadgeDTO badge) {
        if (fragment == null || badge == null || badge.getText() == null || badge.getText().isEmpty()) {
            return;
        }
        BulletinFactory.of(fragment).createSimpleBulletin(org.telegram.messenger.R.raw.info, badge.getText()).show();
    }
}
