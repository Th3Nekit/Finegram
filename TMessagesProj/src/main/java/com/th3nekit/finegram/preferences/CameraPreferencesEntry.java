/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 * You should have received a copy of the license in this archive (see LICENSE).
 * Please, be respectful and credit the original author if you use this code.
 *
 * Copyright github.com/arsLan4k1390, 2022-2026.
 */

package com.th3nekit.finegram.preferences;

import com.th3nekit.finegram.core.helpers.DeeplinkHelper;

import static org.telegram.messenger.LocaleController.getString;

import static com.th3nekit.finegram.preferences.helpers.SettingsHelper.applyNewSpan;

import android.content.Context;
import android.util.Size;
import android.view.View;

import androidx.camera.video.Quality;

import org.telegram.messenger.MessagesController;
import org.telegram.messenger.R;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.Map;
import java.util.stream.Collectors;
import java.util.stream.Stream;

import com.th3nekit.finegram.camera.CameraTypeSelector;
import com.th3nekit.finegram.camera.CameraXUtils;
import com.th3nekit.finegram.core.configs.FinegramCameraConfig;
import com.th3nekit.finegram.core.configs.FinegramCoreConfig;
import com.th3nekit.finegram.helpers.ui.PopupHelper;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

public class CameraPreferencesEntry extends BaseCGPreferencesEntry {

    private final int disableAttachCameraRow = 1;
    private final int cameraAspectRatioRow = 2;

    private final int cameraUseDualCameraRow = 3;
    private final int rearCamRow = 4;
    private final int newRoundEngineRow = 1400;
    private final int manualCameraRow = 1401;
    private final int manualCameraFrontRow = 1402, manualCameraRearRow = 1403;
    private final int cameraAudioRow = 1404;
    private final int roundZoomScaleRow = 1405;
    private final int cameraTransitionsRow = 1406;
    private final int startFromUltraWideRow = 5;

    private final int cameraXQualityRow = 6;
    private final int cameraXFpsRangeRow = 7;

    private final int cameraEnhancementsRow = 8;
    private final int opticalStabilisationRow = 9;
    private final int videoStabilisationRow = 10;
    private final int continuousAutofocusRow = 11;
    private final int noiceReductionRow = 12;
    private final int faceDetectionRow = 13;
    private final int bokehEffectRow = 14;

    private final int exposureSliderRow = 15;
    private final int cameraControlButtonsRow = 16;

    private final int roundVideoBitrateRow = 17;
    private final int roundAudioBitrateRow = 18;
    private final int roundWatermarkRow = 19;
    private final int lensSwitcherRow = 20;

