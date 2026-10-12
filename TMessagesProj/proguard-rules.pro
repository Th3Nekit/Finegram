-keep public class com.google.android.gms.* { public *; }
-keepnames @com.google.android.gms.common.annotation.KeepName class *
-keepclassmembernames class * {
    @com.google.android.gms.common.annotation.KeepName *;
}
-keep class org.webrtc.* { *; }
-keep class org.webrtc.audio.* { *; }
-keep class org.webrtc.voiceengine.* { *; }
-keep class org.telegram.tgnet.RequestTimeDelegate { *; }
-keep class org.telegram.tgnet.RequestDelegate { *; }
-keep class androidx.media3.** { *; }


-keep class com.google.android.exoplayer2.decoder.SimpleDecoderOutputBuffer { *; }
-keep class org.telegram.ui.Stories.recorder.FfmpegAudioWaveformLoader { *; }
-keep class androidx.mediarouter.app.MediaRouteButton { *; }
-keepclassmembers class ** {
    @android.webkit.JavascriptInterface <methods>;
}

-keep class com.google.mlkit.nl.languageid.internal.LanguageIdentificationJni { *; }

-keep class com.google.android.exoplayer2.upstream.RawResourceDataSource {
  public static android.net.Uri buildRawResourceUri(int);
}

-dontnote com.google.android.exoplayer2.ext.flac.FlacLibrary
-keepclassmembers class com.google.android.exoplayer2.ext.flac.FlacLibrary {

}

-keep class com.google.android.exoplayer2.decoder.VideoDecoderOutputBuffer {
  *;
}

-dontnote com.google.android.exoplayer2.ext.opus.LibopusAudioRenderer
-keepclassmembers class com.google.android.exoplayer2.ext.opus.LibopusAudioRenderer {
  <init>(android.os.Handler, com.google.android.exoplayer2.audio.AudioRendererEventListener, com.google.android.exoplayer2.audio.AudioProcessor[]);
}
-dontnote com.google.android.exoplayer2.ext.flac.LibflacAudioRenderer
-keepclassmembers class com.google.android.exoplayer2.ext.flac.LibflacAudioRenderer {
  <init>(android.os.Handler, com.google.android.exoplayer2.audio.AudioRendererEventListener, com.google.android.exoplayer2.audio.AudioProcessor[]);
}
-dontnote com.google.android.exoplayer2.ext.ffmpeg.FfmpegAudioRenderer
-keepclassmembers class com.google.android.exoplayer2.ext.ffmpeg.FfmpegAudioRenderer {
  <init>(android.os.Handler, com.google.android.exoplayer2.audio.AudioRendererEventListener, com.google.android.exoplayer2.audio.AudioProcessor[]);
}

-dontnote com.google.android.exoplayer2.ext.flac.FlacExtractor
-keepclassmembers class com.google.android.exoplayer2.ext.flac.FlacExtractor {
  <init>();
}

-dontnote com.google.android.exoplayer2.source.dash.offline.DashDownloader
-keepclassmembers class com.google.android.exoplayer2.source.dash.offline.DashDownloader {
  <init>(android.net.Uri, java.util.List, com.google.android.exoplayer2.offline.DownloaderConstructorHelper);
}
-dontnote com.google.android.exoplayer2.source.hls.offline.HlsDownloader
-keepclassmembers class com.google.android.exoplayer2.source.hls.offline.HlsDownloader {
  <init>(android.net.Uri, java.util.List, com.google.android.exoplayer2.offline.DownloaderConstructorHelper);
}
-dontnote com.google.android.exoplayer2.source.smoothstreaming.offline.SsDownloader
-keepclassmembers class com.google.android.exoplayer2.source.smoothstreaming.offline.SsDownloader {
  <init>(android.net.Uri, java.util.List, com.google.android.exoplayer2.offline.DownloaderConstructorHelper);
}

-dontnote com.google.android.exoplayer2.source.dash.DashMediaSource$Factory
-keepclasseswithmembers class com.google.android.exoplayer2.source.dash.DashMediaSource$Factory {
  <init>(com.google.android.exoplayer2.upstream.DataSource$Factory);
}
-dontnote com.google.android.exoplayer2.source.hls.HlsMediaSource$Factory
-keepclasseswithmembers class com.google.android.exoplayer2.source.hls.HlsMediaSource$Factory {
  <init>(com.google.android.exoplayer2.upstream.DataSource$Factory);
}
-dontnote com.google.android.exoplayer2.source.smoothstreaming.SsMediaSource$Factory
-keepclasseswithmembers class com.google.android.exoplayer2.source.smoothstreaming.SsMediaSource$Factory {
  <init>(com.google.android.exoplayer2.upstream.DataSource$Factory);
}

-keep class com.huawei.hianalytics.**{ *; }
-keep class com.huawei.updatesdk.**{ *; }
-keep class com.huawei.hms.**{ *; }

-keepclassmembers class com.google.common.util.concurrent.AbstractFuture** {
  *** waiters;
  *** value;
  *** listeners;
  *** thread;
  *** next;
}

-keep,allowshrinking,allowobfuscation class com.google.common.util.concurrent.AbstractFuture** {
  <fields>;
}

