package com.sylvania.launcher.core

import com.sylvania.launcher.core.config.RealmlistEntry
import com.sylvania.launcher.core.patch.HdComponentToggle
import com.sylvania.launcher.core.patch.WowConfigWriter
import com.sylvania.launcher.core.realmlist.RealmlistWriter
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

/**
 * Pure-JVM unit tests for the ported launcher logic. These run without a
 * device/emulator (`./gradlew test`) and validate the behaviour we ported from
 * the C++ source — the "testable hors jeu" deliverable of Chantier B.
 */
class LauncherLogicTest {

    @get:Rule
    val tmp = TemporaryFolder()

    @Test
    fun legacyRealmlistIsMigratedToCanonical() {
        val entries = listOf(
            RealmlistEntry("Old", "set realmlist logon.sylvania-wow.com", true),
            RealmlistEntry("Keep", "set realmlist sylvania-servergame.com", false),
        )
        val (migrated, didMigrate) = RealmlistWriter.migrateLegacy(entries)
        assertTrue(didMigrate)
        assertEquals(Constants.CANONICAL_REALMLIST, migrated[0].address)
        assertEquals("set realmlist sylvania-servergame.com", migrated[1].address)
    }

    @Test
    fun nonLegacyRealmlistIsUnchanged() {
        val entries = listOf(RealmlistEntry("Cur", Constants.CANONICAL_REALMLIST, true))
        val (_, didMigrate) = RealmlistWriter.migrateLegacy(entries)
        assertFalse(didMigrate)
    }

    @Test
    fun realmlistWrittenOnlyWhereParentDirExists() {
        val wow = tmp.newFolder("wow")
        File(wow, "Data/enUS").mkdirs() // only enUS locale present
        val ok = RealmlistWriter.updateRealmlist(wow, Constants.CANONICAL_REALMLIST)
        assertTrue(ok)
        assertEquals(
            Constants.CANONICAL_REALMLIST,
            File(wow, "realmlist.wtf").readText().trim(),
        )
        assertEquals(
            Constants.CANONICAL_REALMLIST,
            File(wow, "Data/enUS/realmlist.wtf").readText().trim(),
        )
        // frFR dir does not exist -> no file created.
        assertFalse(File(wow, "Data/frFR/realmlist.wtf").exists())
    }

    @Test
    fun realmlistHandlesLowercaseAndExistingLocaleDirs() {
        // Mirrors the real D:\wotlk client: lowercase "enus", mixed "frFR".
        val wow = tmp.newFolder("wow")
        File(wow, "Data/enus").mkdirs()
        File(wow, "Data/frFR").mkdirs()
        val ok = RealmlistWriter.updateRealmlist(wow, Constants.CANONICAL_REALMLIST)
        assertTrue(ok)
        assertEquals(Constants.CANONICAL_REALMLIST, File(wow, "realmlist.wtf").readText().trim())
        assertEquals(Constants.CANONICAL_REALMLIST, File(wow, "Data/enus/realmlist.wtf").readText().trim())
        assertEquals(Constants.CANONICAL_REALMLIST, File(wow, "Data/frFR/realmlist.wtf").readText().trim())
    }

    @Test
    fun configWtfContainsSylvaniaRealmAndKeySettings() {
        val body = WowConfigWriter.configWtfContents()
        assertTrue(body.contains("""SET realmList "sylvania-servergame.com""""))
        assertTrue(body.contains("""SET gxResolution "1920x1080""""))
        // No mojibake should survive the port.
        assertFalse(body.contains("Ã©"))
    }

    @Test
    fun configWtfWrittenToWtfDir() {
        val wow = tmp.newFolder("wow")
        assertTrue(WowConfigWriter.writeConfigWtf(wow))
        assertTrue(File(wow, "WTF/Config.wtf").exists())
    }

    @Test
    fun hdComponentToggleRenamesReversibly() {
        val wow = tmp.newFolder("wow")
        val active = HdComponentToggle.rootPatchPath(wow, "w")
        active.parentFile.mkdirs()
        active.writeText("mpq")

        assertTrue(HdComponentToggle.isEnabled(active))
        assertTrue(HdComponentToggle.setEnabled(active, false))
        assertFalse(active.exists())
        assertTrue(File(active.path + ".disabled").exists())

        assertTrue(HdComponentToggle.setEnabled(active, true))
        assertTrue(active.exists())
        assertFalse(File(active.path + ".disabled").exists())
    }
}
