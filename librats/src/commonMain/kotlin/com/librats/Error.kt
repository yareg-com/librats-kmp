package com.librats

enum class Error(
    val id: Int
) {
    // Ids should match rats_error_t values

    OK(0),
    InvalidArguments(1),
    NotStarted(2),
    AlreadyStarted(3),
    NotEnabled(4),
    NoSuchPeer(5),
    Bind(6),
    Internal(7)
}