-keep public class com.google.android.gms.* { public *; }
-keepnames @com.google.android.gms.common.annotation.KeepName class *
-keepclassmembernames class * {
    @com.google.android.gms.common.annotation.KeepName *;
}
-keep class org.webrtc.* { *; }
-keep class org.webrtc.audio.* { *; }
-keep class org.webrtc.voiceengine.* { *; }
#-keep class org.telegram.messenger.* { *; }
#-keep class org.telegram.messenger.camera.* { *; }
#-keep class org.telegram.messenger.secretmedia.* { *; }
#-keep class org.telegram.messenger.support.* { *; }
#-keep class org.telegram.messenger.support.* { *; }
#-keep class org.telegram.messenger.time.* { *; }
#-keep class org.telegram.messenger.video.* { *; }
#-keep class org.telegram.messenger.voip.* { *; }
#-keep class org.telegram.SQLite.** { *; }
#-keep class org.telegram.tgnet.ConnectionsManager { *; }
#-keep class org.telegram.tgnet.NativeByteBuffer { *; }
-keep class org.telegram.tgnet.RequestTimeDelegate { *; }
-keep class org.telegram.tgnet.RequestDelegate { *; }
-keep class androidx.media3.** { *; }


-keep class com.google.android.exoplayer2.decoder.SimpleDecoderOutputBuffer { *; }
-keep class org.telegram.ui.Stories.recorder.FfmpegAudioWaveformLoader { *; }
-keep class androidx.mediarouter.app.MediaRouteButton { *; }
-keepclassmembers class ** {
    @android.webkit.JavascriptInterface <methods>;
}

-keep class org.telegram.utils.code.highlight.PrismaHighlighter {
    private static native <methods>;
}

# https://developers.google.com/ml-kit/known-issues#android_issues
-keep class com.google.mlkit.nl.languageid.internal.LanguageIdentificationJni { *; }

# Constant folding for resource integers may mean that a resource passed to this method appears to be unused. Keep the method to prevent this from happening.
-keep class com.google.android.exoplayer2.upstream.RawResourceDataSource {
  public static android.net.Uri buildRawResourceUri(int);
}

# Methods accessed via reflection in DefaultExtractorsFactory
-dontnote com.google.android.exoplayer2.ext.flac.FlacLibrary
-keepclassmembers class com.google.android.exoplayer2.ext.flac.FlacLibrary {

}

# Some members of this class are being accessed from native methods. Keep them unobfuscated.
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

# Constructors accessed via reflection in DefaultExtractorsFactory
-dontnote com.google.android.exoplayer2.ext.flac.FlacExtractor
-keepclassmembers class com.google.android.exoplayer2.ext.flac.FlacExtractor {
  <init>();
}

# Constructors accessed via reflection in DefaultDownloaderFactory
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

# Constructors accessed via reflection in DownloadHelper
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

# Huawei Services
-keep class com.huawei.hianalytics.**{ *; }
-keep class com.huawei.updatesdk.**{ *; }
-keep class com.huawei.hms.**{ *; }

# Used by AtomicReferenceFieldUpdater and sun.misc.Unsafe
-keepclassmembers class com.google.common.util.concurrent.AbstractFuture** {
  *** waiters;
  *** value;
  *** listeners;
  *** thread;
  *** next;
}

# Since Unsafe is using the field offsets of these inner classes, we don't want
# to have class merging or similar tricks applied to these classes and their
# fields. It's safe to allow obfuscation, since the by-name references are
# already preserved in the -keep statement above.
-keep,allowshrinking,allowobfuscation class com.google.common.util.concurrent.AbstractFuture** {
  <fields>;
}

-keepclasseswithmembers,allowobfuscation class * {
  @com.google.gson.annotations.SerializedName <fields>;
}

# Gson uses generic type information stored in a class file when working with fields. Proguard
# removes such information by default, so configure it to keep all of it.
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

# Keep Cherrygram fields name
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

# Keep all class member names of CameraX
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

# JLatexMath: macro/atom classes are loaded reflectively by Class.forName
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
# Chaquopy приносит с собой ktor, а тот на обычной Java заглядывает в java.lang.management.
# На Android этого пакета нет, и до него никогда не доходит: правило гасит предупреждение,
# из-за которого R8 отказывался собирать релиз.
-dontwarn java.lang.management.**
-dontwarn io.ktor.**

# Плагины обращаются к клиенту по именам: находят класс, метод и поле строкой, а потом
# перехватывают или вызывают их. R8 сокращал имена методов и полей (ignoreSetOnline -> ধ),
# и в собранном клиенте плагины просто ничего не находили. Имена сохраняем, а вырезать
# неиспользуемое R8 по-прежнему может — keepnames этому не мешает.
-keep class org.telegram.** { *; }
-keep class com.th3nekit.finegram.** { *; }

# Интерфейсы, по которым пишутся плагины на dex. Внутри клиента их никто не реализует,
# поэтому без явного правила R8 выбросил бы их целиком.
-keep interface com.th3nekit.finegram.plugins.api.** { *; }
-keep class com.th3nekit.finegram.plugins.api.** { *; }

