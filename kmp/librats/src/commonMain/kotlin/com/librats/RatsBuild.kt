package com.librats

object RatsBuild {

    val version: IntArray
        get() = RatsClient.nativeVersion()

    val versionString: String
        get() = RatsClient.nativeVersionString()

    val gitCommitHash: String
        get() = RatsClient.nativeGitDescribe()

    val abi: Int
        get() = RatsClient.nativeAbi()

}