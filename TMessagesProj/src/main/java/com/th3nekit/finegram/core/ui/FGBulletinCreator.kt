/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.ui

import android.content.Context
import android.os.Build
import androidx.annotation.RequiresApi
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.ApplicationLoader
import org.telegram.messenger.ContactsController
import org.telegram.messenger.LocaleController.formatString
import org.telegram.messenger.LocaleController.getString
import org.telegram.messenger.PasskeysController
import org.telegram.messenger.R
import org.telegram.messenger.UserConfig
import org.telegram.tgnet.TLObject
import org.telegram.tgnet.TLRPC
import org.telegram.ui.ActionBar.AlertDialog
import org.telegram.ui.ActionBar.BaseFragment
import org.telegram.ui.Components.Bulletin
import org.telegram.ui.Components.BulletinFactory
import org.telegram.ui.LaunchActivity
import com.th3nekit.finegram.core.helpers.AppRestartHelper

object FGBulletinCreator {

    fun createRestartBulletin(fragment: BaseFragment) {
        BulletinFactory.of(fragment).createSimpleBulletin(
            R.raw.chats_infotip,
            getString(R.string.FG_RestartToApply),
            getString(R.string.BotUnblock)
        ) {
            AppRestartHelper.restartApp(fragment.context)
        }.show()
    }

    fun createDebugSuccessBulletin(fragment: BaseFragment) {
        BulletinFactory.of(fragment)
            .createSuccessBulletin(getString(R.string.YourPasswordSuccess))
            .setDuration(Bulletin.DURATION_LONG)
            .show()
    }

    fun createSwitchAccountBulletin(account: Int) {
        val nextAcc = UserConfig.getInstance(account).currentUser

        if (nextAcc is TLRPC.User) {
            AndroidUtilities.runOnUIThread({

                val activity = LaunchActivity.instance
                if (activity == null || activity.isFinishing || activity.isDestroyed) {
                    return@runOnUIThread
                }

                val accs = ArrayList<TLObject?>()
                accs.add(nextAcc)

                val text = AndroidUtilities.replaceTags(
                    formatString(
                        R.string.FG_SwitchedToAccount,
                        ContactsController.formatName(nextAcc.first_name, nextAcc.last_name)
                    )
                )

                BulletinFactory.global()
                    .createChatsBulletin(accs, text, null)
                    .setDuration(Bulletin.DURATION_SHORT)
                    .show()

                accs.clear()
            }, 200)
        }
    }

    @RequiresApi(Build.VERSION_CODES.P)
    fun createPasskeyBulletin(fragment: BaseFragment) {
        BulletinFactory.of(fragment).createSimpleBulletin(
            R.raw.passkey,
            getString(R.string.FG_PasskeyNoCredentialAvailable),
            getString(R.string.Settings)
        ) {
            PasskeysController.openSettings(fragment.getParentActivity())
        }.show()
    }

}
