/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.badges.source;

import com.exteragram.messenger.api.dto.BadgeDTO;
import com.exteragram.messenger.badges.BadgesController;

public final class ApiBadgeSource {

    public ApiBadgeSource() {
    }

    public BadgeDTO getBadge(long userId, boolean ignored) {
        return BadgesController.INSTANCE.getPluginBadgeDto(userId);
    }

    public boolean canChangeBadge(long userId) {
        return false;
    }

    public boolean isDeveloper(long userId) {
        return false;
    }
}
