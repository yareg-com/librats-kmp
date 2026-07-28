package com.librats

import java.io.File

@Suppress("UnsafeDynamicallyLoadedCode")
actual fun loadLibrary(
    name: String
) {
    val libName = System.mapLibraryName(name) // "rats_jni" -> "librats_jni.so" on Linux

    val hash = RatsClient.javaClass.getResourceAsStream("/$libName.sha256")?.use {
        it.readAllBytes().decodeToString().trim()//.take(12)
    } ?: error("Unable to get resource: $libName.sha256")

    val libFile = File(getCacheDir(name), "$hash/$libName")

    if (!libFile.exists()) {
        RatsClient.javaClass.getResourceAsStream("/$libName")?.use {
            it.copyTo(libFile.outputStream())
        } ?: error("Unable to get resource: $libName")
    }

    System.load(libFile.absolutePath)
}

private fun getCacheDir(
    appName: String
): File {
    val userHome = System.getProperty("user.home")
    val xdgCache = System.getenv("XDG_CACHE_HOME")

    return File(
        when {
            !xdgCache.isNullOrEmpty() -> File(xdgCache)
            userHome != null          -> File(userHome, ".cache")
            else                      -> File(System.getProperty("java.io.tmpdir"))
        },
        "$appName/lib"
    ).apply {
        if (!exists()) mkdirs()
    }
}