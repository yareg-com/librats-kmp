package com.librats.callback

/**
 * Callback invoked when a transfer finishes (successfully or not).
 * 
 * 
 * Register with [com.librats.RatsClient.setFileCompleteCallback] before
 * [com.librats.RatsClient.start]. Requires the file-transfer subsystem
 * ([com.librats.RatsClient.enableFileTransfer]). Fires on an internal reactor
 * thread.
 */
fun interface FileCompleteCallback {

    /**
     * Called when a transfer terminates.
     * 
     * @param transferId unique transfer identifier
     * @param success    true if the transfer completed successfully
     * @param path       final path of the transferred file/directory (may be null)
     */
    fun onFileComplete(
        transferId: Long,
        success: Boolean,
        path: String?
    )

}
