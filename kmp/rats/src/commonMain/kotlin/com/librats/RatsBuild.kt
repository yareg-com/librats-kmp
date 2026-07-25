package com.librats

object RatsBuild {

    val getCommitHash: String
        get() = RatsClient.nativeGitDescribe()

}