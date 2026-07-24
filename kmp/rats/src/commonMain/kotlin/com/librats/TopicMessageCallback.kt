package com.librats

/**
 * Callback invoked when a pub/sub message arrives on a subscribed topic.
 * 
 * 
 * Register per-topic with [RatsClient.subscribe]
 * before [RatsClient.start]. Requires the pub/sub subsystem
 * ([RatsClient.enablePubsub]). Fires on an internal reactor thread.
 */
interface TopicMessageCallback {

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
