package com.librats

/**
 * Callback invoked when raw bytes arrive on a named application channel.
 * 
 * 
 * Register per-channel with [RatsClient.on]
 * before [RatsClient.start]. Fires on an internal reactor thread.
 */
interface MessageCallback {

    /**
     * Called when a message is received on the channel this callback was
     * registered for.
     * 
     * @param peerId 64-char lowercase hex of the sending peer's id
     * @param data   the raw message bytes
     */
    fun onMessage(
        peerId: String,
        data: ByteArray
    )

}
