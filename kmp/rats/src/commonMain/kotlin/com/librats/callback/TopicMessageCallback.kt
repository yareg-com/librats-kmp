package com.librats.callback

/**
 * Callback invoked when a pub/sub message arrives on a subscribed topic.
 * 
 * 
 * Register per-topic with [com.librats.RatsClient.subscribe]
 * before [com.librats.RatsClient.start]. Requires the pub/sub subsystem
 * ([com.librats.RatsClient.enablePubsub]). Fires on an internal reactor thread.
 */
fun interface TopicMessageCallback {

    /**
     * Called when a message is published to a subscribed topic.
     * 
     * @param peerId 64-char lowercase hex of the publishing peer's id
     * @param topic  the topic the message was published on
     * @param data   the raw message bytes
     */
    fun onTopicMessage(
        peerId: String,
        topic: String,
        data: ByteArray
    )

}
