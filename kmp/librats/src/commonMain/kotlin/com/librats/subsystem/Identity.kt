package com.librats.subsystem

import com.librats.RatsClient

class Identity(
    private val ptr: () -> Long
) {

    /**
     * @return the port the node is listening on
     */

    val port: Int
        get() = RatsClient.nativeListenPort(ptr())

    /**
     * @return our self-certifying peer id as 64-char lowercase hex
     */
    val localId: String
        get() = RatsClient.nativeLocalId(ptr())

    /**
     * @return the application protocol id bound into the handshake (e.g. "librats/1.0")
     */

    val protocol: String
        get() = RatsClient.nativeProtocol(ptr())

}