# Слой совместимости с плагинами exteraGram. Внутри клиента эти классы почти никто не
# зовёт — их ищут сами плагины, по именам, — поэтому без явного правила R8 выбросил бы их
# целиком, и плагин падал бы ещё на импорте.
# Ядро своих серверов (sing-box). Всё общение с ним идёт через JNI: имена классов и
# методов зашиты в нативной части, и переименование их R8 ломает ядро на первом же вызове.
# Обратные вызовы (PlatformInterface, CommandServerHandler) ядро зовёт из Go, поэтому наши
# реализации тоже нельзя ни выбросить, ни переименовать.
-keep class io.nekohasekai.libbox.** { *; }
-keep class go.** { *; }
-keep class com.th3nekit.finegram.net.outbound.** { *; }

-keep class com.exteragram.** { *; }
-keep interface com.exteragram.** { *; }

# То же про слой перехватов: плагины наследуют эти классы и зовут их методы по именам.
-keep class de.robv.android.xposed.** { *; }
-keep interface de.robv.android.xposed.** { *; }
# Движок перехватов: заглушки LSPlant зовут HookTarget.callback по имени, а natives
# ArtHook находятся по именам JNI. Кадр вызова Python-плагины читают по именам полей.
-keep class com.th3nekit.finegram.hook.** { *; }

# Плагины берут значки и строки клиента по именам: R.drawable.msg_settings и подобное.
# R8 подставляет такие константы прямо в код и выбрасывает поля за ненадобностью — для
# клиента это безвредно, а плагин падает, не найдя поля. Классы ресурсов держим целиком.
-keep class org.telegram.messenger.R$* { *; }
-keepclassmembers class org.telegram.messenger.R$* { public static <fields>; }

# Карточки профиля: их объявляют плагины, внутри клиента эти вызовы почти не встречаются.
-keep class com.th3nekit.finegram.profile.pills.** { *; }

# Мост между клиентом и плагинами. Его методы зовёт только Python — для R8 это мёртвый
# код: он их выбрасывает или переименовывает, и плагин падает на ровном месте
# («'FGHookBridge' object has no attribute 'runOnUiThread'»). Здесь нужен именно keep:
# keepnames сохраняет имена, но не мешает выбрасывать неиспользуемое.
-keep class com.th3nekit.finegram.plugins.** { *; }

# Классы клиента, которые плагины ищут по именам и приватным полям. Без этих правил R8
# переименовывает их, и плагин, написанный для exteraGram, не находит ни прокси, ни
# соединение — со стороны это выглядит как «плагин не запускается».
-keep class org.telegram.messenger.SharedConfig { *; }
-keep class org.telegram.messenger.SharedConfig$* { *; }
-keep class org.telegram.proxy.** { *; }
-keep class org.telegram.tgnet.ConnectionsManager { *; }
# Плагины ищут методы клиента по имени и точному набору доводов. Переименованный или
# выброшенный метод для них — «такого нет», причём молча.
-keepclassmembers class org.telegram.ui.ChatActivity {
    public void sendMedia(...);
}

-keep class org.telegram.ui.LaunchActivity { *; }
-keep class org.telegram.ui.DialogsActivity { *; }
-keep class org.telegram.ui.ProxyListActivity { *; }
-keep class org.telegram.ui.ProxyListActivity$* { *; }

# dex-плагины, собранные под exteraGram, вызывают методы androidx по их настоящим именам.
# R8 оставлял классам имена, но переименовывал методы (LongSparseArray.get стал «㟟»), и
# плагин падал с NoSuchMethodError. Коллекции и список держим целиком — это маленькие
# библиотеки, которыми клиент и так пользуется; в остальном androidx.core сохраняем имена.
-keep class androidx.collection.** { *; }
-keep class androidx.recyclerview.widget.** { *; }
-keepnames class androidx.core.** { *; }

# Фигуры Material 3 для плагинов. Модули находят MaterialShapes по имени класса и поля, а
# контур строят через Shapes_androidKt.toPath, который ищут перебором методов. Без keep R8
# выкинул бы неиспользуемые клиентом фигуры и переименовал методы, и форма аватарок пропала бы.
-keep class com.google.android.material.shape.MaterialShapes { *; }
-keep class com.google.android.material.** { *; }
-keep class androidx.core.graphics.ColorUtils { *; }
-keep class androidx.core.content.FileProvider { *; }
-keep class androidx.core.util.Consumer { *; }
-keep class androidx.graphics.shapes.** { *; }

# Подставные вью для плагинов: плагин ищет этот класс по имени, своих ссылок на него в
# клиенте нет, и без правила он уходит при сжатии вместе со всеми потомками.
-keep class com.th3nekit.finegram.plugins.FGPluginViews { *; }
-keep class com.th3nekit.finegram.plugins.FGPluginViews$* { *; }
