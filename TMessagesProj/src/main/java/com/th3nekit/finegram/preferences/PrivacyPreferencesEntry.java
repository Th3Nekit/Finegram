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

import org.telegram.messenger.LocaleController;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.provider.Settings;
import android.view.View;

import androidx.biometric.BiometricPrompt;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.DialogObject;
import org.telegram.messenger.R;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;
import org.telegram.ui.UsersSelectActivity;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;

import com.th3nekit.finegram.core.FGBiometricPrompt;
import com.th3nekit.finegram.core.FinegramLogger;
import com.th3nekit.finegram.core.configs.FinegramPrivacyConfig;
import com.th3nekit.finegram.core.helpers.AppRestartHelper;
import com.th3nekit.finegram.helpers.ui.PopupHelper;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

public class PrivacyPreferencesEntry extends BaseCGPreferencesEntry {

    private final int hideArchiveFromChatsListRow = 1;
    private final int askBiometricsToOpenDialogsRow = 2;
    private final int askBiometricsToOpenChatsRow = 3;
    private final int askBiometricsToOpenSecretChatsRow = 4;
    private final int askBiometricsToOpenArchivedChatsRow = 5;

    private final int lockedChatsRow = 6;
    private final int lockedChatsRememberRow = 61;
    private final int requireBiometricsToDeleteChatsRow = 7;
    private final int allowSystemPinRow = 8;
    private final int testFingerprintRow = 9;

    private final int deleteAccountRow = 11;

    private final int hideArchivedStoriesRow = 1390;

