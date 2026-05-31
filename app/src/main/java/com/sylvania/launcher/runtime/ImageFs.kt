package com.sylvania.launcher.runtime

import android.content.Context
import java.io.File

/**
 * Locations of the extracted bionic rootfs ("imagefs"), ported in spirit from
 * Winlator's ImageFs. The rootfs lives in app-internal storage so no storage
 * permission is needed and Wine/Box64 can reference real absolute paths.
 *
 *   <filesDir>/imagefs/                 root ("/")
 *   <filesDir>/imagefs/usr/             PREFIX
 *   <filesDir>/imagefs/home/xuser/      HOME
 */
class ImageFs(val rootDir: File) {

    val usrDir: File get() = File(rootDir, "usr")
    val libDir: File get() = File(rootDir, "usr/lib")
    val binDir: File get() = File(rootDir, "usr/bin")
    val homeDir: File get() = File(rootDir, HOME_RELATIVE)
    private val versionFile: File get() = File(rootDir, ".sylvania_imagefs_version")

    fun installedVersion(): Int =
        versionFile.takeIf { it.exists() }?.readText()?.trim()?.toIntOrNull() ?: -1

    fun isInstalled(expected: Int): Boolean = installedVersion() == expected

    fun markInstalled(version: Int) {
        rootDir.mkdirs()
        versionFile.writeText(version.toString())
    }

    companion object {
        const val USER = "xuser"
        const val HOME_RELATIVE = "home/$USER"
        const val HOME_PATH = "/home/$USER"

        fun of(context: Context): ImageFs = ImageFs(File(context.filesDir, "imagefs"))
    }
}
