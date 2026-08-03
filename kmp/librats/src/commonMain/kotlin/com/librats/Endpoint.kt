package com.librats

import com.librats.subsystem.Discovery
import com.librats.subsystem.Identity
import com.librats.subsystem.Message
import com.librats.subsystem.Peers
import com.librats.subsystem.Topics
import kotlin.Int

class Endpoint(
    options: EndpointOptions,
    setup: Endpoint.() -> Unit = { }
) : AutoCloseable {

    private val ptr: Long = create(options).apply {
        if (this == 0L) throw RatsException("Failed to create native rats node")
    }

    init {
        setup(this)
    }

    //------------------------------------------------------------------------------------------------------------------
    //
    // LIFECYCLE
    //
    //------------------------------------------------------------------------------------------------------------------

    fun create(
        options: EndpointOptions
    ): Long = options.run {
        RatsClient.nativeCreateConfig(
            listenPort = port,
            enableListen = listen,
            bindAddress = bindAddress,
            security = security.id,
            dataDir = dataDirectory,
            protocol = protocol,
            maxPeers = maxPeers
        )
    }

    /**
     * Starts the node: binds the listener and brings up enabled subsystems
     *
     * @return {@link #OK} on success, otherwise a {@code rats_error_t} code ({@link #ERR_ALREADY_STARTED},
     * {@link #ERR_BIND})
     */

    fun start(): Int = RatsClient.nativeStart(ptr)

    /**
     * Stops the node and closes all connections
     */

    fun stop(): Unit = RatsClient.nativeStop(ptr)

    /**
     * Destroys the node and releases all native resources
     */

    fun destroy(): Unit = RatsClient.nativeDestroy(ptr)

    override fun close() = destroy()

    //------------------------------------------------------------------------------------------------------------------
    //
    // SUBSYSTEMS
    //
    //------------------------------------------------------------------------------------------------------------------

    val identity: Identity
        get() = Identity { ptr }

    val discovery: Discovery
        get() = Discovery { ptr }

    val peers: Peers
        get() = Peers { ptr }

    val message: Message
        get() = Message { ptr }

    val topics: Topics
        get() = Topics { ptr }

    /**
     * Dials a peer at host:port.
     *
     * @return {@link #OK} on success, otherwise a {@code rats_error_t} code
     */

    fun connect(
        host: String,
        port: Int
    ): Int = RatsClient.nativeConnect(
        ptr = ptr,
        host = host,
        port = port
    )

}