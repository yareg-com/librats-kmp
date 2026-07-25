package com.librats.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.*
import androidx.compose.runtime.retain.retain
import androidx.compose.ui.Modifier
import androidx.lifecycle.compose.collectAsStateWithLifecycle
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
                Scaffold(
                    modifier = Modifier.fillMaxSize(),
                    topBar = {
                        TopAppBar(title = { Text("Librats") })
                    }
                ) { innerPadding ->

                    Column(modifier = Modifier.padding(innerPadding)) {

                        val node = retain { RatsNode(8080) }
                        val info = retain { RatsBuild }

                        val ptr = node.ptr.collectAsStateWithLifecycle()
                        val localId = node.localId.collectAsStateWithLifecycle(initialValue = "No id yet")

                        Text("Node PTR: ${ptr.value}")

                        HorizontalDivider()

                        Text("Node local ID: ${localId.value}")

                        HorizontalDivider()

                        Text("Rats build git commit hash: ${info.getCommitHash}")
                    }
                }
            }
        }
    }
}