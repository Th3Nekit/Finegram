/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.preferences;

import com.th3nekit.finegram.core.helpers.DeeplinkHelper;

import static org.telegram.messenger.AndroidUtilities.dp;
import static org.telegram.messenger.LocaleController.getString;

import android.content.Context;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.View;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.DialogObject;
import org.telegram.messenger.R;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.ActionBar;
import org.telegram.ui.ActionBar.ActionBarMenuItem;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.Components.OutlineEditText;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;
import org.telegram.ui.UsersSelectActivity;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;

import com.th3nekit.finegram.chats.filters.MessagesFilterHelper;
import com.th3nekit.finegram.core.FinegramLogger;
import com.th3nekit.finegram.core.configs.FinegramMessagesConfig;
import com.th3nekit.finegram.helpers.ui.PopupHelper;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

public class MessageFiltersPreferencesEntry extends BaseCGPreferencesEntry {

    private final int enableFilterRow = 1;
    private final int detectTranslitRow = 3;
    private final int exactWordMatchRow = 4;
    private final int exclusionsRow = 5;
    private final int onlyInChatsRow = 51;

    private final int filterFromBlockedRow = 6;
    private final int detectEntitiesRow = 7;

    private final int hideAllRow = 9;
    private final int collapseAutomaticallyRow = 10;
    private final int makeTransparentRow = 11;

    private final int useRegexRow = 12;
    private final int filterLogicRow = 13;

    private OutlineEditText regexEditText;

    private OutlineEditText outlineEditText;

