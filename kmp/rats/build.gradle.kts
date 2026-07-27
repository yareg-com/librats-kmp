import java.util.Properties

plugins {
    alias(libs.plugins.kotlin.multiplatform)
    alias(libs.plugins.kotlin.android)
    id("maven-publish")
}

group = "com.github.yareg-com"

kotlin {
    compilerOptions {
        freeCompilerArgs = listOf(
            "-Xexpect-actual-classes" // TODO: Remove after expect/actual becomes stable
        )
    }

    jvm()

    android {
        namespace = "com.librats"
        minSdk = 21
        compileSdk = libs.versions.android.sdk.compile.get().toInt()
        buildToolsVersion = libs.versions.android.build.tools.get()
    }

    sourceSets {
        commonMain.dependencies {
            implementation(libs.kotlinx.coroutines.core)
        }

        androidMain.dependencies {
            implementation(projects.jni)
        }
    }
}

publishing {
    repositories {
        maven {
            name = "GitHubPackages"
            url = uri("https://maven.pkg.github.com/yareg-com/librats-kmp")
            credentials {
                username = System.getenv("GITHUB_ACTOR")
                password = System.getenv("GITHUB_TOKEN")
            }
        }
    }
}

val androidSdkDir by lazy {
    System.getenv("ANDROID_HOME") ?:
    System.getenv("ANDROID_SDK_ROOT") ?:
    Properties().run {
        println("Looking for local.properies...")
        try {
            load(rootProject.file("local.properties").inputStream())
            getProperty("sdk.dir")
        } catch (_: Exception) {
            println("Failed to find local.properies")
            null
        }
    }.also {
        if (it != null) {
            println("Found Android SDK: $it")
        }
    }
}

val androidNdkDir by lazy {
    androidSdkDir?.let {
        val path = file("$it/ndk")

        if (path.exists()) {
            println("Found NDK path: $path")
            val latestNdk = path.listFiles()?.filter { file -> file.isDirectory}?.maxOrNull()

            if (latestNdk?.exists() == true) {
                println("Found latest NDK path: $latestNdk")
                latestNdk
            } else null
        } else null
    }
}

val cmake: String by lazy {
    androidSdkDir?.let {
        val path = file("$it/cmake")

        if (path.exists()) {
            println("Found cmake path: $path")
            val binary = path.listFiles()?.maxOrNull()?.resolve("bin/cmake")

            if (binary?.exists() == true) {
                println("Using cmake from Android SDK: $binary")
                binary.absolutePath
            } else null
        } else null
    } ?: "cmake".also {
        println("Using system cmake")
    }
}

fun ndkBinary(
    name: String
): String? = androidNdkDir?.let {
    it.walkTopDown().firstOrNull { file ->
        file.isFile &&
        (file.name == name || file.name == "$name.exe") &&
        file.parentFile?.name == "bin"
    }?.run {
        if (exists()) {
            println("Using $name from Android NDK: $this")
            absolutePath
        } else null
    }
}

val cCompiler: String? by lazy {
    ndkBinary("clang")
}

val cxxCompiler: String? by lazy {
    ndkBinary("clang++")
}

val cxxBuildDir = "intermediates/cxx"

val cmakeGenerate = tasks.register<Exec>("cmakeGenerate") {
    description = "Generate build files"

    val sourceDir = file("../..")
    val buildDir = layout.buildDirectory.dir(cxxBuildDir).get().asFile

    inputs.file(file("$sourceDir/CMakeLists.txt"))
    inputs.dir(file("$sourceDir/src"))
    inputs.dir(file("$sourceDir/tests"))
    outputs.dir(buildDir)

    val args = mutableListOf(
        cmake,
        "-DRATS_BUILD_TESTS=OFF",
        "-DRATS_BUILD_CLIENT=OFF",
        "-DRATS_SHARED_LIBRARY=ON",
        "-DRATS_STATIC_LIBRARY=OFF",
        "-DCMAKE_BUILD_TYPE=Release",
        "-B", buildDir.absolutePath,
        "-S", sourceDir.absolutePath
    )

    cCompiler?.let { c ->
        cxxCompiler?.let { cxx ->
            args.add("-DCMAKE_C_COMPILER=$c")
            args.add("-DCMAKE_CXX_COMPILER=$cxx")
        }
    }

    commandLine(args)
}

val cmakeBuild = tasks.register<Exec>("cmakeBuild") {
    description = "Compile C++ code"

    dependsOn(cmakeGenerate)

    val buildDir = layout.buildDirectory.dir(cxxBuildDir).get().asFile
    inputs.dir(buildDir)

    commandLine(cmake, "--build", buildDir.absolutePath, "--parallel")
}

val buildJvmNativeLib = tasks.register<Copy>("buildJvmNativeLib") {
    description = "Copy shared library into resources"
    dependsOn(cmakeBuild)

    from(layout.buildDirectory.dir("$cxxBuildDir/lib")) {
        include("librats.so")
    }

    into(layout.projectDirectory.dir("src/jvmMain/resources"))
}

tasks.named("jvmProcessResources") {
    dependsOn(buildJvmNativeLib)
}