    private boolean expandedBiometricSection = false;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.SettingsPrivacySecurity);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {

        items.add(UItem.asHeader(getString(R.string.FilterChats)));
        items.add(SettingsHelper.asSwitchCG(hideArchivedStoriesRow, getString(R.string.SP_HideArchivedStories), getString(R.string.SP_HideArchivedStories_Desc))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getHideArchivedStories())
        );
        items.add(SettingsHelper.asSwitchCG(hideArchiveFromChatsListRow, getString(R.string.SP_HideArchive), getString(R.string.SP_HideArchive_Desc))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getHideArchiveFromChatsList())
        .slug("hideArchive"));
        if (getChatsPasswordHelper().checkBiometricAvailable()) {
            items.add(UItem.asShadow(null));
            items.add(
                    SettingsHelper.asExpandableSwitch(
                            askBiometricsToOpenDialogsRow,
                            R.drawable.msg_pin_code,
                            getString(R.string.Passcode),
                            getBiometricCountText()
                    )
                    .setChecked(
                            FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenChat() ||
                            FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenEncrypted() ||
                            FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenArchive()
                    )
                    .setCollapsed(!expandedBiometricSection)
                    .setClickCallback(v -> FGBiometricPrompt.prompt(getParentActivity(), () -> {
                        boolean newValue = !(
                                FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenChat() ||
                                FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenEncrypted() ||
                                FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenArchive()
                        );

                        FinegramPrivacyConfig.INSTANCE.setAskBiometricsToOpenChat(newValue);
                        FinegramPrivacyConfig.INSTANCE.setAskBiometricsToOpenEncrypted(newValue);
                        FinegramPrivacyConfig.INSTANCE.setAskBiometricsToOpenArchive(newValue);

                        expandedBiometricSection = !expandedBiometricSection;
                        updateRows(true);
                    }))
            .slug("passcodeLock"));
            if (expandedBiometricSection) {
                items.add(UItem.asRoundCheckbox(askBiometricsToOpenChatsRow, getString(R.string.FilterChats))
                        .setChecked(FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenChat())
                        .setPad(1)
                );

                items.add(UItem.asRoundCheckbox(askBiometricsToOpenSecretChatsRow, getString(R.string.SecretChat))
                        .setChecked(FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenEncrypted())
                        .setPad(1)
                );

                items.add(UItem.asRoundCheckbox(askBiometricsToOpenArchivedChatsRow, getString(R.string.ArchivedChats))
                        .setChecked(FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenArchive())
                        .setPad(1)
                );
            }

            items.add(UItem.asShadow(getString(R.string.SP_AskBioToOpenChats_Desc)));

            if (FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenChat()) {
                items.add(UItem.asButton(lockedChatsRow, R.drawable.msg_discussion, getString(R.string.SP_LockedChats), String.valueOf(getChatsPasswordHelper().getLockedChatsCount())));
            }
            items.add(UItem.asButton(lockedChatsRememberRow, R.drawable.msg_recent, getString(R.string.FG_LockedChatsRemember), rememberValueTitle()));
            items.add(SettingsHelper.asSwitchCG(requireBiometricsToDeleteChatsRow, getString(R.string.SP_AskPinBeforeDelete), getString(R.string.SP_AskPinBeforeDelete_Desc))
                    .setChecked(FinegramPrivacyConfig.INSTANCE.getAskPasscodeBeforeDelete())
            .slug("askPasscodeToDelete"));
            items.add(SettingsHelper.asSwitchCG(allowSystemPinRow, getString(R.string.SP_AllowUseSystemPasscode), getString(R.string.SP_AllowUseSystemPasscode_Desc))
                    .setChecked(FinegramPrivacyConfig.INSTANCE.getAllowSystemPasscode())
            .slug("useSystemPin"));
        }
        items.add(UItem.asButton(testFingerprintRow, R.drawable.fingerprint, getString(R.string.SP_TestFingerprint)));
        items.add(UItem.asShadow(getString(R.string.SP_TestFingerprint_Desc)));

        items.add(UItem.asHeader(getString(R.string.LocalMiscellaneousCache)));
        UItem deleteAccountButton = UItem.asButton(
                deleteAccountRow,
                R.drawable.msg_delete,
                getString(R.string.SP_DeleteAccount)
        );
        deleteAccountButton.red = true;
        items.add(deleteAccountButton);
        items.add(UItem.asShadow(null));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == hideArchivedStoriesRow) {
            FinegramPrivacyConfig.INSTANCE.setHideArchivedStories(!FinegramPrivacyConfig.INSTANCE.getHideArchivedStories());
            SettingsHelper.updateCheckState(view, FinegramPrivacyConfig.INSTANCE.getHideArchivedStories());

            showRestartBulletin();
        } else if (item.id == hideArchiveFromChatsListRow) {
            FinegramPrivacyConfig.INSTANCE.setHideArchiveFromChatsList(!FinegramPrivacyConfig.INSTANCE.getHideArchiveFromChatsList());
            SettingsHelper.updateCheckState(view, FinegramPrivacyConfig.INSTANCE.getHideArchiveFromChatsList());
        } else if (item.id == askBiometricsToOpenDialogsRow) {
            expandedBiometricSection = !expandedBiometricSection;
            item.collapsed = !item.collapsed;

            updateRows(true);
        } else if (item.id == askBiometricsToOpenChatsRow) {
            FGBiometricPrompt.prompt(getParentActivity(), () -> {
                FinegramPrivacyConfig.INSTANCE.setAskBiometricsToOpenChat(!FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenChat());
                SettingsHelper.updateCheckState(view, FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenChat());

                updateRows(true);
            });
        } else if (item.id == askBiometricsToOpenSecretChatsRow) {
            FGBiometricPrompt.prompt(getParentActivity(), () -> {
                FinegramPrivacyConfig.INSTANCE.setAskBiometricsToOpenEncrypted(!FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenEncrypted());
                SettingsHelper.updateCheckState(view, FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenEncrypted());

                updateRows(true);
            });
        } else if (item.id == askBiometricsToOpenArchivedChatsRow) {
            FGBiometricPrompt.prompt(getParentActivity(), () -> {
                FinegramPrivacyConfig.INSTANCE.setAskBiometricsToOpenArchive(!FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenArchive());
                SettingsHelper.updateCheckState(view, FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenArchive());

                updateRows(true);
            });
        } else if (item.id == lockedChatsRow) {
            FGBiometricPrompt.prompt(getParentActivity(), () -> createUsersSelectActivity(view));
        } else if (item.id == lockedChatsRememberRow) {
            showRememberSelector(view);
        } else if (item.id == requireBiometricsToDeleteChatsRow) {
            FinegramPrivacyConfig.INSTANCE.setAskPasscodeBeforeDelete(!FinegramPrivacyConfig.INSTANCE.getAskPasscodeBeforeDelete());
            SettingsHelper.updateCheckState(view, FinegramPrivacyConfig.INSTANCE.getAskPasscodeBeforeDelete());
        } else if (item.id == allowSystemPinRow) {
            FinegramPrivacyConfig.INSTANCE.setAllowSystemPasscode(!FinegramPrivacyConfig.INSTANCE.getAllowSystemPasscode());
            SettingsHelper.updateCheckState(view, FinegramPrivacyConfig.INSTANCE.getAllowSystemPasscode());
        } else if (item.id == testFingerprintRow) {
            testFingerprint();
        } else if (item.id == deleteAccountRow) {
            if (getChatsPasswordHelper().checkBiometricAvailable()) {
                FGBiometricPrompt.prompt(getParentActivity(), () -> DeleteAccountDialog.showDeleteAccountDialog(this));
            } else {
                DeleteAccountDialog.showDeleteAccountDialog(this);
            }
        }
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        return false;
    }

    private String getBiometricCountText() {
        int count = 0;

        if (FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenChat()) count++;
        if (FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenEncrypted()) count++;
        if (FinegramPrivacyConfig.INSTANCE.getAskBiometricsToOpenArchive()) count++;

        return count + "/3";
    }

    private void createUsersSelectActivity(View view) {
        AndroidUtilities.runOnUIThread(() -> {
            UsersSelectActivity activity = getUsersSelectActivity();
            activity.setDelegate((ids, type) -> {
                Set<Long> chatIds = new HashSet<>(ids);

                Set<String> lockedChats = new HashSet<>(getChatsPasswordHelper().getArrayList(getChatsPasswordHelper().getPasscodeArray()));

                FinegramLogger.d(() -> "old locked chats array: " + lockedChats);

                lockedChats.clear();

                if (!chatIds.isEmpty()) {
                    for (Long id : chatIds) {
                        if (DialogObject.isUserDialog(id) || DialogObject.isChatDialog(id)) {
                            lockedChats.add(String.valueOf(id));
                        }
                    }
                }

                getChatsPasswordHelper().saveArrayList(
                        new ArrayList<>(lockedChats),
                        getChatsPasswordHelper().getPasscodeArray()
                );

                FinegramLogger.d(() -> "new locked chats array: " + lockedChats);

                SettingsHelper.updateButtonValue(view, String.valueOf(getChatsPasswordHelper().getLockedChatsCount()));
            });

            presentFragment(activity);
        }, 300);
    }

    private UsersSelectActivity getUsersSelectActivity() {
        ArrayList<Long> chatsList = new ArrayList<>();
        ArrayList<String> lockedChatIds = getChatsPasswordHelper().getArrayList(getChatsPasswordHelper().getPasscodeArray());

        for (String chatIdStr : lockedChatIds) {
            long chatId = Long.parseLong(chatIdStr);

            TLRPC.User user = getMessagesController().getUser(chatId);
            TLRPC.Chat chat = getMessagesController().getChat(-chatId);

            if (user != null) {
                chatsList.add(user.id);
            } else if (chat != null) {
                chatsList.add(-chat.id);
            }
        }

        UsersSelectActivity activity = new UsersSelectActivity(true, chatsList, 0);
        activity.asLockedChats();
        return activity;
    }

    private void testFingerprint() {
        FGBiometricPrompt.fixFingerprint(getParentActivity(), new FGBiometricPrompt.FGBiometricListener() {
            @Override
            public void onSuccess(BiometricPrompt.AuthenticationResult result) {
                handle();
            }

            @Override
            public void onFailed() {

            }

            @Override
            public void onError(int error, CharSequence msg) {
                showError(error);
            }

            private void handle() {
                FGBiometricPrompt.cancelPendingAuthentications();
                FGBiometricPrompt.reloadFingerprintState();

                if (listView != null && listView.adapter != null) updateRows(true);

                if (FGBiometricPrompt.hasFingerprintCached()) {
                    AndroidUtilities.runOnUIThread(() ->
                            BulletinFactory.of(PrivacyPreferencesEntry.this)
                                    .createSimpleBulletin(
                                            R.raw.chats_infotip,
                                            getString(R.string.SP_BiometricUnavailable_Test_Fixed),
                                            getString(R.string.FG_RestartToApply),
                                            getString(R.string.OK),
                                            () -> AppRestartHelper.restartApp(getContext())
                                    ).show(),
                            300
                    );
                } else {
                    showError(0);
                }
            }

            private void showError(int error) {
                String title = getString(R.string.FG_AppCrashed) + (error == 0 ? "" : " (e" + error + ")");

                BulletinFactory.of(PrivacyPreferencesEntry.this).createSimpleBulletin(
                        R.raw.chats_infotip,
                        title,
                        getString(R.string.SP_BiometricUnavailable_Test_Wrong_Desc),
                        getString(R.string.Settings),
                        () -> openFingerprintSettings(getContext())
                ).show();
            }

            private void openFingerprintSettings(Context context) {
                Intent fallbackIntent = new Intent(Settings.ACTION_SECURITY_SETTINGS);

                try {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                        Intent fingerprintIntent = new Intent(Settings.ACTION_FINGERPRINT_ENROLL);
                        fingerprintIntent.setPackage("com.android.settings");

                        if (fingerprintIntent.resolveActivity(context.getPackageManager()) != null) {
                            context.startActivity(fingerprintIntent);
                            return;
                        }
                    }
                    context.startActivity(fallbackIntent);
                } catch (SecurityException e) {
                    FinegramLogger.e(e);
                    context.startActivity(fallbackIntent);
                } catch (Exception e) {
                    FinegramLogger.e(e);
                }
            }
        });
    }

    private static final int[] REMEMBER_VALUES = {0, 60, 300, 900, Integer.MAX_VALUE / 1000};

    private CharSequence rememberValueTitle() {
        int value = FinegramPrivacyConfig.INSTANCE.getLockedChatsRememberSeconds();
        if (value <= 0) return getString(R.string.FG_LockedChatsRemember_Always);
        if (value >= REMEMBER_VALUES[4]) return getString(R.string.FG_LockedChatsRemember_Restart);
        if (value <= 60) return getString(R.string.FG_LockedChatsRemember_1Min);
        if (value <= 300) return getString(R.string.FG_LockedChatsRemember_5Min);
        return getString(R.string.FG_LockedChatsRemember_15Min);
    }

    private void showRememberSelector(View view) {
        ArrayList<String> titles = new ArrayList<>();
        titles.add(getString(R.string.FG_LockedChatsRemember_Always));
        titles.add(getString(R.string.FG_LockedChatsRemember_1Min));
        titles.add(getString(R.string.FG_LockedChatsRemember_5Min));
        titles.add(getString(R.string.FG_LockedChatsRemember_15Min));
        titles.add(getString(R.string.FG_LockedChatsRemember_Restart));

        PopupHelper.show(titles, getString(R.string.FG_LockedChatsRemember), currentRememberIndex(), getContext(), i -> {
            FinegramPrivacyConfig.INSTANCE.setLockedChatsRememberSeconds(REMEMBER_VALUES[i]);
            updateRows(true);
        }, getResourceProvider());
    }

    private int currentRememberIndex() {
        int value = FinegramPrivacyConfig.INSTANCE.getLockedChatsRememberSeconds();
        for (int i = 0; i < REMEMBER_VALUES.length; i++) {
            if (REMEMBER_VALUES[i] == value) return i;
        }
        return 0;
    }

    @Override
    protected String getKey() {
        return DeeplinkHelper.DeepLinksRepo.FG_Privacy;
    }
}