    private static final int done_button = 1;
    private ActionBarMenuItem doneButton;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.CP_Message_Filtering);
    }

    @Override
    public View createView(Context context) {
        actionBar.setActionBarMenuOnItemClick(new ActionBar.ActionBarMenuOnItemClick() {
            @Override
            public void onItemClick(int id) {
                if (id == -1) {
                    finishFragment();
                } else if (id == done_button) {
                    checkDone(true);
                }
            }
        });
        doneButton = actionBar.createMenu().addItemWithWidth(done_button, R.drawable.ic_ab_done, dp(56), getString(R.string.Done));

        return super.createView(context);
    }

    @Override
    public void onFragmentDestroy() {
        checkDone(true);
        super.onFragmentDestroy();
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {

        items.add(UItem.asHeader(getString(R.string.General)));
        items.add(SettingsHelper.asSwitchCG(enableFilterRow, getString(R.string.CP_Message_Filtering_Filter), getString(R.string.CP_Message_Filtering_Filter_Desc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getEnableMsgFilters())
        .slug("enableMessageFilter"));
        outlineEditText = new OutlineEditText(getContext(), getResourceProvider());
        outlineEditText.setPadding(dp(16), dp(12), dp(16), dp(12));
        outlineEditText.setEnabled(FinegramMessagesConfig.INSTANCE.getEnableMsgFilters(), null);
        outlineEditText.getEditText().setEnabled(FinegramMessagesConfig.INSTANCE.getEnableMsgFilters());
        outlineEditText.getEditText().addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) { }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable s) {
                checkDone(false);
            }
        });
        outlineEditText.getEditText().setSingleLine(false);
        outlineEditText.setHint(getString(R.string.CP_Message_Filtering_Field));
        outlineEditText.getEditText().setText(FinegramMessagesConfig.INSTANCE.getMsgFiltersElements());
        outlineEditText.setMinimumHeight(200);
        outlineEditText.getEditText().setPadding(dp(16), dp(12), dp(16), dp(12));
        items.add(SettingsHelper.asCustomWithBackground(outlineEditText));
        items.add(UItem.asShadow(getString(R.string.CP_Message_Filtering_Field_Desc)));

        items.add(SettingsHelper.asSwitchCG(useRegexRow, getString(R.string.FG_MF_UseRegex), getString(R.string.FG_MF_UseRegex_Desc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getMsgFiltersUseRegex())
                .setEnabled(FinegramMessagesConfig.INSTANCE.getEnableMsgFilters())
        );
        if (FinegramMessagesConfig.INSTANCE.getMsgFiltersUseRegex()) {
            regexEditText = new OutlineEditText(getContext(), getResourceProvider());
            regexEditText.setPadding(dp(16), dp(12), dp(16), dp(12));
            regexEditText.setEnabled(FinegramMessagesConfig.INSTANCE.getEnableMsgFilters(), null);
            regexEditText.getEditText().setEnabled(FinegramMessagesConfig.INSTANCE.getEnableMsgFilters());
            regexEditText.getEditText().addTextChangedListener(new TextWatcher() {
                @Override
                public void beforeTextChanged(CharSequence s, int start, int count, int after) { }

                @Override
                public void onTextChanged(CharSequence s, int start, int before, int count) { }

                @Override
                public void afterTextChanged(Editable s) {
                    checkDone(false);
                }
            });
            regexEditText.getEditText().setSingleLine(false);
            regexEditText.setHint(getString(R.string.FG_MF_RegexField));
            regexEditText.getEditText().setText(FinegramMessagesConfig.INSTANCE.getMsgFiltersRegexPatterns());
            regexEditText.setMinimumHeight(200);
            regexEditText.getEditText().setPadding(dp(16), dp(12), dp(16), dp(12));
            items.add(SettingsHelper.asCustomWithBackground(regexEditText));
            items.add(UItem.asShadow(getString(R.string.FG_MF_RegexField_Desc)));

            items.add(UItem.asButton(filterLogicRow, R.drawable.settings_folders, getString(R.string.FG_MF_Logic),
                    getFilterLogicValue()));
            items.add(UItem.asShadow(null));
        }

        items.add(SettingsHelper.asSwitchCG(detectTranslitRow, getString(R.string.CP_Message_Filtering_Translit), getString(R.string.CP_Message_Filtering_Translit_Desc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getMsgFiltersDetectTranslit())
                .setEnabled(FinegramMessagesConfig.INSTANCE.getEnableMsgFilters())
        .slug("translitDetection"));
        items.add(SettingsHelper.asSwitchCG(exactWordMatchRow, getString(R.string.CP_Message_Filtering_Exact_Words), getString(R.string.CP_Message_Filtering_Exact_Words_Desc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getMsgFiltersMatchExactWord())
                .setEnabled(FinegramMessagesConfig.INSTANCE.getEnableMsgFilters())
        .slug("exactWordMatch"));
        items.add(UItem.asButton(exclusionsRow, R.drawable._menu_stream_comments_off_24, getString(R.string.CP_Message_Filtering_Exclusions), String.valueOf(MessagesFilterHelper.INSTANCE.getExcludedChatsCount()))
                .setEnabled(FinegramMessagesConfig.INSTANCE.getEnableMsgFilters())
        .slug("exclusions"));
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.LocalMiscellaneousCache)));
        items.add(SettingsHelper.asSwitchCG(filterFromBlockedRow, getString(R.string.CP_Message_Filtering_FilterBlocked), getString(R.string.CP_Message_Filtering_FilterBlockedDesc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getMsgFiltersHideFromBlocked())
                .setEnabled(FinegramMessagesConfig.INSTANCE.getEnableMsgFilters())
        .slug("filterFromBlocked"));
        items.add(SettingsHelper.asSwitchCG(detectEntitiesRow, getString(R.string.CP_Message_Filtering_Entities), getString(R.string.CP_Message_Filtering_EntitiesDesc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getMsgFiltersDetectEntities())
                .setEnabled(FinegramMessagesConfig.INSTANCE.getEnableMsgFilters())
        .slug("filterEntities"));
        items.add(UItem.asButton(onlyInChatsRow, R.drawable.msg_folders, getString(R.string.FG_MsgFilters_OnlyInChats),
                String.valueOf(MessagesFilterHelper.INSTANCE.getWhitelistedChats(currentAccount).size())));
        items.add(UItem.asShadow(getString(R.string.FG_MsgFilters_OnlyInChats_Desc)));

        items.add(SettingsHelper.asSwitchCG(hideAllRow, getString(R.string.CP_Message_Filtering_HideAll), getString(R.string.CP_Message_Filtering_HideAllDesc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getMsgFiltersHideAllUnderSpoiler())
                .setEnabled(FinegramMessagesConfig.INSTANCE.getEnableMsgFilters())
        .slug("spoilAll"));
        items.add(SettingsHelper.asSwitchCG(collapseAutomaticallyRow, getString(R.string.CP_Message_Filtering_Collapse), getString(R.string.CP_Message_Filtering_Collapse_Desc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getMsgFiltersCollapseAutomatically())
                .setEnabled(
                        FinegramMessagesConfig.INSTANCE.getEnableMsgFilters() && (FinegramMessagesConfig.INSTANCE.getMsgFiltersHideFromBlocked() || FinegramMessagesConfig.INSTANCE.getMsgFiltersHideAllUnderSpoiler())
                )
        .slug("collapseMessages"));
        items.add(SettingsHelper.asSwitchCG(makeTransparentRow, getString(R.string.CP_Message_Filtering_Transparent), getString(R.string.CP_Message_Filtering_Transparent_Desc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getMsgFilterTransparentMsg())
                .setEnabled(FinegramMessagesConfig.INSTANCE.getEnableMsgFilters())
        .slug("transparency"));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {

        var holder = listView.findViewHolderForAdapterPosition(position);
        if (holder == null || !listView.adapter.isEnabled(holder)) {
            return;
        }
        if (item.id == enableFilterRow) {
            FinegramMessagesConfig.INSTANCE.setEnableMsgFilters(!FinegramMessagesConfig.INSTANCE.getEnableMsgFilters());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getEnableMsgFilters());

            updateRows(true);
        } else if (item.id == useRegexRow) {
            FinegramMessagesConfig.INSTANCE.setMsgFiltersUseRegex(!FinegramMessagesConfig.INSTANCE.getMsgFiltersUseRegex());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getMsgFiltersUseRegex());
            updateRows(true);
        } else if (item.id == filterLogicRow) {
            ArrayList<String> titles = new ArrayList<>();
            ArrayList<Integer> values = new ArrayList<>();
            titles.add(getString(R.string.FG_MF_Logic_Or));
            values.add(FinegramMessagesConfig.MSG_FILTERS_LOGIC_OR);
            titles.add(getString(R.string.FG_MF_Logic_And));
            values.add(FinegramMessagesConfig.MSG_FILTERS_LOGIC_AND);
            PopupHelper.show(titles, getString(R.string.FG_MF_Logic),
                    values.indexOf(FinegramMessagesConfig.INSTANCE.getMsgFiltersLogic()), getContext(), i -> {
                        FinegramMessagesConfig.INSTANCE.setMsgFiltersLogic(values.get(i));
                        SettingsHelper.updateButtonValue(view, getFilterLogicValue());
                    });
        } else if (item.id == detectTranslitRow) {
            FinegramMessagesConfig.INSTANCE.setMsgFiltersDetectTranslit(!FinegramMessagesConfig.INSTANCE.getMsgFiltersDetectTranslit());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getMsgFiltersDetectTranslit());
        } else if (item.id == exactWordMatchRow) {
            FinegramMessagesConfig.INSTANCE.setMsgFiltersMatchExactWord(!FinegramMessagesConfig.INSTANCE.getMsgFiltersMatchExactWord());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getMsgFiltersMatchExactWord());
        } else if (item.id == onlyInChatsRow) {
            selectWhitelistedChats(view);
        } else if (item.id == exclusionsRow) {
            AndroidUtilities.runOnUIThread(() -> {
                UsersSelectActivity activity = getUsersSelectActivity();
                activity.setDelegate((ids, unused) -> {
                    MessagesFilterHelper messagesFilterHelper = MessagesFilterHelper.INSTANCE;

                    Set<Long> chatIds = new HashSet<>(ids);
                    Set<String> excludedChats = new HashSet<>(messagesFilterHelper.getArrayList(messagesFilterHelper.getExcludedList()));

                    FinegramLogger.d(() -> "old excluded chats array: " + excludedChats);
                    excludedChats.clear();

                    if (!chatIds.isEmpty()) {
                        for (Long id : chatIds) {
                            if (DialogObject.isUserDialog(id) || DialogObject.isChatDialog(id)) {
                                excludedChats.add(String.valueOf(id));
                            }
                        }
                    }

                    messagesFilterHelper.saveArrayList(new ArrayList<>(excludedChats), messagesFilterHelper.getExcludedList());
                    FinegramLogger.d(() -> "new excluded chats array: " + excludedChats);

                    SettingsHelper.updateButtonValue(view, String.valueOf(MessagesFilterHelper.INSTANCE.getExcludedChatsCount()));
                });
                presentFragment(activity);
            }, 300);
        } else if (item.id == filterFromBlockedRow) {
            FinegramMessagesConfig.INSTANCE.setMsgFiltersHideFromBlocked(!FinegramMessagesConfig.INSTANCE.getMsgFiltersHideFromBlocked());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getMsgFiltersHideFromBlocked());
        } else if (item.id == detectEntitiesRow) {
            FinegramMessagesConfig.INSTANCE.setMsgFiltersDetectEntities(!FinegramMessagesConfig.INSTANCE.getMsgFiltersDetectEntities());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getMsgFiltersDetectEntities());
        } else if (item.id == hideAllRow) {
            FinegramMessagesConfig.INSTANCE.setMsgFiltersHideAllUnderSpoiler(!FinegramMessagesConfig.INSTANCE.getMsgFiltersHideAllUnderSpoiler());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getMsgFiltersHideAllUnderSpoiler());
        } else if (item.id == collapseAutomaticallyRow) {
            FinegramMessagesConfig.INSTANCE.setMsgFiltersCollapseAutomatically(!FinegramMessagesConfig.INSTANCE.getMsgFiltersCollapseAutomatically());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getMsgFiltersCollapseAutomatically());
        } else if (item.id == makeTransparentRow) {
            FinegramMessagesConfig.INSTANCE.setMsgFilterTransparentMsg(!FinegramMessagesConfig.INSTANCE.getMsgFilterTransparentMsg());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getMsgFilterTransparentMsg());
        }
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        return false;
    }

    private String getFilterLogicValue() {
        return getString(FinegramMessagesConfig.INSTANCE.getMsgFiltersLogic() == FinegramMessagesConfig.MSG_FILTERS_LOGIC_AND
                ? R.string.FG_MF_Logic_And : R.string.FG_MF_Logic_Or);
    }

    private boolean hasChanges() {
        if (regexEditText != null
                && !TextUtils.equals(FinegramMessagesConfig.INSTANCE.getMsgFiltersRegexPatterns(),
                        regexEditText.getEditText().getText().toString())) {
            return true;
        }
        return !TextUtils.equals(FinegramMessagesConfig.INSTANCE.getMsgFiltersElements(),
                outlineEditText.getEditText().getText().toString());
    }

    private void checkDone(boolean finish) {
        if (doneButton == null || outlineEditText == null) return;

        if (finish && hasChanges()) {
            doOnDone(this);
        }

        boolean changed = hasChanges();

        doneButton.setEnabled(changed);

        doneButton.animate()
                .alpha(changed ? 1.0f : 0.0f)
                .scaleX(changed ? 1.0f : 0.0f)
                .scaleY(changed ? 1.0f : 0.0f)
                .setDuration(180)
                .start();
    }

    private void doOnDone(BaseFragment fragment) {
        if (fragment == null || fragment.getParentActivity() == null) {
            return;
        }

        FinegramMessagesConfig.INSTANCE.setMsgFiltersElements(
                outlineEditText.getEditText().getText().toString()
        );
        if (regexEditText != null) {
            FinegramMessagesConfig.INSTANCE.setMsgFiltersRegexPatterns(
                    regexEditText.getEditText().getText().toString()
            );
        }

        AndroidUtilities.runOnUIThread(() -> AndroidUtilities.hideKeyboard(listView), 50);

        outlineEditText.getEditText().clearFocus();

        if (FinegramMessagesConfig.INSTANCE.getMsgFiltersHideFromBlocked()) {
            getMessagesController().getBlockedPeers(false);
        }
    }

    private UsersSelectActivity getUsersSelectActivity() {
        MessagesFilterHelper messagesFilterHelper = MessagesFilterHelper.INSTANCE;

        ArrayList<Long> chatsList = new ArrayList<>();
        ArrayList<String> savedChats = messagesFilterHelper.getArrayList(messagesFilterHelper.getExcludedList());

        for (String chatIdStr : savedChats) {
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
        activity.asFilterExcludedChats();
        return activity;
    }

    private void selectWhitelistedChats(View view) {
        AndroidUtilities.runOnUIThread(() -> {
            ArrayList<Long> current = MessagesFilterHelper.INSTANCE.getWhitelistedChats(currentAccount);
            UsersSelectActivity activity = new UsersSelectActivity(true, current, 0);
            activity.asLockedChats();
            activity.setDelegate((ids, unused) -> {
                Set<Long> chatIds = new HashSet<>();
                for (Long id : ids) {
                    if (DialogObject.isUserDialog(id) || DialogObject.isChatDialog(id)) {
                        chatIds.add(id);
                    }
                }
                MessagesFilterHelper.INSTANCE.setWhitelistedChats(currentAccount, chatIds);
                SettingsHelper.updateButtonValue(view, String.valueOf(chatIds.size()));
            });
            presentFragment(activity);
        }, 300);
    }

    @Override
    protected String getKey() {
        return DeeplinkHelper.DeepLinksRepo.FG_Message_Filters;
    }
}