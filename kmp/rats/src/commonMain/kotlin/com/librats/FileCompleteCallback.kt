package com.librats

/**
 * Callback invoked when a transfer finishes (successfully or not).
 * 
 * 
 * Register with [RatsClient.setFileCompleteCallback] before
 * [RatsClient.start]. Requires the file-transfer subsystem
 * ([RatsClient.enableFileTransfer]). Fires on an internal reactor
 * thread.
 */
interface FileCompleteCallback {

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
        path: String? = null
    )

}
