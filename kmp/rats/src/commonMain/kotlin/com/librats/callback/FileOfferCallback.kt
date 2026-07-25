package com.librats.callback

/**
 * Callback invoked when a peer offers a file or directory.
 * 
 * 
 * Register with [com.librats.RatsClient.setFileOfferCallback] before
 * [com.librats.RatsClient.start]. Requires the file-transfer subsystem
 * ([com.librats.RatsClient.enableFileTransfer]). Respond by calling
 * [com.librats.RatsClient.acceptFile] or
 * [com.librats.RatsClient.rejectFile]. Fires on an internal reactor
 * thread.
 */
fun interface FileOfferCallback {

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
