package com.librats

enum class LogLevel(
    val id: Int
) {
    // Ids should match rats_log_level_t values

    Debug(0),
    Info(1),
    Warn(2),
    Error(3)
}