    private boolean expandedCameraEnhancementsSection = false;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.CP_Category_Camera);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        if (CameraXUtils.isCameraXSupported()) {
            items.add(UItem.asHeader(getString(R.string.CP_CameraType)));
            items.add(SettingsHelper.asCustomWithBackground(new CameraTypeSelector(getContext()) {
                @Override
                protected void onSelectedCamera(int cameraSelected) {
                    super.onSelectedCamera(cameraSelected);

                    FinegramCameraConfig.INSTANCE.setCameraType(cameraSelected);

                    runAfterGesture(() -> updateRows(false));
                }
            }));
            items.add(UItem.asShadow(null));
        }

        if (FinegramCameraConfig.INSTANCE.getCameraType() != FinegramCameraConfig.CAMERA_2 || FinegramCoreConfig.isDevBuild()) {
            items.add(UItem.asHeader(getString(R.string.CP_Category_Camera)));
        }
        if (FinegramCoreConfig.isDevBuild()) {
            items.add(SettingsHelper.asSwitchCG(disableAttachCameraRow, getString(R.string.CP_DisableCam), getString(R.string.CP_DisableCam_Desc))
                    .setChecked(FinegramCameraConfig.INSTANCE.getDisableAttachCamera())
            );
        }
        if (FinegramCameraConfig.INSTANCE.getCameraType() != FinegramCameraConfig.CAMERA_2) {
            items.add(UItem.asButton(cameraAspectRatioRow, getString(R.string.CP_CameraAspectRatio), getCameraAspectRatio()).slug("aspectRatio"));
        }
        if (CameraXUtils.isCurrentCameraCameraX()) {
            items.add(SettingsHelper.asSwitchCG(lensSwitcherRow, getString(R.string.CP_LensSwitcher), getString(R.string.CP_LensSwitcher_Desc))
                    .setChecked(FinegramCameraConfig.INSTANCE.getLensSwitcher())
            );
        }
        if (FinegramCameraConfig.INSTANCE.getCameraType() != FinegramCameraConfig.CAMERA_2 || FinegramCoreConfig.isDevBuild()) {
            items.add(UItem.asShadow(null));
        }

        items.add(UItem.asHeader(getString(R.string.CP_Header_Videomessages)));
        if (FinegramCameraConfig.INSTANCE.getCameraType() == FinegramCameraConfig.CAMERA_2 || CameraXUtils.isCurrentCameraCameraX()) {
            items.add(SettingsHelper.asSwitchCG(cameraUseDualCameraRow, applyNewSpan(getString(R.string.CP_CameraDualCamera)), getString(R.string.CP_CameraDualCamera_Desc))
                    .setChecked(FinegramCameraConfig.INSTANCE.getUseDualCamera())
            .slug("dualCamera"));
        }
        items.add(UItem.asButton(rearCamRow, getString(R.string.FG_RoundCamera), roundCameraName(
                FinegramCameraConfig.INSTANCE.getRoundStartCamera())).slug("rearCamera"));
        items.add(UItem.asButton(newRoundEngineRow, getString(R.string.FG_RoundEngine),
                getString(org.telegram.ui.Components.InstantCameraViewBase.isUsingCamera2Implementation()
                        ? R.string.FG_CameraModeNew : R.string.FG_CameraModeOld)));
        items.add(UItem.asButton(cameraAudioRow, getString(R.string.FG_CameraAudioMode),
                getString(FinegramCameraConfig.INSTANCE.getNewCameraAudio()
                        ? R.string.FG_CameraModeNew : R.string.FG_CameraModeOld)));
        items.add(UItem.asShadow(getString(R.string.FG_CameraAudioMode_Desc)));
        if (CameraXUtils.isCurrentCameraCameraX()
                && !org.telegram.ui.Components.InstantCameraViewBase.isUsingCamera2Implementation()) {
            items.add(SettingsHelper.asSwitchCG(roundZoomScaleRow, getString(R.string.FG_RoundZoomScale), null)
                    .setChecked(FinegramCameraConfig.INSTANCE.getRoundZoomScale()));
            items.add(SettingsHelper.asSwitchCG(cameraTransitionsRow, getString(R.string.FG_CameraTransitions), null)
                    .setChecked(FinegramCameraConfig.INSTANCE.getSmoothCameraModuleTransitions()));
        }
        items.add(SettingsHelper.asSwitchCG(manualCameraRow,
                        applyNewSpan(getString(R.string.FG_ManualCameraId)),
                        getString(R.string.FG_ManualCameraId_Desc))
                .setChecked(FinegramCameraConfig.INSTANCE.getManualCameraId())
        );
        if (FinegramCameraConfig.INSTANCE.getManualCameraId()) {
            items.add(UItem.asButton(manualCameraFrontRow,
                    getString(R.string.FG_ManualCameraId_Front),
                    describeCameraId(FinegramCameraConfig.INSTANCE.getManualCameraIdFront())));
            items.add(UItem.asButton(manualCameraRearRow,
                    getString(R.string.FG_ManualCameraId_Rear),
                    describeCameraId(FinegramCameraConfig.INSTANCE.getManualCameraIdRear())));
        }
        if (CameraXUtils.isCurrentCameraCameraX()) {
            items.add(SettingsHelper.asSwitchCG(startFromUltraWideRow, getString(R.string.CP_CameraUW), getString(R.string.CP_CameraUW_Desc))
                    .setChecked(FinegramCameraConfig.INSTANCE.getStartFromUltraWideCam())
            .slug("startFromUW"));
        }

        items.add(UItem.asButton(roundVideoBitrateRow, getString(R.string.FG_RoundVideoBitrate),
                FinegramCameraConfig.INSTANCE.getVideoMessagesBitrateKbps() + " kbps"));
        items.add(UItem.asButton(roundAudioBitrateRow, getString(R.string.FG_RoundAudioBitrate),
                FinegramCameraConfig.INSTANCE.getVideoMessagesAudioBitrateKbps() + " kbps"));
        items.add(UItem.asShadow(getString(R.string.FG_RoundBitrate_Desc)));

        items.add(SettingsHelper.asSwitchCG(roundWatermarkRow, getString(R.string.FG_RoundWatermark),
                        getString(R.string.FG_RoundWatermark_Desc))
                .setChecked(FinegramCameraConfig.INSTANCE.getVideoMessageWatermark())
        );

        if (CameraXUtils.isCurrentCameraCameraX() || FinegramCameraConfig.INSTANCE.getCameraType() == FinegramCameraConfig.CAMERA_2) {
            items.add(UItem.asShadow(null));
            if (CameraXUtils.isCurrentCameraCameraX()) {
                items.add(UItem.asButton(cameraXQualityRow, getString(R.string.CP_CameraQuality), FinegramCameraConfig.INSTANCE.getCameraResolution() + "p"));
            }
            items.add(UItem.asButton(cameraXFpsRangeRow, "FPS", getCameraXFpsRange()).slug("fps"));
            items.add(
                    SettingsHelper.asExpandableSwitch(
                            cameraEnhancementsRow,
                            R.drawable.magic_stick_solar,
                            getString(R.string.CP_CategoryEnhancements),
                            getArchiveStoriesCountText()
                    )
                    .setChecked(useCaptureRequestOptions())
                    .setCollapsed(!expandedCameraEnhancementsSection)
                    .setClickCallback(v -> {
                        boolean newValue = !(useCaptureRequestOptions());

                        FinegramCameraConfig.INSTANCE.setOpticalStabilisation(newValue);
                        FinegramCameraConfig.INSTANCE.setVideoStabilisation(newValue);
                        FinegramCameraConfig.INSTANCE.setContinuousAutofocus(newValue);
                        FinegramCameraConfig.INSTANCE.setNoiceReduction(newValue);
                        FinegramCameraConfig.INSTANCE.setFaceDetection(newValue);
                        FinegramCameraConfig.INSTANCE.setBokehEffect(newValue);

                        expandedCameraEnhancementsSection = !expandedCameraEnhancementsSection;
                        updateRows(true);
                    })
            );
            if (expandedCameraEnhancementsSection) {
                items.add(UItem.asRoundCheckbox(opticalStabilisationRow, getString(R.string.CP_OpticalStabilisation))
                        .setChecked(FinegramCameraConfig.INSTANCE.getOpticalStabilisation())
                        .setPad(1)
                );

                items.add(UItem.asRoundCheckbox(videoStabilisationRow, getString(R.string.CP_VideoStabilisation))
                        .setChecked(FinegramCameraConfig.INSTANCE.getVideoStabilisation())
                        .setPad(1)
                );

                items.add(UItem.asRoundCheckbox(continuousAutofocusRow, getString(R.string.CP_ContinuousAutofocus))
                        .setChecked(FinegramCameraConfig.INSTANCE.getContinuousAutofocus())
                        .setPad(1)
                );

                items.add(UItem.asRoundCheckbox(noiceReductionRow, getString(R.string.CP_NoiseReduction))
                        .setChecked(FinegramCameraConfig.INSTANCE.getNoiceReduction())
                        .setPad(1)
                );

                items.add(UItem.asRoundCheckbox(faceDetectionRow, getString(R.string.CP_FaceDetection))
                        .setChecked(FinegramCameraConfig.INSTANCE.getFaceDetection())
                        .setPad(1)
                );

                if (isBokehAvailable()) {
                    items.add(SettingsHelper.asRoundGroupCheckbox(bokehEffectRow, getString(R.string.CP_Bokeh), getString(R.string.CP_Bokeh_Desc))
                            .setChecked(FinegramCameraConfig.INSTANCE.getBokehEffect())
                    );
                }
            }
            items.add(UItem.asShadow(getString(R.string.CP_EnhancementsNote)));
        } else {
            items.add(UItem.asShadow(null));
        }

        if (CameraXUtils.isCurrentCameraCameraX()) {
            items.add(UItem.asButton(exposureSliderRow, getString(R.string.CP_ExposureSliderPosition), getExposureSliderPosition()).slug("exposureSlider"));
        }
        items.add(SettingsHelper.asSwitchCG(cameraControlButtonsRow, getString(R.string.CP_CenterCameraControlButtons), getString(R.string.CP_CenterCameraControlButtons_Desc))
                .setChecked(FinegramCameraConfig.INSTANCE.getCenterCameraControlButtons())
        .slug("centerControlButtons"));
        items.add(UItem.asShadow(null));
    }

    private void applyRoundBitrate() {
        MessagesController controller = MessagesController.getInstance(currentAccount);
        controller.roundVideoBitrate = FinegramCameraConfig.INSTANCE.getVideoMessagesBitrateKbps();
        controller.roundAudioBitrate = FinegramCameraConfig.INSTANCE.getVideoMessagesAudioBitrateKbps();
        MessagesController.getMainSettings(currentAccount).edit()
                .putInt("roundVideoBitrate", controller.roundVideoBitrate)
                .putInt("roundAudioBitrate", controller.roundAudioBitrate)
                .apply();
    }

    private java.util.List<String> deviceCameraIds(boolean front) {
        final java.util.ArrayList<String> out = new java.util.ArrayList<>();
        try {
            final android.hardware.camera2.CameraManager manager =
                    (android.hardware.camera2.CameraManager) org.telegram.messenger.ApplicationLoader
                            .applicationContext.getSystemService(android.content.Context.CAMERA_SERVICE);
            if (manager == null) return out;
            for (String id : manager.getCameraIdList()) {
                Integer facing = manager.getCameraCharacteristics(id).get(android.hardware.camera2.CameraCharacteristics.LENS_FACING);
                if (facing != null && facing == (front ? android.hardware.camera2.CameraCharacteristics.LENS_FACING_FRONT
                        : android.hardware.camera2.CameraCharacteristics.LENS_FACING_BACK)) out.add(id);
            }
        } catch (Throwable ignored) {
        }
        return out;
    }

    private String describeCameraId(String id) {
        return id == null || id.isEmpty() ? getString(R.string.FG_ManualCameraId_Auto) : id;
    }

    private void askCameraId(View view, boolean front) {
        final java.util.List<String> ids = deviceCameraIds(front);
        final java.util.ArrayList<String> labels = new java.util.ArrayList<>();
        labels.add(getString(R.string.FG_ManualCameraId_Auto));
        labels.addAll(ids);
        labels.add(getString(R.string.FG_CameraId_Enter));
        final String current = front
                ? FinegramCameraConfig.INSTANCE.getManualCameraIdFront()
                : FinegramCameraConfig.INSTANCE.getManualCameraIdRear();
        final int selected = current == null || current.isEmpty() ? 0 : ids.indexOf(current) + 1;
        PopupHelper.show(labels,
                getString(front ? R.string.FG_ManualCameraId_Front : R.string.FG_ManualCameraId_Rear),
                Math.max(0, selected), getContext(), i -> {
                    if (i == labels.size() - 1) {
                        enterCameraId(view, front, current);
                        return;
                    }
                    final String picked = i == 0 ? "" : ids.get(i - 1);
                    if (front) {
                        FinegramCameraConfig.INSTANCE.setManualCameraIdFront(picked);
                    } else {
                        FinegramCameraConfig.INSTANCE.setManualCameraIdRear(picked);
                    }
                    SettingsHelper.updateButtonValue(view, describeCameraId(picked));
                });
    }

    private void enterCameraId(View view, boolean front, String current) {
        org.telegram.ui.Components.EditTextBoldCursor input = new org.telegram.ui.Components.EditTextBoldCursor(getContext());
        input.setSingleLine(true);
        input.setTextSize(18);
        input.setTextColor(org.telegram.ui.ActionBar.Theme.getColor(org.telegram.ui.ActionBar.Theme.key_dialogTextBlack));
        input.setHintTextColor(org.telegram.ui.ActionBar.Theme.getColor(org.telegram.ui.ActionBar.Theme.key_dialogTextHint));
        input.setHint("ID");
        input.setText(current);
        input.setPadding(org.telegram.messenger.AndroidUtilities.dp(24), org.telegram.messenger.AndroidUtilities.dp(8),
                org.telegram.messenger.AndroidUtilities.dp(24), org.telegram.messenger.AndroidUtilities.dp(8));
        org.telegram.ui.ActionBar.AlertDialog dialog = new org.telegram.ui.ActionBar.AlertDialog.Builder(getContext())
                .setTitle(getString(front ? R.string.FG_ManualCameraId_Front : R.string.FG_ManualCameraId_Rear))
                .setView(input)
                .setNegativeButton(getString(R.string.Cancel), null)
                .setPositiveButton(getString(R.string.Save), null).create();
        showDialog(dialog);
        dialog.getButton(org.telegram.ui.ActionBar.AlertDialog.BUTTON_POSITIVE).setOnClickListener(v -> {
            String id = input.getText().toString().trim();
            if (!id.isEmpty()) {
                try {
                    android.hardware.camera2.CameraManager manager = (android.hardware.camera2.CameraManager)
                            getContext().getSystemService(Context.CAMERA_SERVICE);
                    Integer facing = manager.getCameraCharacteristics(id).get(android.hardware.camera2.CameraCharacteristics.LENS_FACING);
                    if (facing == null || facing != (front ? android.hardware.camera2.CameraCharacteristics.LENS_FACING_FRONT
                            : android.hardware.camera2.CameraCharacteristics.LENS_FACING_BACK)) throw new IllegalArgumentException();
                } catch (Exception error) {
                    input.setError(getString(R.string.FG_CameraId_Invalid));
                    return;
                }
            }
            if (front) FinegramCameraConfig.INSTANCE.setManualCameraIdFront(id);
            else FinegramCameraConfig.INSTANCE.setManualCameraIdRear(id);
            SettingsHelper.updateButtonValue(view, describeCameraId(id));
            dialog.dismiss();
        });
        input.requestFocus();
        org.telegram.messenger.AndroidUtilities.showKeyboard(input);
    }

    private String roundCameraName(int mode) {
        switch (mode) {
            case FinegramCameraConfig.ROUND_CAMERA_REAR:
                return getString(R.string.FG_RoundCamera_Rear);
            case FinegramCameraConfig.ROUND_CAMERA_LAST:
                return getString(R.string.FG_RoundCamera_Last);
            case FinegramCameraConfig.ROUND_CAMERA_ASK:
                return getString(R.string.FG_RoundCamera_Ask);
            default:
                return getString(R.string.FG_RoundCamera_Front);
        }
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == disableAttachCameraRow) {
            FinegramCameraConfig.INSTANCE.setDisableAttachCamera(!FinegramCameraConfig.INSTANCE.getDisableAttachCamera());
            SettingsHelper.updateCheckState(view, FinegramCameraConfig.INSTANCE.getDisableAttachCamera());

            showRestartBulletin();
        } else if (item.id == cameraAspectRatioRow) {
            showAspectRatioSelector(getContext(), () -> SettingsHelper.updateButtonValue(view, getCameraAspectRatio()));
        } else if (item.id == cameraUseDualCameraRow) {
            FinegramCameraConfig.INSTANCE.setUseDualCamera(!FinegramCameraConfig.INSTANCE.getUseDualCamera());
            SettingsHelper.updateCheckState(view, FinegramCameraConfig.INSTANCE.getUseDualCamera());

            if (FinegramCameraConfig.INSTANCE.getUseDualCamera()) {
                FinegramCameraConfig.INSTANCE.setBokehEffect(false);
                if (CameraXUtils.isCurrentCameraCameraX()) {
                    FinegramCameraConfig.INSTANCE.setCameraAspectRatio(FinegramCameraConfig.Camera4to3);
                }
            }

            updateRows(true);
        } else if (item.id == newRoundEngineRow) {
            PopupHelper.show(new java.util.ArrayList<>(java.util.Arrays.asList(getString(R.string.FG_CameraModeOld),
                    getString(R.string.FG_CameraModeNew))), getString(R.string.FG_RoundEngine),
                    org.telegram.ui.Components.InstantCameraViewBase.isUsingCamera2Implementation() ? 1 : 0,
                    getContext(), index -> {
                        org.telegram.ui.Components.InstantCameraViewBase.setUseCamera2Implementation(index == 1);
                        SettingsHelper.updateButtonValue(view, getString(index == 1
                                ? R.string.FG_CameraModeNew : R.string.FG_CameraModeOld));
                        updateRows(false);
                    });
        } else if (item.id == cameraAudioRow) {
            PopupHelper.show(new java.util.ArrayList<>(java.util.Arrays.asList(getString(R.string.FG_CameraModeOld),
                    getString(R.string.FG_CameraModeNew))), getString(R.string.FG_CameraAudioMode),
                    FinegramCameraConfig.INSTANCE.getNewCameraAudio() ? 1 : 0, getContext(), index -> {
                        FinegramCameraConfig.INSTANCE.setNewCameraAudio(index == 1);
                        SettingsHelper.updateButtonValue(view, getString(index == 1
                                ? R.string.FG_CameraModeNew : R.string.FG_CameraModeOld));
                    });
        } else if (item.id == roundZoomScaleRow) {
            FinegramCameraConfig.INSTANCE.setRoundZoomScale(!FinegramCameraConfig.INSTANCE.getRoundZoomScale());
            SettingsHelper.updateCheckState(view, FinegramCameraConfig.INSTANCE.getRoundZoomScale());
        } else if (item.id == cameraTransitionsRow) {
            FinegramCameraConfig.INSTANCE.setSmoothCameraModuleTransitions(!FinegramCameraConfig.INSTANCE.getSmoothCameraModuleTransitions());
            SettingsHelper.updateCheckState(view, FinegramCameraConfig.INSTANCE.getSmoothCameraModuleTransitions());
        } else if (item.id == manualCameraRow) {
            FinegramCameraConfig.INSTANCE.setManualCameraId(
                    !FinegramCameraConfig.INSTANCE.getManualCameraId());
            SettingsHelper.updateCheckState(view, FinegramCameraConfig.INSTANCE.getManualCameraId());
            updateRows(true);
        } else if (item.id == manualCameraFrontRow || item.id == manualCameraRearRow) {
            askCameraId(view, item.id == manualCameraFrontRow);
        } else if (item.id == rearCamRow) {
            final ArrayList<String> labels = new ArrayList<>();
            labels.add(roundCameraName(FinegramCameraConfig.ROUND_CAMERA_FRONT));
            labels.add(roundCameraName(FinegramCameraConfig.ROUND_CAMERA_REAR));
            labels.add(roundCameraName(FinegramCameraConfig.ROUND_CAMERA_LAST));
            labels.add(roundCameraName(FinegramCameraConfig.ROUND_CAMERA_ASK));
            PopupHelper.show(labels, getString(R.string.FG_RoundCamera), FinegramCameraConfig.INSTANCE.getRoundStartCamera(), getContext(), i -> {
                FinegramCameraConfig.INSTANCE.setRoundStartCamera(i);
                updateRows(false);
            });
        } else if (item.id == lensSwitcherRow) {
            FinegramCameraConfig.INSTANCE.setLensSwitcher(!FinegramCameraConfig.INSTANCE.getLensSwitcher());
            SettingsHelper.updateCheckState(view, FinegramCameraConfig.INSTANCE.getLensSwitcher());
        } else if (item.id == startFromUltraWideRow) {
            FinegramCameraConfig.INSTANCE.setStartFromUltraWideCam(!FinegramCameraConfig.INSTANCE.getStartFromUltraWideCam());
            SettingsHelper.updateCheckState(view, FinegramCameraConfig.INSTANCE.getStartFromUltraWideCam());
        } else if (item.id == cameraXQualityRow) {
            Map<Quality, Size> availableSizes = CameraXUtils.getAvailableVideoSizes();
            Stream<Integer> tmp = availableSizes.values().stream().sorted(Comparator.comparingInt(Size::getWidth).reversed()).map(Size::getHeight);
            ArrayList<Integer> types = tmp.collect(Collectors.toCollection(ArrayList::new));
            ArrayList<String> arrayList = types.stream().map(p -> p + "p").collect(Collectors.toCollection(ArrayList::new));

            PopupHelper.show(arrayList, getString(R.string.CP_CameraQuality), types.indexOf(FinegramCameraConfig.INSTANCE.getCameraResolution()), getContext(), i -> {
                FinegramCameraConfig.INSTANCE.setCameraResolution(types.get(i));
                SettingsHelper.updateButtonValue(view, FinegramCameraConfig.INSTANCE.getCameraResolution() + "p");
            });
        } else if (item.id == cameraXFpsRangeRow) {
            ArrayList<String> configStringKeys = new ArrayList<>();
            ArrayList<Integer> configValues = new ArrayList<>();

            configStringKeys.add("25-30");
            configValues.add(FinegramCameraConfig.CameraXFpsRange25to30);

            configStringKeys.add("30-30");
            configValues.add(FinegramCameraConfig.CameraXFpsRange30to30);

            configStringKeys.add("30-60");
            configValues.add(FinegramCameraConfig.CameraXFpsRange30to60);

            configStringKeys.add("60-60");
            configValues.add(FinegramCameraConfig.CameraXFpsRange60to60);

            configStringKeys.add(getString(R.string.Default));
            configValues.add(FinegramCameraConfig.CameraXFpsRangeDefault);

            PopupHelper.show(configStringKeys, "FPS", configValues.indexOf(FinegramCameraConfig.INSTANCE.getCameraXFpsRange()), getContext(), i -> {
                FinegramCameraConfig.INSTANCE.setCameraXFpsRange(configValues.get(i));
                SettingsHelper.updateButtonValue(view, getCameraXFpsRange());
            });
        } else if (item.id == cameraEnhancementsRow) {
            expandedCameraEnhancementsSection = !expandedCameraEnhancementsSection;
            item.collapsed = !item.collapsed;

            updateRows(true);
        } else if (item.id == opticalStabilisationRow) {
            FinegramCameraConfig.INSTANCE.setOpticalStabilisation(!FinegramCameraConfig.INSTANCE.getOpticalStabilisation());
            updateRows(true);
        } else if (item.id == videoStabilisationRow) {
            FinegramCameraConfig.INSTANCE.setVideoStabilisation(!FinegramCameraConfig.INSTANCE.getVideoStabilisation());
            updateRows(true);
        } else if (item.id == continuousAutofocusRow) {
            FinegramCameraConfig.INSTANCE.setContinuousAutofocus(!FinegramCameraConfig.INSTANCE.getContinuousAutofocus());
            updateRows(true);
        } else if (item.id == noiceReductionRow) {
            FinegramCameraConfig.INSTANCE.setNoiceReduction(!FinegramCameraConfig.INSTANCE.getNoiceReduction());
            updateRows(true);
        } else if (item.id == faceDetectionRow) {
            FinegramCameraConfig.INSTANCE.setFaceDetection(!FinegramCameraConfig.INSTANCE.getFaceDetection());
            updateRows(true);
        } else if (item.id == bokehEffectRow) {
            FinegramCameraConfig.INSTANCE.setBokehEffect(!FinegramCameraConfig.INSTANCE.getBokehEffect());
            updateRows(true);
        } else if (item.id == exposureSliderRow) {
            ArrayList<String> configStringKeys = new ArrayList<>();
            ArrayList<Integer> configValues = new ArrayList<>();

            configStringKeys.add(getString(R.string.CP_ZoomSliderPosition_Right));
            configValues.add(FinegramCameraConfig.EXPOSURE_SLIDER_RIGHT);

            configStringKeys.add(getString(R.string.Disable));
            configValues.add(FinegramCameraConfig.EXPOSURE_SLIDER_NONE);

            PopupHelper.show(configStringKeys, getString(R.string.CP_ExposureSliderPosition), configValues.indexOf(FinegramCameraConfig.INSTANCE.getExposureSlider()), getContext(), i -> {
                FinegramCameraConfig.INSTANCE.setExposureSlider(configValues.get(i));
                SettingsHelper.updateButtonValue(view, getExposureSliderPosition());
            });
        } else if (item.id == roundVideoBitrateRow) {
            ArrayList<String> labels = new ArrayList<>();
            ArrayList<Integer> values = new ArrayList<>();
            for (int kbps : new int[] {1000, 1500, 2200, 3000, 4000}) {
                labels.add(kbps + " kbps");
                values.add(kbps);
            }
            int current = values.indexOf(FinegramCameraConfig.INSTANCE.getVideoMessagesBitrateKbps());
            PopupHelper.show(labels, getString(R.string.FG_RoundVideoBitrate), current, getContext(), i -> {
                FinegramCameraConfig.INSTANCE.setVideoMessagesBitrateKbps(values.get(i));
                applyRoundBitrate();
                SettingsHelper.updateButtonValue(view, values.get(i) + " kbps");
            });
        } else if (item.id == roundWatermarkRow) {
            FinegramCameraConfig.INSTANCE.setVideoMessageWatermark(
                    !FinegramCameraConfig.INSTANCE.getVideoMessageWatermark());
            SettingsHelper.updateCheckState(view, FinegramCameraConfig.INSTANCE.getVideoMessageWatermark());
        } else if (item.id == roundAudioBitrateRow) {
            ArrayList<String> labels = new ArrayList<>();
            ArrayList<Integer> values = new ArrayList<>();
            for (int kbps : new int[] {32, 64, 96, 128, 192}) {
                labels.add(kbps + " kbps");
                values.add(kbps);
            }
            int current = values.indexOf(FinegramCameraConfig.INSTANCE.getVideoMessagesAudioBitrateKbps());
            PopupHelper.show(labels, getString(R.string.FG_RoundAudioBitrate), current, getContext(), i -> {
                FinegramCameraConfig.INSTANCE.setVideoMessagesAudioBitrateKbps(values.get(i));
                applyRoundBitrate();
                SettingsHelper.updateButtonValue(view, values.get(i) + " kbps");
            });
        } else if (item.id == cameraControlButtonsRow) {
            FinegramCameraConfig.INSTANCE.setCenterCameraControlButtons(!FinegramCameraConfig.INSTANCE.getCenterCameraControlButtons());
            SettingsHelper.updateCheckState(view, FinegramCameraConfig.INSTANCE.getCenterCameraControlButtons());
        }
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        return false;
    }

    private String getArchiveStoriesCountText() {
        int count = 0;

        if (FinegramCameraConfig.INSTANCE.getOpticalStabilisation()) count++;
        if (FinegramCameraConfig.INSTANCE.getVideoStabilisation()) count++;
        if (FinegramCameraConfig.INSTANCE.getContinuousAutofocus()) count++;
        if (FinegramCameraConfig.INSTANCE.getNoiceReduction()) count++;
        if (FinegramCameraConfig.INSTANCE.getFaceDetection()) count++;
        if (FinegramCameraConfig.INSTANCE.getBokehEffect() && isBokehAvailable()) count++;

        return count + (isBokehAvailable() ? "/6" : "/5");
    }

    private boolean useCaptureRequestOptions() {
        return FinegramCameraConfig.INSTANCE.getOpticalStabilisation() || FinegramCameraConfig.INSTANCE.getVideoStabilisation()
                || FinegramCameraConfig.INSTANCE.getContinuousAutofocus()
                || FinegramCameraConfig.INSTANCE.getNoiceReduction()
                || FinegramCameraConfig.INSTANCE.getFaceDetection()
                || (FinegramCameraConfig.INSTANCE.getBokehEffect() && isBokehAvailable());
    }

    public static void showAspectRatioSelector(Context context, Runnable runnable) {
        ArrayList<String> configStringKeys = new ArrayList<>();
        ArrayList<Integer> configValues = new ArrayList<>();

        configStringKeys.add("1:1");
        configValues.add(FinegramCameraConfig.Camera1to1);

        configStringKeys.add("4:3");
        configValues.add(FinegramCameraConfig.Camera4to3);

        configStringKeys.add("16:9");
        configValues.add(FinegramCameraConfig.Camera16to9);

        if (!FinegramCameraConfig.INSTANCE.getUseDualCamera() && CameraXUtils.isCurrentCameraCameraX()) {
            configStringKeys.add(getString(R.string.Default));
            configValues.add(FinegramCameraConfig.CameraAspectDefault);
        }

        PopupHelper.show(configStringKeys, getString(R.string.CP_CameraAspectRatio), configValues.indexOf(FinegramCameraConfig.INSTANCE.getCameraAspectRatio()), context, i -> {
            FinegramCameraConfig.INSTANCE.setCameraAspectRatio(configValues.get(i));
            if (runnable != null) runnable.run();
        });
    }

    private boolean isBokehAvailable() {
        return CameraXUtils.isCurrentCameraCameraX()
                && !FinegramCameraConfig.INSTANCE.getUseDualCamera();
    }

    public static String getCameraName() {
        return switch (FinegramCameraConfig.INSTANCE.getCameraType()) {
            case FinegramCameraConfig.TELEGRAM_CAMERA -> "Telegram";
            case FinegramCameraConfig.CAMERA_X -> "CameraX";
            case FinegramCameraConfig.CAMERA_2 -> "Camera 2 (Telegram)";
            default -> getString(R.string.CP_CameraTypeSystem);
        };
    }

    private static String getCameraAspectRatio()  {
        return switch (FinegramCameraConfig.INSTANCE.getCameraAspectRatio()) {
            case FinegramCameraConfig.Camera1to1 -> "1:1";
            case FinegramCameraConfig.Camera4to3 -> "4:3";
            case FinegramCameraConfig.Camera16to9 -> "16:9";
            default -> getString(R.string.Default);
        };
    }

    private String getCameraXFpsRange() {
        return switch (FinegramCameraConfig.INSTANCE.getCameraXFpsRange()) {
            case FinegramCameraConfig.CameraXFpsRange25to30 -> "25-30";
            case FinegramCameraConfig.CameraXFpsRange30to30 -> "30-30";
            case FinegramCameraConfig.CameraXFpsRange30to60 -> "30-60";
            case FinegramCameraConfig.CameraXFpsRange60to60 -> "60-60";
            default -> getString(R.string.Default);
        };
    }

    private String getExposureSliderPosition() {
        return switch (FinegramCameraConfig.INSTANCE.getExposureSlider()) {
            case FinegramCameraConfig.EXPOSURE_SLIDER_BOTTOM -> getString(R.string.CP_ZoomSliderPosition_Bottom);
            case FinegramCameraConfig.EXPOSURE_SLIDER_RIGHT -> getString(R.string.CP_ZoomSliderPosition_Right);
            case FinegramCameraConfig.EXPOSURE_SLIDER_LEFT -> getString(R.string.CP_ZoomSliderPosition_Left);
            default -> getString(R.string.Disable);
        };
    }

    @Override
    protected String getKey() {
        return DeeplinkHelper.DeepLinksRepo.FG_Camera;
    }
}
