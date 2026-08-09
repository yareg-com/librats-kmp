package com.librats.jni

import android.util.Log

object Android {

    private val tag: String
        get() = javaClass.simpleName

    fun loadLibrary(
        name: String
    ) = try {
        System.loadLibrary(name)
    } catch (e: UnsatisfiedLinkError) {
        Log.e(tag, "Failed to load native library", e)
        throw e
    }

}