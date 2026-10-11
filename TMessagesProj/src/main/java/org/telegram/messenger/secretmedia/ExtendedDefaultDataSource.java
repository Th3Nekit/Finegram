/*
 * This is the source code of Telegram for Android v. 5.x.x.
 * It is licensed under GNU GPL v. 2 or later.
 * You should have received a copy of the license in this archive (see LICENSE).
 *
 * Copyright Nikolai Kudashov, 2013-2018.
 */

package org.telegram.messenger.secretmedia;

import android.content.Context;
import android.net.Uri;
import android.util.Log;
import android.util.LongSparseArray;

import androidx.annotation.Nullable;
import androidx.annotation.OptIn;
import androidx.media3.common.util.Assertions;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.common.util.Util;
import androidx.media3.datasource.AssetDataSource;
import androidx.media3.datasource.ContentDataSource;
import androidx.media3.datasource.DataSchemeDataSource;
import androidx.media3.datasource.DataSource;
import androidx.media3.datasource.DataSpec;
import androidx.media3.datasource.DefaultHttpDataSource;
import androidx.media3.datasource.FileDataSource;
import androidx.media3.datasource.RawResourceDataSource;
import androidx.media3.datasource.TransferListener;

import org.telegram.messenger.FileStreamLoadOperation;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.NavigableSet;
import java.util.Set;

@OptIn(markerClass = UnstableApi.class)
public final class ExtendedDefaultDataSource implements DataSource {

    private static final String TAG = "ExtendedDefaultDataSource";

    private static final String SCHEME_ASSET = "asset";
    private static final String SCHEME_CONTENT = "content";
    private static final String SCHEME_RTMP = "rtmp";
    private static final String SCHEME_RAW = RawResourceDataSource.RAW_RESOURCE_SCHEME;

    private final Context context;
    private final List<TransferListener> transferListeners;
    private final DataSource baseDataSource;

    private @Nullable
    DataSource fileDataSource;
    private @Nullable DataSource assetDataSource;
    private @Nullable DataSource contentDataSource;
    private @Nullable DataSource encryptedFileDataSource;
    private @Nullable DataSource rtmpDataSource;
    private @Nullable DataSource dataSchemeDataSource;
    private @Nullable DataSource rawResourceDataSource;

    private @Nullable DataSource dataSource;

    public ExtendedDefaultDataSource(Context context, String userAgent, boolean allowCrossProtocolRedirects) {
        this(
                context,
                userAgent,
                DefaultHttpDataSource.DEFAULT_CONNECT_TIMEOUT_MILLIS,
                DefaultHttpDataSource.DEFAULT_READ_TIMEOUT_MILLIS,
                allowCrossProtocolRedirects);
    }

    public ExtendedDefaultDataSource(
            Context context,
            String userAgent,
            int connectTimeoutMillis,
            int readTimeoutMillis,
            boolean allowCrossProtocolRedirects) {
        this(
            context,
            new DefaultHttpDataSource.Factory()
                .setUserAgent(userAgent)
                .setConnectTimeoutMs(connectTimeoutMillis)
                .setReadTimeoutMs(readTimeoutMillis)
                .setAllowCrossProtocolRedirects(allowCrossProtocolRedirects)
                .createDataSource(),
            null
        );
    }

    public ExtendedDefaultDataSource(Context context, DataSource baseDataSource, LongSparseArray<Uri> mtprotoUris) {
        this.context = context.getApplicationContext();
        this.baseDataSource = Assertions.checkNotNull(baseDataSource);
        transferListeners = new ArrayList<>();
        this.mtprotoUris = mtprotoUris;
    }

    private final LongSparseArray<Uri> mtprotoUris;

    @Deprecated
    public ExtendedDefaultDataSource(
            Context context, @Nullable TransferListener listener, DataSource baseDataSource, LongSparseArray<Uri> mtprotoUris) {
        this(context, baseDataSource, mtprotoUris);
        if (listener != null) {
            transferListeners.add(listener);
            baseDataSource.addTransferListener(listener);
        }
    }

    @Override
    public void addTransferListener(TransferListener transferListener) {
        baseDataSource.addTransferListener(transferListener);
        transferListeners.add(transferListener);
        maybeAddListenerToDataSource(fileDataSource, transferListener);
        maybeAddListenerToDataSource(assetDataSource, transferListener);
        maybeAddListenerToDataSource(contentDataSource, transferListener);
        maybeAddListenerToDataSource(rtmpDataSource, transferListener);
        maybeAddListenerToDataSource(dataSchemeDataSource, transferListener);
        maybeAddListenerToDataSource(rawResourceDataSource, transferListener);
    }

