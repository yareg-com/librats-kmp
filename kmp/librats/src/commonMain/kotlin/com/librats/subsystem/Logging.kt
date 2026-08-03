package com.librats.subsystem

import com.librats.LogLevel
import com.librats.RatsClient

object Logging {

    /** Sets the process-global log level (one of {@code LogLevel}). */

    fun setLogLevel(
        logLevel: LogLevel
    ): Unit = RatsClient.nativeSetLogLevel(
        level = logLevel.id
    )

    //------------------------------------------------------------------------------------------------------------------

    /** Mirrors logs to a file (null/empty disables file logging). */

    fun setLogFile(
        path: String
    ): Unit = RatsClient.nativeSetLogFile(
        path = path
    )

    //------------------------------------------------------------------------------------------------------------------

    /** @return static human-readable name of a {@code rats_error_t} value. */

    fun getErrorString(
        errorId: Int
    ): String = RatsClient.nativeErrorStr(
        error = errorId
    )

}