package com.librats

/**
 * Exception thrown by [RatsClient] for failed native operations.
 * 
 * 
 * The [.getErrorCode] mirrors the C ABI `rats_error_t` enum
 * (see `src/bindings/rats.h`). [RatsClient.OK] (0) is success; any
 * other value is an error.
 */
class RatsException : RuntimeException {
    /** @return the underlying `rats_error_t` code.
     */
    val errorCode: Int

    constructor(message: String?) : super(message) {
        this.errorCode = RatsClient.ERR_INTERNAL
    }

    constructor(errorCode: Int) : super(getErrorMessage(errorCode)) {
        this.errorCode = errorCode
    }

    constructor(message: String?, errorCode: Int) : super(message) {
        this.errorCode = errorCode
    }

    constructor(message: String?, cause: Throwable?) : super(message, cause) {
        this.errorCode = RatsClient.ERR_INTERNAL
    }

    companion object {
        /** Human-readable name for a `rats_error_t` value.  */
        fun getErrorMessage(errorCode: Int): String {
            return when (errorCode) {
                RatsClient.OK                  -> "OK"
                RatsClient.ERR_INVALID_ARG     -> "Invalid argument"
                RatsClient.ERR_NOT_STARTED     -> "Node not started"
                RatsClient.ERR_ALREADY_STARTED -> "Node already started"
                RatsClient.ERR_NOT_ENABLED     -> "Subsystem not enabled"
                RatsClient.ERR_NO_SUCH_PEER    -> "No such peer or transfer"
                RatsClient.ERR_BIND            -> "Listen/bind failed"
                RatsClient.ERR_INTERNAL        -> "Internal error"
                else                           -> "Unknown error ($errorCode)"
            }
        }
    }
}
