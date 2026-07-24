package com.librats

object RatsNode {

    val version: String
        get() = RatsClient.nativeGitDescribe()

}