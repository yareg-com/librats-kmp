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

        //publishLibraryVariants("release")
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

val cxxBuildDir = "intermediates/cxx"

val cmakeGenerate = tasks.register<Exec>("cmakeGenerate") {
    description = "Generate build files"

    val sourceDir = file("../..")
    val buildDir = layout.buildDirectory.dir(cxxBuildDir).get().asFile

    inputs.dir(sourceDir)
    outputs.dir(buildDir)

    commandLine(
        "cmake",
        "-DRATS_BUILD_TESTS=OFF",
        "-DRATS_BUILD_CLIENT=OFF",
        "-DRATS_SHARED_LIBRARY=ON",
        "-DRATS_STATIC_LIBRARY=OFF",
        "-DCMAKE_BUILD_TYPE=Release",
        "-B", buildDir.absolutePath,
        "-S", sourceDir.absolutePath
    )
}

val cmakeBuild = tasks.register<Exec>("cmakeBuild") {
    description = "Compile C++ code"

    dependsOn(cmakeGenerate)

    val buildDir = layout.buildDirectory.dir(cxxBuildDir).get().asFile
    inputs.dir(buildDir)

    commandLine("cmake", "--build", buildDir.absolutePath, "--parallel")
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