package com.librats.callback

/**
 * Callback invoked with progress updates for an in-flight transfer.
 * 
 * 
 * Register with [com.librats.RatsClient.setFileProgressCallback] before
 * [com.librats.RatsClient.start]. Requires the file-transfer subsystem
 * ([com.librats.RatsClient.enableFileTransfer]). Fires on an internal reactor
 * thread.
 */
fun interface FileProgressCallback {

    /**
     * Called as bytes are transferred.
     * 
     * @param transferId       unique transfer identifier
     * @param peerId           64-char lowercase hex of the remote peer's id
     * @param bytesTransferred bytes moved so far
     * @param totalBytes       total bytes for the transfer
     * @param status           subsystem status code for the transfer
     */
    fun onFileProgress(
        transferId: Long,
        peerId: String,
        bytesTransferred: Long,
        totalBytes: Long,
        status: Int
    )

}
