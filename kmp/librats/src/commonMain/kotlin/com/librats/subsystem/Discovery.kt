package com.librats.subsystem

import com.librats.RatsClient

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
        @return {@link #OK} on success, otherwise a {@code rats_error_t} code
    */

    fun enablePex(): Int = RatsClient.nativeEnablePex(ptr())

}