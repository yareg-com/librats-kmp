package com.librats

/**
 * Callback invoked when a peer offers a file or directory.
 * 
 * 
 * Register with [RatsClient.setFileOfferCallback] before
 * [RatsClient.start]. Requires the file-transfer subsystem
 * ([RatsClient.enableFileTransfer]). Respond by calling
 * [RatsClient.acceptFile] or
 * [RatsClient.rejectFile]. Fires on an internal reactor
 * thread.
 */
interface FileOfferCallback {

    /**
     * Called when a peer offers a transfer.
     * 
     * @param peerId      64-char lowercase hex of the offering peer's id
     * @param transferId  unique transfer identifier (use to accept/reject)
     * @param name        the offered file or directory name
     * @param size        total size in bytes
     * @param isDirectory true if the offer is a directory tree
     */
    fun onFileOffer(
        peerId: String,
        transferId: Long,
        name: String,
        size: Long,
        isDirectory: Boolean
    )

}
