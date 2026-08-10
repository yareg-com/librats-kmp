package com.librats.subsystem

import com.librats.RatsClient
import com.librats.callback.PeerDiscoveredCallback

class Discovery(
    private val ptr: () -> Long
) {

    /**
        Enables DHT discovery. Call before node start
        @param port - DHT port (0 = ephemeral)
        @param key  - app namespace key (null = default)
        @return {@link #OK} on success, otherwise a {@code rats_error_t} code
    */

    fun enableDht(
        port: Int = 0,
        key: String? = null,
        bootstrapNodes: List<String>? = null, // Custom bootstrap nodes list, default used if null
        stunServers: List<String>? = null     // Custom STUN servers list, default used if null
    ): Int = RatsClient.nativeEnableDht(
        ptr = ptr(),
        dhtPort = port,
        discoveryKey = key,
        bootstrapNodes = bootstrapNodes?.toTypedArray(),
        stunServers = stunServers?.toTypedArray()
    )

    //------------------------------------------------------------------------------------------------------------------

    /**
        Enables local-network mDNS discovery. Call before node start
        @return {@link #OK} on success, otherwise a {@code rats_error_t} code
    */

    fun enableMdns(): Int = RatsClient.nativeEnableMdns(ptr())

    //------------------------------------------------------------------------------------------------------------------

    /**
        Enables automatic NAT port forwarding for the listen port. Call before node start
        @return {@link #OK} on success, otherwise a {@code rats_error_t} code
    */

    fun enablePortMapping(
        upnp: Boolean,
        natPmp: Boolean
    ): Int = RatsClient.nativeEnablePortMapping(
        ptr = ptr(),
        enableUpnp = upnp,
        enableNatpmp = natPmp
    )

    //------------------------------------------------------------------------------------------------------------------

    /**
        Enables the PeerExchange (PEX) subsystem. Call before node start.
        @param publicOnly if true, only share globally-routable (non-LAN) addresses
        @return {@link #OK} on success, otherwise a {@code rats_error_t} code
    */

    fun enablePex(
        publicOnly: Boolean = false
    ): Int = RatsClient.nativeEnablePex(
        ptr = ptr(),
        publicOnly = publicOnly
    )

    //------------------------------------------------------------------------------------------------------------------

    /**
        Enable STUN probing: on start the node probes STUN servers to discover its
        public address, which is then shared via identify and PEX.
        @param servers STUN servers as "host:port" strings, or null for built-in defaults.
        @return {@link #OK} on success, otherwise a {@code rats_error_t} code
    */

    fun enableStun(
        servers: List<String>? = null
    ): Int = RatsClient.nativeEnableStun(
        ptr = ptr(),
        servers = servers?.toTypedArray()
    )

    //------------------------------------------------------------------------------------------------------------------

    /**
        Explicitly ask every connected peer for its known peer list.
        Useful when the automatic on-connect request missed peers.
        @return {@link #OK} on success, otherwise a {@code rats_error_t} code
    */

    fun requestPeers(): Int = RatsClient.nativeRequestPeers(ptr())

    //------------------------------------------------------------------------------------------------------------------

    /**
        Register a callback invoked when a new peer is discovered via PEX,
        before the connection is attempted. Call before node start.
        @param block callback receiving peerId and list of "ip:port" addresses
    */

    fun onPeerDiscovered(
        block: (peerId: String, addresses: List<String>) -> Unit
    ) = RatsClient.nativeOnPeerDiscovered(
        ptr = ptr(),
        callback = { peerId, addresses ->
            block(peerId, addresses.toList())
        }
    )

}