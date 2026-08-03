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
        this.errorCode = Error.Internal.id
    }

    constructor(errorCode: Int) : super(getErrorMessage(errorCode)) {
        this.errorCode = errorCode
    }

    constructor(message: String?, errorCode: Int) : super(message) {
        this.errorCode = errorCode
    }

    constructor(message: String?, cause: Throwable?) : super(message, cause) {
        this.errorCode = Error.Internal.id
    }

    companion object {
        /** Human-readable name for a `rats_error_t` value.  */
        fun getErrorMessage(errorCode: Int): String {
            return when (errorCode) {
                Error.OK.id                  -> "OK"
                Error.InvalidArguments.id     -> "Invalid argument"
                Error.NotStarted.id -> "Node not started"
                Error.AlreadyStarted.id -> "Node already started"
                Error.NotEnabled.id -> "Subsystem not enabled"
                Error.NoSuchPeer.id -> "No such peer or transfer"
                Error.Bind.id -> "Listen/bind failed"
                Error.Internal.id -> "Internal error"
                else                           -> "Unknown error ($errorCode)"
            }
        }
    }
}
