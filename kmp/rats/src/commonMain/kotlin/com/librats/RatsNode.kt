package com.librats

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.*

class RatsNode(
    port: Int = 0
) {
    val ptr: StateFlow<Long?>
        field = MutableStateFlow(null)

    @OptIn(ExperimentalCoroutinesApi::class)
    val localId: Flow<String>
        get() = ptr.filterNotNull().mapLatest {
            RatsClient.nativeLocalId(it)
        }

    init {
        ptr.update {
            RatsClient.nativeCreate(port)
        }
    }


}