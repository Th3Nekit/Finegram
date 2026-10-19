package org.telegram.tasks

import com.android.build.api.artifact.SingleArtifact
import com.android.build.api.variant.AndroidComponentsExtension
import org.gradle.api.Plugin
import org.gradle.api.Project
import org.gradle.kotlin.dsl.register

/**
 * Кладёт в ассеты сборки таблицу «id строки → хеш её имени».
 *
 * Клиент ищет строку сначала по загруженному переводу и только потом уходит в R:
 * для этого ему нужно по числовому id узнать имя ресурса, а имена в собранном
 * приложении не сохраняются. Таблицу строим из R.txt того же варианта сборки,
 * иначе id разъедутся и подстановки переводов молча перестанут совпадать.
 */
class StringResourceIdsPlugin : Plugin<Project> {
    override fun apply(project: Project) {
        val androidComponents = project.extensions.findByType(AndroidComponentsExtension::class.java)
            ?: error("Apply com.android.application before org.telegram.string-resource-ids")

        androidComponents.onVariants { variant ->
            val suffix = variant.name.replaceFirstChar { it.uppercase() }
            val task = project.tasks.register<GenerateStringResourceIdsAssetTask>(
                "generate${suffix}StringResourceIdsAsset"
            ) {
                runtimeSymbolList.set(variant.artifacts.get(SingleArtifact.RUNTIME_SYMBOL_LIST))
                outputDir.set(
                    project.layout.buildDirectory.dir("generated/stringResourceIds/${variant.name}/assets")
                )
            }
            variant.sources.assets?.addGeneratedSourceDirectory(
                task, GenerateStringResourceIdsAssetTask::outputDir
            )

            val packEmoji = project.tasks.register<PrepareEmojiPackTask>("prepare${suffix}EmojiPack") {
                emojiPack.set(project.layout.projectDirectory.file("../TMessagesProj/src/main/assets/emoji.pack"))
                outputDir.set(project.layout.buildDirectory.dir("generated/emojiPack/${variant.name}/assets"))
            }
            variant.sources.assets?.addGeneratedSourceDirectory(packEmoji, PrepareEmojiPackTask::outputDir)
        }
    }
}
