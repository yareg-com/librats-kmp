package com.librats

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update

class RatsNode(
    port: Int = 0
) {
    val ptr: Long = RatsClient.nativeCreate(port).apply {
        if (this == 0L) throw RatsException("Failed to create native rats node")
    }

    val localId: String
        get() = RatsClient.nativeLocalId(ptr)


    val status: StateFlow<String>
        field = MutableStateFlow("STOPPED")

    //------------------------------------------------------------------------------------------------------------------

    fun start() = status.update {
        when (RatsClient.nativeStart(ptr)) {
            RatsClient.OK -> "ACTIVE"
            else -> "ERROR"
        }
    }

}