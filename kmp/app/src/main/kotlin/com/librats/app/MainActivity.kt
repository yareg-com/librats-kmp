package com.librats.app

import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.*
import androidx.compose.runtime.retain.retain
import androidx.compose.ui.Modifier
import com.librats.RatsBuild
import com.librats.RatsNode
import com.librats.app.ui.theme.LibratskmpTheme

class MainActivity : ComponentActivity() {
    @OptIn(ExperimentalMaterial3Api::class)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        enableEdgeToEdge()

        setContent {
            LibratskmpTheme {
                val node = retain {
                    RatsNode(8080) {
                        discovery.enableMdns()

                        peers.onConnected { peerId ->
                            Toast.makeText(
                                this@MainActivity,
                                "Connected: $peerId",
                                Toast.LENGTH_LONG
                            ).show()
                        }
                    }
                }

                val info = retain { RatsBuild }
                //val status = node.status.collectAsStateWithLifecycle()

                Scaffold(
                    modifier = Modifier.fillMaxSize(),
                    topBar = {
                        TopAppBar(
                            title = { Text("Librats") },
                            actions = {
                                OutlinedButton(onClick = { node.start() }) {
                                    Text("Start")
                                }
                            }
                        )
                    }
                ) { innerPadding ->

                    Column(modifier = Modifier.padding(innerPadding)) {

                        Text("Local ID: ${node.localId}")

                        HorizontalDivider()

                        //Text("Status: ${status.value}")

                        HorizontalDivider()

                        Text("Rats build git commit hash: ${info.gitCommitHash}")
                    }
                }
            }
        }
    }
}