    @Override
    public long open(DataSpec dataSpec) throws IOException {
        Assertions.checkState(dataSource == null);
        Uri uri = dataSpec.uri;
        if ("mtproto".equals(uri.getScheme())) {
            final long docId = Long.parseLong(dataSpec.uri.toString().substring("mtproto:".length()));
            uri = mtprotoUris.get(docId);
            dataSpec = dataSpec.buildUpon().setUri(uri).build();
        }

        String scheme = uri.getScheme();
        if (Util.isLocalFileUri(uri)) {
            String uriPath = uri.getPath();
            if (uriPath != null && uriPath.startsWith("/android_asset/")) {
                dataSource = getAssetDataSource();
            } else {
                if (uri.getPath().endsWith(".enc")) {
                    dataSource = getEncryptedFileDataSource();
                } else {
                    dataSource = getFileDataSource();
                }
            }
        } else if ("tg".equals(scheme)) {
            dataSource = getStreamDataSource();
        } else if (SCHEME_ASSET.equals(scheme)) {
            dataSource = getAssetDataSource();
        } else if (SCHEME_CONTENT.equals(scheme)) {
            dataSource = getContentDataSource();
        } else if (SCHEME_RTMP.equals(scheme)) {
            dataSource = getRtmpDataSource();
        } else if (DataSchemeDataSource.SCHEME_DATA.equals(scheme)) {
            dataSource = getDataSchemeDataSource();
        } else if (SCHEME_RAW.equals(scheme)) {
            dataSource = getRawResourceDataSource();
        } else {
            dataSource = baseDataSource;
        }

        return dataSource.open(dataSpec);
    }

    @Override
    public int read(byte[] buffer, int offset, int readLength) throws IOException {
        return Assertions.checkNotNull(dataSource).read(buffer, offset, readLength);
    }

    @Override
    public @Nullable Uri getUri() {
        return dataSource == null ? null : dataSource.getUri();
    }

    @Override
    public Map<String, List<String>> getResponseHeaders() {
        return dataSource == null ? Collections.emptyMap() : dataSource.getResponseHeaders();
    }

    @Override
    public void close() throws IOException {
        if (dataSource != null) {
            try {
                dataSource.close();
            } finally {
                dataSource = null;
            }
        }
    }

    private DataSource getFileDataSource() {
        if (fileDataSource == null) {
            fileDataSource = new FileDataSource();
            addListenersToDataSource(fileDataSource);
        }
        return fileDataSource;
    }

    private DataSource getAssetDataSource() {
        if (assetDataSource == null) {
            assetDataSource = new AssetDataSource(context);
            addListenersToDataSource(assetDataSource);
        }
        return assetDataSource;
    }

    private DataSource getEncryptedFileDataSource() {
        if (encryptedFileDataSource == null) {
            encryptedFileDataSource = new EncryptedFileDataSource();
            addListenersToDataSource(encryptedFileDataSource);
        }
        return encryptedFileDataSource;
    }

    private FileStreamLoadOperation streamLoadOperation;
    private DataSource getStreamDataSource() {
        if (streamLoadOperation == null) {
            streamLoadOperation = new FileStreamLoadOperation();
            addListenersToDataSource(streamLoadOperation);
        }
        return streamLoadOperation;
    }

    private DataSource getContentDataSource() {
        if (contentDataSource == null) {
            contentDataSource = new ContentDataSource(context);
            addListenersToDataSource(contentDataSource);
        }
        return contentDataSource;
    }

    private DataSource getRtmpDataSource() {
        if (rtmpDataSource == null) {
            try {

                Class<?> clazz = Class.forName("com.google.android.exoplayer2.ext.rtmp.RtmpDataSource");
                rtmpDataSource = (DataSource) clazz.getConstructor().newInstance();

                addListenersToDataSource(rtmpDataSource);
            } catch (ClassNotFoundException e) {

                Log.w(TAG, "Attempting to play RTMP stream without depending on the RTMP extension");
            } catch (Exception e) {

                throw new RuntimeException("Error instantiating RTMP extension", e);
            }
            if (rtmpDataSource == null) {
                rtmpDataSource = baseDataSource;
            }
        }
        return rtmpDataSource;
    }

    private DataSource getDataSchemeDataSource() {
        if (dataSchemeDataSource == null) {
            dataSchemeDataSource = new DataSchemeDataSource();
            addListenersToDataSource(dataSchemeDataSource);
        }
        return dataSchemeDataSource;
    }

    private DataSource getRawResourceDataSource() {
        if (rawResourceDataSource == null) {
            rawResourceDataSource = new RawResourceDataSource(context);
            addListenersToDataSource(rawResourceDataSource);
        }
        return rawResourceDataSource;
    }

    private void addListenersToDataSource(DataSource dataSource) {
        for (int i = 0; i < transferListeners.size(); i++) {
            dataSource.addTransferListener(transferListeners.get(i));
        }
    }

    private void maybeAddListenerToDataSource(
            @Nullable DataSource dataSource, TransferListener listener) {
        if (dataSource != null) {
            dataSource.addTransferListener(listener);
        }
    }
}
