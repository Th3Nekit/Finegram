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
import android.text.Html;
import android.text.InputFilter;
import android.text.InputType;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.TextUtils;
import android.view.View;
import android.view.inputmethod.EditorInfo;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.R;
import org.telegram.ui.Components.OutlineEditText;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;
import java.util.Locale;

import com.th3nekit.finegram.chats.gemini.network.ApiClient;
import com.th3nekit.finegram.chats.gemini.network.ModelInfo;
import com.th3nekit.finegram.core.configs.FinegramMessagesConfig;
import com.th3nekit.finegram.core.helpers.FGResourcesHelper;
import com.th3nekit.finegram.helpers.ui.PopupHelper;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

public class GeminiPreferencesEntry extends BaseCGPreferencesEntry {

    private final int geminiModelsListButton = 1;

    private OutlineEditText geminiApiKeyField;
    private OutlineEditText geminiModelNameField;
    private OutlineEditText geminiSystemPromptField;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.CP_GeminiAI_Header);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        items.add(UItem.asHeader(getString(R.string.CP_GeminiAI_API_Key)));
        geminiApiKeyField = new OutlineEditText(getContext(), getResourceProvider());
        geminiApiKeyField.getEditText().setInputType(InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD);
        geminiApiKeyField.getEditText().setImeOptions(EditorInfo.IME_ACTION_NEXT);
        geminiApiKeyField.getEditText().setOnEditorActionListener((v, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_NEXT){
                doOnDone();
            }
            return false;
        });
        geminiApiKeyField.setHint("\uD83D\uDD11");
        geminiApiKeyField.getEditText().setSingleLine(false);
        geminiApiKeyField.getEditText().setFilters(getInputFilter());
        if (!TextUtils.isEmpty(FinegramMessagesConfig.INSTANCE.getGeminiApiKey())) {
            geminiApiKeyField.getEditText().setText(FinegramMessagesConfig.INSTANCE.getGeminiApiKey());
        }
        geminiApiKeyField.setMinimumHeight(200);
        geminiApiKeyField.setPadding(dp(16), dp(12), dp(16), dp(12));
        geminiApiKeyField.getEditText().setPadding(dp(16), dp(12), dp(16), dp(12));
        items.add(SettingsHelper.asCustomWithBackground(geminiApiKeyField).slug("apiKeyField"));
        items.add(UItem.asShadow(getGeminiApiKeyAdvice()));

        items.add(UItem.asHeader(getString(R.string.CP_GeminiAI_Model)));
        geminiModelNameField = new OutlineEditText(getContext(), getResourceProvider());
        geminiModelNameField.getEditText().setInputType(InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD);
        geminiModelNameField.getEditText().setImeOptions(EditorInfo.IME_ACTION_NEXT);
        geminiModelNameField.getEditText().setOnEditorActionListener((v, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_NEXT){
                doOnDone();
            }
            return false;
        });
        geminiModelNameField.setHint("\uD83E\uDD16");
        geminiModelNameField.getEditText().setSingleLine(true);
        geminiModelNameField.getEditText().setFilters(getInputFilter());
        geminiModelNameField.getEditText().setHint(getString(R.string.CP_GeminiAI_Sample) + " gemini-1.5-flash");
        geminiModelNameField.getEditText().setText(FinegramMessagesConfig.INSTANCE.getGeminiModelName());
        geminiModelNameField.setMinimumHeight(200);
        geminiModelNameField.setPadding(dp(16), dp(12), dp(16), dp(12));
        geminiModelNameField.getEditText().setPadding(dp(16), dp(12), dp(16), dp(12));
        items.add(SettingsHelper.asCustomWithBackground(geminiModelNameField).slug("modelName"));

        items.add(
                SettingsHelper.asTextDetail(
                        geminiModelsListButton,
                        R.drawable.msg_list,
                        getString(R.string.CP_GeminiAI_Model_Selector),
                        getString(R.string.CP_GeminiAI_Model_Selector_Desc)
                )
        );
        items.add(UItem.asShadow(getGeminiModelNameAdvice()));

        items.add(UItem.asHeader(getString(R.string.CP_GeminiAI_System_Prompt)));
        geminiSystemPromptField = new OutlineEditText(getContext(), getResourceProvider());
        geminiSystemPromptField.getEditText().setInputType(InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD);
        geminiSystemPromptField.getEditText().setImeOptions(EditorInfo.IME_ACTION_DONE);
        geminiSystemPromptField.getEditText().setOnEditorActionListener((v, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_DONE){
                doOnDone();
            }
            return false;
        });
        geminiSystemPromptField.setHint("⚙️");
        geminiSystemPromptField.getEditText().setSingleLine(false);
        geminiSystemPromptField.getEditText().setHint(getString(R.string.CP_GeminiAI_System_Prompt));
        geminiSystemPromptField.getEditText().setText(FinegramMessagesConfig.INSTANCE.getGeminiSystemPrompt());
        geminiSystemPromptField.setMinimumHeight(200);
        geminiSystemPromptField.setPadding(dp(16), dp(12), dp(16), dp(12));
        geminiSystemPromptField.getEditText().setPadding(dp(16), dp(12), dp(16), dp(12));
        items.add(SettingsHelper.asCustomWithBackground(geminiSystemPromptField).slug("systemPrompt"));
        items.add(UItem.asShadow(getString(R.string.CP_GeminiAI_System_Prompt_Desc)));

        items.add(UItem.asHeader(getString(R.string.CP_GeminiAI_Temperature)));
        items.add(
                UItem.asIntSlideView(
                        1,
                        1,
                        FinegramMessagesConfig.INSTANCE.getGeminiTemperatureValue(),
                        10,
                        val -> String.format(Locale.US, "%.1f", val / 10f),
                        FinegramMessagesConfig.INSTANCE::setGeminiTemperatureValue
                )
        );
        items.add(UItem.asShadow(getString(R.string.CP_GeminiAI_Temperature_Desc)));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == geminiModelsListButton) {
            FinegramMessagesConfig.INSTANCE.setGeminiApiKey(
                    geminiApiKeyField.getEditText().getText().toString()
            );

            ApiClient.fetchModels(
                    getContext(),
                    getResourceProvider(),
                    FinegramMessagesConfig.INSTANCE.getGeminiApiKey(),
                    models -> AndroidUtilities.runOnUIThread(() -> {
                        if (models.isEmpty()) return;

                        ArrayList<String> modelShortTitleArray = new ArrayList<>();
                        ArrayList<String> modelFullTitleArray = new ArrayList<>();
                        ArrayList<String> modelDescriptionArray = new ArrayList<>();

                        for (ModelInfo model : models) {
                            modelShortTitleArray.add(model.name.replace("models/", ""));
                            modelFullTitleArray.add(model.displayName);
                            modelDescriptionArray.add(model.description);
                        }

                        PopupHelper.show(getString(R.string.CP_GeminiAI_Model), modelFullTitleArray, modelDescriptionArray, modelShortTitleArray.indexOf(FinegramMessagesConfig.INSTANCE.getGeminiModelName()), getContext(), i -> {
                            FinegramMessagesConfig.INSTANCE.setGeminiModelName(modelShortTitleArray.get(i));
                            geminiModelNameField.getEditText().setText(modelShortTitleArray.get(i));
                        }, getResourceProvider());
                    })
            );
        }
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        return false;
    }

    private InputFilter[] getInputFilter() {
        InputFilter filter = (source, start, end, dest, dstart, dend) -> {
            for (int i = start; i < end; i++) {
                if (Character.isWhitespace(source.charAt(i))) {
                    return "";
                }
            }
            return null;
        };
        return new InputFilter[] { filter };
    }

    private void doOnDone() {

        FinegramMessagesConfig.INSTANCE.setGeminiApiKey(
                geminiApiKeyField.getEditText().getText().toString()
        );

        FinegramMessagesConfig.INSTANCE.setGeminiModelName(
                geminiModelNameField.getEditText().getText().toString()
        );

        FinegramMessagesConfig.INSTANCE.setGeminiSystemPrompt(
                geminiSystemPromptField.getEditText().getText().toString()
        );
    }

    private CharSequence getGeminiApiKeyAdvice() {
        String advise = getString(R.string.CP_GeminiAI_API_Key_Desc);

        Spannable htmlParsed = new SpannableString(Html.fromHtml(advise, Html.FROM_HTML_MODE_LEGACY));

        return FGResourcesHelper.INSTANCE.getUrlNoUnderlineText(htmlParsed);
    }

    private CharSequence getGeminiModelNameAdvice() {
        String advise = getString(R.string.CP_GeminiAI_Model_Desc);

        Spannable htmlParsed = new SpannableString(Html.fromHtml(advise, Html.FROM_HTML_MODE_LEGACY));

        return FGResourcesHelper.INSTANCE.getUrlNoUnderlineText(htmlParsed);
    }

    @Override
    protected String getKey() {
        return DeeplinkHelper.DeepLinksRepo.FG_Gemini;
    }
}
