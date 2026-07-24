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
        androidMain.dependencies {
            implementation(projects.jni)
        }
    }
}

/*val cmakeGenerate = tasks.register<Exec>("cmakeGenerate") {
    description = "Generate build files"

    val cppDir = file("cpp")
    val buildDir = layout.buildDirectory.dir("cmake-jvm").get().asFile

    inputs.dir(cppDir)
    outputs.dir(buildDir)

    commandLine("cmake", "-B", buildDir.absolutePath, "-S", cppDir.absolutePath)
}

val cmakeBuild = tasks.register<Exec>("cmakeBuild") {
    description = "Compile C++ code"

    dependsOn(cmakeGenerate)

    val buildDir = layout.buildDirectory.dir("cmake-jvm").get().asFile
    inputs.dir(buildDir)

    commandLine("cmake", "--build", buildDir.absolutePath)
}

val buildJvmNativeLib = tasks.register("buildJvmNativeLib") {
    description = "Copy binary into resources"
    dependsOn(cmakeBuild)

    val buildDir = layout.buildDirectory.dir("cmake-jvm").get().asFile
    val resourceDir = file("src/jvmMain/resources/native")

    inputs.dir(buildDir)
    outputs.dir(resourceDir)

    doLast {
        copy {
            from(buildDir)
            into(resourceDir)
            include("*.so", "*.dylib", "*.dll")
        }
    }
}

tasks.named("jvmProcessResources") {
    dependsOn(buildJvmNativeLib)
}*/