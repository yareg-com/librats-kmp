package com.librats

import com.librats.callback.ConnectionCallback
import com.librats.callback.DisconnectCallback

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
        Sets the peer-connected callback. Call before node start
    */

    fun onConnected(
        block: ConnectionCallback
    ) = RatsClient.nativeOnPeerConnected(
        ptr = ptr(),
        callback = block
    )

    //------------------------------------------------------------------------------------------------------------------

    /**
        Sets the peer-disconnected callback. Call before node start
    */

    fun onDisconnected(
        block: DisconnectCallback
    ) = RatsClient.nativeOnPeerDisconnected(
        ptr = ptr(),
        callback = block
    )

}