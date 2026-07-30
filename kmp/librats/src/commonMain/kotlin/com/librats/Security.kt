package com.librats

enum class Security(
    val id: Int
) {
    Noise(RatsClient.SECURITY_NOISE),
    Plaintext(RatsClient.SECURITY_PLAINTEXT)
}