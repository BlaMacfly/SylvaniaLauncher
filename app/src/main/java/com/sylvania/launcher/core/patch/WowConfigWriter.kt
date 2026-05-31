package com.sylvania.launcher.core.patch

import java.io.File

/**
 * Generates the optimal `WTF/Config.wtf`, ported verbatim from the C++
 * `HdPatchManager::generateConfigWtf`. Pure logic (no Android deps) so it is
 * unit-testable; [writeConfigWtf] performs the actual file write.
 *
 * Note: the original C++ contained mojibake for the voice-chat driver names
 * (a Windows-1252 artifact). We emit clean UTF-8 "System Default" instead,
 * which 3.3.5a parses identically and avoids garbled bytes on Linux/Android.
 */
object WowConfigWriter {

    /** The fixed Config.wtf body. Order preserved from the C++ source. */
    fun configWtfContents(): String = buildString {
        appendLine("""SET locale "frFR"""")
        appendLine("""SET hwDetect "0"""")
        appendLine("""SET gxRefresh "60"""")
        appendLine("""SET gxMultisampleQuality "0.000000"""")
        appendLine("""SET gxWindow "1"""")
        appendLine("""SET videoOptionsVersion "3"""")
        appendLine("""SET movie "0"""")
        appendLine("""SET Gamma "1.000000"""")
        appendLine("""SET readTOS "1"""")
        appendLine("""SET readEULA "1"""")
        appendLine("""SET readTerminationWithoutNotice "1"""")
        appendLine("""SET showToolsUI "1"""")
        appendLine("""SET Sound_OutputDriverName "System Default"""")
        appendLine("""SET Sound_MusicVolume "0.40000000596046"""")
        appendLine("""SET Sound_AmbienceVolume "0.60000002384186"""")
        appendLine("""SET componentTextureLevel "9"""")
        appendLine("""SET farclip "1277"""")
        appendLine("""SET projectedTextures "1"""")
        appendLine("""SET weatherDensity "3"""")
        appendLine("""SET accounttype "LK"""")
        appendLine("""SET mouseSpeed "1.2000000476837"""")
        appendLine("""SET gameTip "16"""")
        appendLine("""SET uiScale "0.75999999046326"""")
        appendLine("""SET useUiScale "1"""")
        appendLine("""SET checkAddonVersion "0"""")
        appendLine("""SET Sound_VoiceChatInputDriverName "System Default"""")
        appendLine("""SET Sound_VoiceChatOutputDriverName "System Default"""")
        appendLine("""SET movieSubtitle "1"""")
        appendLine("""SET Sound_SFXVolume "0.5"""")
        appendLine("""SET dbCompress "0"""")
        appendLine("""SET gxResolution "1920x1080"""")
        appendLine("""SET textureFilteringMode "5"""")
        appendLine("""SET groundEffectDist "140"""")
        appendLine("""SET environmentDetail "1.5"""")
        appendLine("""SET timingTestError "0"""")
        appendLine("""SET windowResizeLock "1"""")
        appendLine("""SET particleDensity "1"""")
        appendLine("""SET mapShadows "0"""")
        appendLine("""SET vertexShaders "0"""")
        appendLine("""SET pixelShaders "0"""")
        appendLine("""SET M2UseShaders "0"""")
        appendLine("""SET M2Faster "3"""")
        appendLine("""SET shadowinstancing "0"""")
        appendLine("""SET objectFade "0"""")
        appendLine("""SET timingMethod "2"""")
        appendLine("""SET ffxSpecial "0"""")
        appendLine("""SET groundEffectDensity "64"""")
        appendLine("""SET realmList "sylvania-servergame.com"""")
        appendLine("""SET realmName "The Kingdom of Sylvania"""")
        appendLine("""SET specular "1"""")
        appendLine("""SET Sound_MasterVolume "0.60000002384186"""")
        appendLine("""SET Sound_ZoneMusicNoDelay "1"""")
        appendLine("""SET gxTripleBuffer "1"""")
        appendLine("""SET gxMultisample "8"""")
        appendLine("""SET shadowLevel "0"""")
        appendLine("""SET extShadowQuality "5"""")
    }

    /** Writes WTF/Config.wtf under [wowPath]. Returns true on success. */
    fun writeConfigWtf(wowPath: File): Boolean = try {
        val wtfDir = File(wowPath, "WTF").apply { mkdirs() }
        File(wtfDir, "Config.wtf").writeText(configWtfContents(), Charsets.UTF_8)
        true
    } catch (e: Exception) {
        false
    }
}
