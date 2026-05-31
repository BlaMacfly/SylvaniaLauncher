package com.sylvania.launcher.core.patch

import java.io.File

/**
 * Enable/disable individual HD patch components by renaming the .mpq file to
 * `.mpq.disabled` and back — ported from the C++ `SettingsDialog` patch-state
 * helpers. Files are never deleted, only renamed, so toggling is reversible.
 */
object HdComponentToggle {

    /** Root patch path: Data/patch-<letter>.mpq */
    fun rootPatchPath(wowPath: File, letter: String): File =
        File(wowPath, "Data/patch-$letter.mpq")

    /** Locale patch path: Data/<locale>/patch-<locale>-<letter>.mpq */
    fun localePatchPath(wowPath: File, locale: String, letter: String): File =
        File(wowPath, "Data/$locale/patch-$locale-$letter.mpq")

    /** True if the component exists in either enabled or disabled form. */
    fun exists(activePath: File): Boolean =
        activePath.exists() || File(activePath.path + ".disabled").exists()

    /** True if the component is currently enabled (the active .mpq is present). */
    fun isEnabled(activePath: File): Boolean = activePath.exists()

    /**
     * Sets the enabled state of a component. Returns true if a rename happened
     * (or the state already matched), false if neither file is present.
     */
    fun setEnabled(activePath: File, enabled: Boolean): Boolean {
        val disabledPath = File(activePath.path + ".disabled")
        return when {
            enabled && disabledPath.exists() -> disabledPath.renameTo(activePath)
            enabled && activePath.exists() -> true // already enabled
            !enabled && activePath.exists() -> activePath.renameTo(disabledPath)
            !enabled && disabledPath.exists() -> true // already disabled
            else -> false // component not present at all
        }
    }
}
