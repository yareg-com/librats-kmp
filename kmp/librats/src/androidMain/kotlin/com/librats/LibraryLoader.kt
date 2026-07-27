package com.librats

import com.librats.jni.Android

actual fun loadLibrary(name: String) = Android.loadLibrary()