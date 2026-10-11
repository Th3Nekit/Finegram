/*
 * Finegram plugin store.
 * Adapted from Kangel Plugins Manager (KangelPlugins).
 * Upstream: https://git.kangel.xyz/KangelPlugins/PluginManager
 * Licensed under GNU GPL v3; see LICENSE.PluginManager and NOTICE.
 */

package com.th3nekit.finegram.store

import android.util.Base64
import com.th3nekit.finegram.core.FinegramLogger
import org.telegram.messenger.ApplicationLoader
import org.telegram.messenger.R
import java.io.File
import java.security.KeyFactory
import java.security.MessageDigest
import java.security.PublicKey
import java.security.Signature
import java.security.spec.X509EncodedKeySpec

object FGStoreVerify {

    enum class Result {

        OK,

        UNCHECKED,

        MISMATCH,
    }

    @Volatile
    private var cachedKey: PublicKey? = null

    @JvmStatic
    fun verify(item: FGStore.Item, file: File): Result {
        val hash = sha256(file)
        if (item.hash.isNotBlank()) {
            if (!item.hash.equals(hash, ignoreCase = true)) {
                FinegramLogger.e("FGStore", { "отпечаток ${item.id} не сошёлся" })
                return Result.MISMATCH
            }
        }
        if (item.signature.isNotBlank()) {
            val key = publicKey()
            if (key != null) {
                val message = "${item.id}:${item.version}:$hash"
                if (!signatureValid(key, message, item.signature)) {
                    FinegramLogger.e("FGStore", { "подпись ${item.id} не сошлась" })
                    return Result.MISMATCH
                }
                return Result.OK
            }
        }
        return if (item.hash.isBlank()) Result.UNCHECKED else Result.OK
    }

    private fun signatureValid(key: PublicKey, message: String, signatureBase64: String): Boolean =
        try {
            val verifier = Signature.getInstance("SHA256withRSA")
            verifier.initVerify(key)
            verifier.update(message.toByteArray(Charsets.UTF_8))
            verifier.verify(Base64.decode(signatureBase64, Base64.DEFAULT))
        } catch (e: Throwable) {
            FinegramLogger.e("FGStore", { "подпись не проверилась" }, e)
            false
        }

    private fun publicKey(): PublicKey? {
        cachedKey?.let { return it }
        return try {
            val pem = ApplicationLoader.applicationContext.resources
                .openRawResource(R.raw.store_key)
                .bufferedReader()
                .use { it.readText() }
            val body = pem.lineSequence()
                .filterNot { it.startsWith("-----") }
                .joinToString("")
            val der = Base64.decode(body, Base64.DEFAULT)
            val key = KeyFactory.getInstance("RSA").generatePublic(X509EncodedKeySpec(der))
            cachedKey = key
            key
        } catch (e: Throwable) {
            FinegramLogger.e("FGStore", { "ключ каталога не прочитался" }, e)
            null
        }
    }

    private fun sha256(file: File): String = try {
        val digest = MessageDigest.getInstance("SHA-256")
        file.inputStream().use { input ->
            val buffer = ByteArray(64 * 1024)
            while (true) {
                val read = input.read(buffer)
                if (read <= 0) break
                digest.update(buffer, 0, read)
            }
        }
        digest.digest().joinToString("") { "%02x".format(it) }
    } catch (e: Throwable) {
        FinegramLogger.e("FGStore", { "отпечаток не посчитался" }, e)
        ""
    }
}