-keepclasseswithmembers,allowobfuscation class * {
  @com.google.gson.annotations.SerializedName <fields>;
}

-keepattributes Signature, *Annotation*, EnclosingMethod, InnerClasses

-keep class org.telegram.messenger.voip.* { *; }
-keep class org.telegram.messenger.AnimatedFileDrawableStream { <methods>; }
-keep class org.telegram.SQLite.SQLiteException { <methods>; }
-keep class org.telegram.tgnet.ConnectionsManager { <methods>; }
-keep class org.telegram.tgnet.NativeByteBuffer { <methods>; }
-keepnames class ** extends org.telegram.ui.ActionBar.BaseFragment
-keepnames class org.telegram.tgnet.TLRPC$TL_* {}
-keepclassmembernames,allowshrinking class org.telegram.ui.* { <fields>; }
-keepclassmembernames,allowshrinking class org.telegram.ui.Cells.* { <fields>; }
-keepclassmembernames,allowshrinking class org.telegram.ui.Components.* { <fields>; }

-keep class com.fasterxml.jackson.**{ *; }

-keepclassmembers class org.telegram.tgnet.** {
    <fields>;
}

-keep class com.th3nekit.finegram.core.configs.CherrygramCoreConfig { *; }
-keepnames class com.th3nekit.finegram.chats.gemini.GeminiErrorDTO.* { <fields>; }
-keep class com.th3nekit.finegram.chats.gemini.GeminiErrorDTO.** {*;}
-keep class com.th3nekit.finegram.chats.gemini.GeminiErrorDTO$ErrorResponse {*;}
-keep class com.th3nekit.finegram.chats.gemini.GeminiErrorDTO$ErrorDetail {*;}
-keep class com.th3nekit.finegram.chats.gemini.GeminiErrorDTO$ErrorDetails{*;}
-keep class com.th3nekit.finegram.chats.gemini.GeminiErrorDTO$Metadata {*;}

-keep class androidx.camera.extensions.** { *; }
-keep class androidx.camera.camera2.internal.** { *; }
-keep class androidx.camera.camera2.interop.** { *; }
-keep class androidx.camera.core.** { *; }
-keep class androidx.camera.core.impl.** { *; }
-keep class androidx.camera.video.** { *; }

-keep class com.th3nekit.finegram.donates.adsgram.** { *; }

-keepclassmembernames class androidx.core.widget.NestedScrollView {
    private android.widget.OverScroller mScroller;
    private void abortAnimatedScroll();
}

-keepclasseswithmembernames,includedescriptorclasses class * {
    native <methods>;
}
-keepclassmembers class * {
    @android.webkit.JavascriptInterface <methods>;
}

-assumenosideeffects class kotlin.jvm.internal.Intrinsics {
    public static void check*(...);
    public static void throw*(...);
}

-keepclassmembers enum * {
     public static **[] values();
     public static ** valueOf(java.lang.String);
}

-keepnames class androidx.recyclerview.widget.RecyclerView
-keepclassmembers class androidx.recyclerview.widget.RecyclerView {
    public void suppressLayout(boolean);
    public boolean isLayoutSuppressed();
}

-dontwarn org.checkerframework.**
-dontwarn javax.annotation.**

-keep class io.nano.tex.** {*;}

-keep class org.telegram.tgnet.** { *; }

-keep class org.scilab.forge.jlatexmath.** { *; }
-keep class ru.noties.jlatexmath.** { *; }
-dontwarn org.scilab.forge.jlatexmath.**

-repackageclasses
-allowaccessmodification
-keepattributes SourceFile,LineNumberTable
-renamesourcefileattribute SourceFile
-dontoptimize
-classobfuscationdictionary ../TMessagesProj_AppStandalone/build/tmp/dictionary/class_obfuscation_dictionary.txt
-obfuscationdictionary ../TMessagesProj_AppStandalone/build/tmp/dictionary/field_obfuscation_dictionary.txt
-packageobfuscationdictionary ../TMessagesProj_AppStandalone/build/tmp/dictionary/package_obfuscation_dictionary.txt

-dontwarn com.google.j2objc.annotations.ReflectionSupport
-dontwarn com.google.j2objc.annotations.RetainedWith
-dontwarn com.google.j2objc.annotations.Weak
-dontwarn android.support.annotation.IntRange
-dontwarn android.support.annotation.NonNull
-dontwarn android.support.annotation.Nullable
-dontwarn android.support.annotation.RequiresApi
-dontwarn android.support.annotation.Size
-dontwarn android.support.annotation.VisibleForTesting
-dontwarn android.support.v4.app.NotificationCompat$Builder
-dontwarn androidx.camera.extensions.**
-dontwarn javax.script.**
-dontwarn java.beans.ConstructorProperties
-dontwarn java.beans.Transient
-dontwarn com.google.android.gms.auth.api.R$drawable
-dontwarn com.google.firebase.messaging.R$drawable
-dontwarn java.lang.invoke.StringConcatFactory

