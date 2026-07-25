package com.librats

class RatsNode(
    port: Int = 0,
    config: RatsNode.() -> Unit = { }
) : AutoCloseable {
    private val ptr: Long = RatsClient.nativeCreate(port).apply {
        if (this == 0L) throw RatsException("Failed to create native rats node")
    }

    init {
        config(this)
    }

    //------------------------------------------------------------------------------------------------------------------
    //
    // LIFECYCLE
    //
    //------------------------------------------------------------------------------------------------------------------

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
    // IDENTITY
    //
    //------------------------------------------------------------------------------------------------------------------

    /**
     * @return the port the node is listening on
     */

    val port: Int
        get() = RatsClient.nativeListenPort(ptr)

    /**
     * @return our self-certifying peer id as 64-char lowercase hex
     */
    val localId: String
        get() = RatsClient.nativeLocalId(ptr)

    /**
     * @return the application protocol id bound into the handshake (e.g. "librats/1.0")
     */

    val protocol: String
        get() = RatsClient.nativeProtocol(ptr)

    //------------------------------------------------------------------------------------------------------------------
    //
    // SUBSYSTEMS
    //
    //------------------------------------------------------------------------------------------------------------------

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