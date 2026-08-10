package com.librats

import com.librats.callback.ConnectionCallback
import com.librats.callback.DisconnectCallback
import com.librats.callback.FileCompleteCallback
import com.librats.callback.FileOfferCallback
import com.librats.callback.FileProgressCallback
import com.librats.callback.PeerDiscoveredCallback
import com.librats.callback.JsonMessageCallback
import com.librats.callback.MessageCallback
import com.librats.callback.TopicMessageCallback

object RatsClient {

    init {
        loadLibrary()
    }

    //------------------------------------------------------------------------------------------------------------------
    //
    // LIFECYCLE
    //
    //------------------------------------------------------------------------------------------------------------------

    external fun nativeCreateConfig(
        listenPort: Int,
        enableListen: Boolean,
        bindAddress: String?,
        security: Int,
        dataDir: String?,
        protocol: String?,
        maxPeers: Long
    ): Long

    external fun nativeCreate(listenPort: Int): Long
    external fun nativeDestroy(ptr: Long)
    external fun nativeStart(ptr: Long): Int
    external fun nativeStop(ptr: Long)

    //------------------------------------------------------------------------------------------------------------------
    //
    // IDENTITY
    //
    //------------------------------------------------------------------------------------------------------------------

    external fun nativeListenPort(ptr: Long): Int
    external fun nativeLocalId(ptr: Long): String
    external fun nativeProtocol(ptr: Long): String

    //------------------------------------------------------------------------------------------------------------------
    //
    // CONNECTIVITY
    //
    //------------------------------------------------------------------------------------------------------------------

    external fun nativeConnect(ptr: Long, host: String, port: Int): Int

    external fun nativeEnablePing(ptr: Long): Int
    external fun nativePeerRttMs(ptr: Long, peerId: String): Long

    external fun nativeEnableReconnect(ptr: Long): Int
    external fun nativeAddReconnect(ptr: Long, host: String, port: Int): Int
    external fun nativeRemoveReconnect(ptr: Long, host: String, port: Int): Int

    //------------------------------------------------------------------------------------------------------------------
    //
    // DISCOVERY
    //
    //------------------------------------------------------------------------------------------------------------------

    external fun nativeEnableDht(
        ptr: Long,
        dhtPort: Int,
        discoveryKey: String?,
        bootstrapNodes: Array<String>?,
        stunServers: Array<String>?
    ): Int
    external fun nativeEnableMdns(ptr: Long): Int
    external fun nativeEnablePortMapping(ptr: Long, enableUpnp: Boolean, enableNatpmp: Boolean): Int
    external fun nativeEnablePex(ptr: Long, publicOnly: Boolean): Int
    external fun nativeEnableStun(ptr: Long, servers: Array<String>?): Int
    external fun nativeRequestPeers(ptr: Long): Int
    external fun nativeOnPeerDiscovered(ptr: Long, callback: PeerDiscoveredCallback): Int

    //------------------------------------------------------------------------------------------------------------------
    //
    // PEERS
    //
    //------------------------------------------------------------------------------------------------------------------

    external fun nativePeerCount(ptr: Long): Long
    external fun nativePeerIds(ptr: Long): Array<String>
    external fun nativeMaxPeers(ptr: Long): Long
    external fun nativeSetMaxPeers(ptr: Long, maxPeers: Long)
    external fun nativeOnPeerConnected(ptr: Long, callback: ConnectionCallback): Int
    external fun nativeOnPeerDisconnected(ptr: Long, callback: DisconnectCallback): Int
    external fun nativeGetPublicAddress(ptr: Long, result: Array<Any?>): Int

    //------------------------------------------------------------------------------------------------------------------
    //
    // BINARY MESSAGES
    //
    //------------------------------------------------------------------------------------------------------------------

    external fun nativeSend(ptr: Long, peerId: String, channel: String, data: ByteArray): Int
    external fun nativeBroadcast(ptr: Long, channel: String, data: ByteArray): Int
    external fun nativeOn(ptr: Long, channel: String, callback: MessageCallback): Int

    //------------------------------------------------------------------------------------------------------------------
    //
    // JSON MESSAGES
    //
    //------------------------------------------------------------------------------------------------------------------

    external fun nativeEnableJson(ptr: Long): Int
    external fun nativeOnJson(ptr: Long, type: String, callback: JsonMessageCallback): Int
    external fun nativeOnceJson(ptr: Long, type: String, callback: JsonMessageCallback): Int
    external fun nativeOffJson(ptr: Long, type: String): Int
    external fun nativeSendJson(ptr: Long, peerId: String, type: String, json: String): Int
    external fun nativeBroadcastJson(ptr: Long, type: String, json: String): Int

    //------------------------------------------------------------------------------------------------------------------
    //
    // TOPICS
    //
    //------------------------------------------------------------------------------------------------------------------

    external fun nativeEnablePubsub(ptr: Long): Int
    external fun nativeSubscribe(ptr: Long, topic: String, callback: TopicMessageCallback?): Int
    external fun nativeUnsubscribe(ptr: Long, topic: String): Int
    external fun nativePublish(ptr: Long, topic: String?, data: ByteArray): Int

    //------------------------------------------------------------------------------------------------------------------
    //
    // FILE TRANSFERS
    //
    //------------------------------------------------------------------------------------------------------------------

    external fun nativeEnableFileTransfer(ptr: Long, tempDir: String): Int
    external fun nativeOnFileOffer(ptr: Long, callback: FileOfferCallback): Int
    external fun nativeOnFileProgress(ptr: Long, callback: FileProgressCallback): Int
    external fun nativeOnFileComplete(ptr: Long, callback: FileCompleteCallback): Int
    external fun nativeSendFile(ptr: Long, peerId: String, path: String): Long
    external fun nativeSendDirectory(ptr: Long, peerId: String, dirPath: String): Long
    external fun nativeAcceptFile(ptr: Long, peerId: String, transferId: Long, destPath: String): Int
    external fun nativeRejectFile(ptr: Long, peerId: String, transferId: Long): Int
    external fun nativeCancelFile(ptr: Long, peerId: String, transferId: Long): Int
    external fun nativePauseFile(ptr: Long, peerId: String, transferId: Long): Int
    external fun nativeResumeFile(ptr: Long, peerId: String, transferId: Long): Int

    //------------------------------------------------------------------------------------------------------------------
    //
    // LOGGING
    //
    //------------------------------------------------------------------------------------------------------------------

    external fun nativeSetLogLevel(level: Int)
    external fun nativeSetLogFile(path: String)
    external fun nativeErrorStr(error: Int): String

    //------------------------------------------------------------------------------------------------------------------
    //
    // BUILD INFO
    //
    //------------------------------------------------------------------------------------------------------------------

    external fun nativeVersionString(): String
    external fun nativeVersion(): IntArray
    external fun nativeGitDescribe(): String
    external fun nativeAbi(): Int

}