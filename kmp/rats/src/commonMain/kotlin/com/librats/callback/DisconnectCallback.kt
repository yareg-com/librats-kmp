package com.librats.callback

/**
 * Callback invoked when a peer disconnects.
 * 
 * 
 * Register with [com.librats.RatsClient.setDisconnectCallback] before
 * [com.librats.RatsClient.start]. Fires on an internal reactor thread.
 */
fun interface DisconnectCallback {

    /**
     * Called when a peer connection is torn down.
     * 
     * @param peerId 64-char lowercase hex of the disconnected peer's id
     */
    fun onDisconnected(peerId: String)

}
