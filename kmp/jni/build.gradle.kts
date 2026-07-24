plugins {
    alias(libs.plugins.android.library)
}

android {
    namespace = "com.librats.jni"
    compileSdk = libs.versions.android.sdk.compile.get().toInt()
    buildToolsVersion = libs.versions.android.build.tools.get()

    defaultConfig {
        minSdk = 21

        ndk {
            abiFilters.addAll(setOf("arm64-v8a", "armeabi-v7a", "x86_64", "x86"))
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../android/src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}