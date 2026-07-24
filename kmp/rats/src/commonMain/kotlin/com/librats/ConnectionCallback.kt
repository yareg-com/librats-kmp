package com.librats

/**
 * Callback invoked when a peer connection is established.
 * 
 * 
 * Register with [RatsClient.setConnectionCallback] before
 * [RatsClient.start]. Fires on an internal reactor thread; marshal to
 * the UI thread before touching views.
 */
interface ConnectionCallback {

    /**
     * Called when a peer handshake completes.
     * 
     * @param peerId 64-char lowercase hex of the connected peer's id
     */
    fun onConnected(peerId: String)

}
