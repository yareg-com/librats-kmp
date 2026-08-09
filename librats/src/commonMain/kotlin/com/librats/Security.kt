package com.librats

enum class Security(
    val id: Int
) {
    // Ids should match rats_security_t values

    Noise(0),
    Plaintext(1)
}