package org.telegram.tasks

import com.android.build.api.artifact.SingleArtifact
import com.android.build.api.variant.AndroidComponentsExtension
import org.gradle.api.Plugin
import org.gradle.api.Project
import org.gradle.kotlin.dsl.register

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

            val packEmoji = project.tasks.register<PackEmojiTask>("pack${suffix}Emoji") {
                emojiDir.set(project.layout.projectDirectory.dir("../TMessagesProj/emoji-src"))
                outputDir.set(project.layout.buildDirectory.dir("generated/emojiPack/${variant.name}/assets"))
            }
            variant.sources.assets?.addGeneratedSourceDirectory(packEmoji, PackEmojiTask::outputDir)
        }
    }
}
