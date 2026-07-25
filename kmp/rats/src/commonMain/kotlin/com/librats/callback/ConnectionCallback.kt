package com.librats.callback

/**
 * Callback invoked when a peer connection is established.
 * 
 * 
 * Register with [com.librats.RatsClient.setConnectionCallback] before
 * [com.librats.RatsClient.start]. Fires on an internal reactor thread; marshal to
 * the UI thread before touching views.
 */
fun interface ConnectionCallback {

    /**
     * Called when a peer handshake completes.
     * 
     * @param peerId 64-char lowercase hex of the connected peer's id
     */
    fun onConnected(peerId: String)

}
