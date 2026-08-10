package com.librats.callback

/**
 * Callback invoked when a new peer is discovered via PEX, before the
 * connection is attempted.
 *
 * Register with [com.librats.RatsClient.onPeerDiscovered] before
 * [com.librats.RatsClient.start]. Fires on an internal reactor thread.
 */
fun interface PeerDiscoveredCallback {

    /**
     * Called when a peer is discovered via PEX.
     *
     * @param peerId 64-char lowercase hex of the discovered peer's id
     * @param addresses list of "ip:port" strings for the discovered peer
     */
    fun onPeerDiscovered(peerId: String, addresses: Array<String>)

}
