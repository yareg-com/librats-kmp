package com.librats.callback

/**
 * Callback invoked when a typed JSON message of a registered type arrives.
 * 
 * 
 * Register per-type with [com.librats.RatsClient.onJson]
 * (or [com.librats.RatsClient.onceJson]) before [com.librats.RatsClient.start]. Requires
 * the JSON subsystem ([com.librats.RatsClient.enableJson]). Fires on an internal
 * reactor thread.
 */
fun interface JsonMessageCallback {

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
