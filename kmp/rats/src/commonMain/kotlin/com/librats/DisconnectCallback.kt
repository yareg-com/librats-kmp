package com.librats

/**
 * Callback invoked when a peer disconnects.
 * 
 * 
 * Register with [RatsClient.setDisconnectCallback] before
 * [RatsClient.start]. Fires on an internal reactor thread.
 */
interface DisconnectCallback {

    /**
     * Called when a peer connection is torn down.
     * 
     * @param peerId 64-char lowercase hex of the disconnected peer's id
     */
    fun onDisconnected(peerId: String)

}