-dontwarn a.a.a.a.a.a
-dontwarn android.telephony.HwTelephonyManager
-dontwarn com.huawei.android.os.BuildEx$VERSION
-dontwarn com.huawei.android.telephony.ServiceStateEx
-dontwarn com.huawei.hianalytics.process.HiAnalyticsConfig$Builder
-dontwarn com.huawei.hianalytics.process.HiAnalyticsConfig
-dontwarn com.huawei.hianalytics.process.HiAnalyticsInstance$Builder
-dontwarn com.huawei.hianalytics.process.HiAnalyticsInstance
-dontwarn com.huawei.hianalytics.process.HiAnalyticsManager
-dontwarn com.huawei.hianalytics.util.HiAnalyticTools
-dontwarn com.huawei.hms.commonkit.config.Config
-dontwarn com.huawei.hms.config.Server
-dontwarn com.huawei.hms.maps.auth.AuthClient
-dontwarn com.huawei.hms.maps.provider.inhuawei.IDistanceCalculatorDelegate
-dontwarn com.huawei.hms.maps.provider.inhuawei.IHuaweiMapDelegate
-dontwarn com.huawei.hms.maps.provider.inhuawei.MapFragmentDelegate
-dontwarn com.huawei.hms.maps.provider.inhuawei.MapViewDelegate
-dontwarn com.huawei.hms.network.NetworkKit$Callback
-dontwarn com.huawei.hms.network.NetworkKit
-dontwarn com.huawei.hms.network.httpclient.HttpClient$Builder
-dontwarn com.huawei.hms.network.httpclient.HttpClient
-dontwarn com.huawei.hms.network.httpclient.Response
-dontwarn com.huawei.hms.network.httpclient.ResponseBody
-dontwarn com.huawei.hms.network.httpclient.Submit
-dontwarn com.huawei.hms.network.restclient.RestClient$Builder
-dontwarn com.huawei.hms.network.restclient.RestClient
-dontwarn com.huawei.hms.network.restclient.anno.Body
-dontwarn com.huawei.hms.network.restclient.anno.GET
-dontwarn com.huawei.hms.network.restclient.anno.HeaderMap
-dontwarn com.huawei.hms.network.restclient.anno.POST
-dontwarn com.huawei.hms.network.restclient.anno.Url
-dontwarn com.huawei.hms.tss.inner.TssCallback
-dontwarn com.huawei.hms.tss.inner.TssInnerAPI
-dontwarn com.huawei.hms.tss.inner.TssInnerClient
-dontwarn com.huawei.hms.tss.inner.entity.GetCertificationKeyReq
-dontwarn com.huawei.hms.tss.inner.entity.GetCertifiedCredentialReq
-dontwarn com.huawei.libcore.io.ExternalStorageFile
-dontwarn com.huawei.libcore.io.ExternalStorageFileInputStream
-dontwarn com.huawei.libcore.io.ExternalStorageFileOutputStream
-dontwarn com.huawei.libcore.io.ExternalStorageRandomAccessFile
-dontwarn java.lang.management.**
-dontwarn io.ktor.**

-keep class org.telegram.** { *; }
-keep class com.th3nekit.finegram.** { *; }

-keep interface com.th3nekit.finegram.plugins.api.** { *; }
-keep class com.th3nekit.finegram.plugins.api.** { *; }

-keep class io.nekohasekai.libbox.** { *; }
-keep class go.** { *; }
-keep class com.th3nekit.finegram.net.outbound.** { *; }

-keep class com.exteragram.** { *; }
-keep interface com.exteragram.** { *; }

-keep class de.robv.android.xposed.** { *; }
-keep interface de.robv.android.xposed.** { *; }
-keep class com.th3nekit.finegram.hook.** { *; }

-keep class org.telegram.messenger.R$* { *; }
-keepclassmembers class org.telegram.messenger.R$* { public static <fields>; }

-keep class com.th3nekit.finegram.profile.pills.** { *; }

-keep class com.th3nekit.finegram.plugins.** { *; }

-keep class org.telegram.messenger.SharedConfig { *; }
-keep class org.telegram.messenger.SharedConfig$* { *; }
-keep class org.telegram.proxy.** { *; }
-keep class org.telegram.tgnet.ConnectionsManager { *; }
-keepclassmembers class org.telegram.ui.ChatActivity {
    public void sendMedia(...);
}

-keep class org.telegram.ui.LaunchActivity { *; }
-keep class org.telegram.ui.DialogsActivity { *; }
-keep class org.telegram.ui.ProxyListActivity { *; }
-keep class org.telegram.ui.ProxyListActivity$* { *; }

-keep class androidx.collection.** { *; }
-keep class androidx.recyclerview.widget.** { *; }
-keepnames class androidx.core.** { *; }

-keep class com.google.android.material.shape.MaterialShapes { *; }
-keep class com.google.android.material.** { *; }
-keep class androidx.core.graphics.ColorUtils { *; }
-keep class androidx.core.content.FileProvider { *; }
-keep class androidx.core.util.Consumer { *; }
-keep class androidx.graphics.shapes.** { *; }

-keep class com.th3nekit.finegram.plugins.FGPluginViews { *; }
-keep class com.th3nekit.finegram.plugins.FGPluginViews$* { *; }
