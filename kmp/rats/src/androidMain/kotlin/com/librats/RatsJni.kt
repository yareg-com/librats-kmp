package com.librats

import com.librats.jni.Android

actual fun loadNativeLib() = Android.loadLibrary()