// Top-level build file. Plugin versions are declared here with `apply false`
// and applied in the module build scripts.
plugins {
    id("com.android.application") version "8.5.2" apply false
    id("org.jetbrains.kotlin.android") version "2.0.20" apply false
    id("org.jetbrains.kotlin.plugin.serialization") version "2.0.20" apply false
    // Kotlin 2.0+: the Compose compiler is a Kotlin plugin (replaces
    // composeOptions.kotlinCompilerExtensionVersion).
    id("org.jetbrains.kotlin.plugin.compose") version "2.0.20" apply false
}
