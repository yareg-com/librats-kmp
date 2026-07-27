package com.librats

import java.io.File
import java.nio.file.Files

actual fun loadLibrary(
    name: String
) {
    val libName = System.mapLibraryName(name) // "librats" -> "librats.so" on Linux
    val libFile = File(getCacheDir(name), libName)

    if (!libFile.exists()) {
        val stream = RatsClient.javaClass.getResourceAsStream("/$libName") ?: error(
            "Could not find $libName in JAR resources"
        )

        stream.use { input ->
            Files.copy(input, libFile.toPath())
        }
    }

    System.loadLibrary(libFile.absolutePath)
}

private fun getCacheDir(
    appName: String
): File {
    val userHome = System.getProperty("user.home")
    val xdgCache = System.getenv("XDG_CACHE_HOME")

    val parent = when {
        !xdgCache.isNullOrEmpty() -> File(xdgCache)
        userHome != null          -> File(userHome, ".cache")
        else                      -> File(System.getProperty("java.io.tmpdir"))
    }

    return File(parent, "$appName/lib").apply {
        if (!exists()) mkdirs()
    }
}