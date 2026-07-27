package com.librats.subsystem

import com.librats.RatsClient
import com.librats.callback.TopicMessageCallback

class Topics(
    private val ptr: () -> Long
) {

    /**
        Enables the pub/sub (GossipSub) subsystem. Call before node start
        @return {@link #OK} on success, otherwise a {@code rats_error_t} code
    */

    fun enable(): Int = RatsClient.nativeEnablePubsub(ptr())

    //------------------------------------------------------------------------------------------------------------------

    /**
        Subscribes to a topic; matching messages invoke the callback. Call before node start
        @return {@link #OK} on success, otherwise a {@code rats_error_t} code
    */

    fun subscribe(
        topic: String,
        callback: TopicMessageCallback
    ): Int = RatsClient.nativeSubscribe(
        ptr = ptr(),
        topic = topic,
        callback = callback
    )

    //------------------------------------------------------------------------------------------------------------------

    /**
        Unsubscribes from a topic
        @return {@link #OK} on success, otherwise a {@code rats_error_t} code
    */

    fun unsubscribe(
        topic: String
    ): Int = RatsClient.nativeUnsubscribe(
        ptr = ptr(),
        topic = topic
    )

    //------------------------------------------------------------------------------------------------------------------

    /**
        Publishes raw bytes to a topic
        @return {@link #OK} on success, otherwise a {@code rats_error_t} code
     */

    fun publish(
        topic: String,
        bytes: ByteArray
    ): Int = RatsClient.nativePublish(
        ptr = ptr(),
        topic = topic,
        data = bytes
    )

}