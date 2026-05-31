plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.serialization")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "com.sylvania.launcher"
    compileSdk = 34
    // NDK pinned to match the runtime stack (Winlator Bionic / Box64), see docs/ARCHITECTURE.md.
    ndkVersion = "27.0.12077973"

    defaultConfig {
        applicationId = "com.sylvania.launcher"
        minSdk = 26
        // targetSdk is deliberately held at 28: the game runtime (Wine-bionic + wowbox64)
        // spawns child processes and accesses an extracted rootfs in internal storage.
        // API 29+ scoped-storage enforcement and the API 31+ phantom-process killer
        // break that model. This mirrors the deliberate choice in Winlator Bionic.
        targetSdk = 28
        versionCode = 1
        versionName = "0.1.0-android"

        ndk {
            // The game runtime is ARM64-only (Box64 / arm64ec WoW64). No 32-bit ABI.
            abiFilters += "arm64-v8a"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
    buildFeatures {
        compose = true
    }
    packaging {
        resources.excludes += "/META-INF/{AL2.0,LGPL2.1}"
    }
}

dependencies {
    // Compose UI
    implementation(platform("androidx.compose:compose-bom:2024.09.03"))
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.activity:activity-compose:1.9.2")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.6")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.6")
    debugImplementation("androidx.compose.ui:ui-tooling")

    // Coroutines
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.8.1")

    // JSON config (matches the C++ launcher's config.json schema)
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.7.1")

    // HTTP downloads (replaces the C++ QNetworkAccessManager)
    implementation("com.squareup.okhttp3:okhttp:4.12.0")

    // ZIP extraction (replaces the C++ PowerShell Expand-Archive). Apache Commons
    // Compress is the same library Winlator uses, supports ZIP64 / data descriptors.
    implementation("org.apache.commons:commons-compress:1.27.1")
    // XZ backend for commons-compress (imagefs.txz / proton-*.txz = tar.xz rootfs).
    implementation("org.tukaani:xz:1.10")

    // --- Tests (pure JVM logic: realmlist text, Config.wtf, legacy migration) ---
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.jetbrains.kotlinx:kotlinx-coroutines-test:1.8.1")
}
