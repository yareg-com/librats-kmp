package com.librats

/**
 * Callback invoked when a typed JSON message of a registered type arrives.
 * 
 * 
 * Register per-type with [RatsClient.onJson]
 * (or [RatsClient.onceJson]) before [RatsClient.start]. Requires
 * the JSON subsystem ([RatsClient.enableJson]). Fires on an internal
 * reactor thread.
 */
interface JsonMessageCallback {

    /**
     * Called when a JSON message of the registered type is received.
     * 
     * @param peerId 64-char lowercase hex of the sending peer's id
     * @param json   compact JSON text payload
     */
    fun onJsonMessage(
        peerId: String,
        json: String
    )

}
