package com.librats

class Config(
    val port: Int = 0,                       // 0 = ephemeral; ignored if listen is false
    val listen: Boolean = true,              // false = dial-only (no listener)
    val bindAddress: String = "",            // "" / "::" dual-stack, "0.0.0.0", or an IP literal
    val security: Security = Security.Noise, // Noise_XX by default
    val dataDirectory: String = "",          // "" = ephemeral identity; else identity.key persists
    val protocol: String = "librats/1.0",    // app id bound into the handshake; must match to connect
    val maxPeers: Long = 0                   // 0 = unlimited (guards inbound only)
)