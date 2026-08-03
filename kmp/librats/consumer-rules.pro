# Native methods are resolved by name from librats_jni.cpp
-keep class com.librats.RatsClient {
    native <methods>;
}

# Callback interfaces and their methods are invoked from native code via
# GetMethodID (onConnected/onDisconnected/onMessage/onTopicMessage/
# onJsonMessage/onFileOffer/onFileProgress/onFileComplete) — keep their names
-keep interface com.librats.callback.** { *; }