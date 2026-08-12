package com.librats.subsystem

import com.librats.RatsClient

class Peers(
    private val ptr: () -> Long
) {

    /**
        Number of currently connected peers
    */

    val count: Long
        get() = RatsClient.nativePeerCount(ptr())

    //------------------------------------------------------------------------------------------------------------------

    /**
        Hex ids of currently connected peers
    */

    val ids: Array<String>
        get() = RatsClient.nativePeerIds(ptr())

    //------------------------------------------------------------------------------------------------------------------

    /**
        Current established peer cap (0 = unlimited)
    */

    val maxCount: Long
        get() = RatsClient.nativeMaxPeers(ptr())

    //------------------------------------------------------------------------------------------------------------------

    /**
        Caps established peers (0 = unlimited). May be called before or after start
    */

    fun setMaxCount(
        value: Long
    ) = RatsClient.nativeSetMaxPeers(
        ptr = ptr(),
        maxPeers = value
    )

    //------------------------------------------------------------------------------------------------------------------

    /**
        Register a callback invoked when a new peer is discovered via PEX,
        before the connection is attempted. Call before node start.
        @param block callback receiving peerId and list of "ip:port" addresses
     */

    fun onPeerDiscovered(
        block: (peerId: String, addresses: Array<String>) -> Unit
    ) = RatsClient.nativeOnPeerDiscovered(
        ptr = ptr(),
        callback = block
    )

    //------------------------------------------------------------------------------------------------------------------

    /**
        Sets the peer-connected callback. Call before node start
    */

    fun onConnected(
        block: (peerId: String) -> Unit
    ) = RatsClient.nativeOnPeerConnected(
        ptr = ptr(),
        callback = block
    )

    //------------------------------------------------------------------------------------------------------------------

    /**
        Sets the peer-disconnected callback. Call before node start
    */

    fun onDisconnected(
        block: (peerId: String) -> Unit
    ) = RatsClient.nativeOnPeerDisconnected(
        ptr = ptr(),
        callback = block
    )

    //------------------------------------------------------------------------------------------------------------------

    /**
        The node's public address as observed by connected peers (NAT-mapped IP:port).
        Returns null if no peer has reported our address yet.
    */

    val publicAddress: Pair<String, Int>?
        get() {
            val result = arrayOfNulls<Any?>(2)
            val err = RatsClient.nativeGetPublicAddress(ptr(), result)
            if (err != 0) return null
            val ip = result[0] as? String ?: return null
            val port = result[1] as? Int ?: return null
            return ip to port
        }

}