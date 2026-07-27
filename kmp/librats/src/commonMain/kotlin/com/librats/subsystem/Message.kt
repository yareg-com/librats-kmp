package com.librats.subsystem

import com.librats.RatsClient
import com.librats.callback.MessageCallback

class Message(
    private val ptr: () -> Long
) {

    /**
        Sends raw bytes to a specific peer over a named channel
        @return {@link #OK} on success, otherwise a {@code rats_error_t} code
    */

    fun send(
        peerId: String,
        channel: String,
        bytes: ByteArray
    ): Int = RatsClient.nativeSend(
        ptr = ptr(),
        peerId = peerId,
        channel = channel,
        data = bytes
    )

    //------------------------------------------------------------------------------------------------------------------

    /**
        Broadcasts raw bytes to all connected peers over a named channel
        @return {@link #OK} on success, otherwise a {@code rats_error_t} code
    */

    fun broadcast(
        channel: String,
        bytes: ByteArray
    ): Int = RatsClient.nativeBroadcast(
        ptr = ptr(),
        channel = channel,
        data = bytes
    )

    //------------------------------------------------------------------------------------------------------------------

    /**
        Registers a handler for raw messages on a channel. Call before start
        @return {@link #OK} on success, otherwise a {@code rats_error_t} code
    */

    fun onReceived(
        channel: String,
        block: MessageCallback
    ): Int = RatsClient.nativeOn(
        ptr = ptr(),
        channel = channel,
        callback = block
    )

}