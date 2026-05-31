package com.sylvania.launcher.ui

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.core.content.ContextCompat
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

class MainActivity : ComponentActivity() {

    private val viewModel: LauncherViewModel by viewModels()

    private val requestStorage =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) { /* refreshed onResume */ }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // The WoW client lives under shared storage (targetSdk 28 = legacy access).
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
            != PackageManager.PERMISSION_GRANTED
        ) {
            requestStorage.launch(Manifest.permission.WRITE_EXTERNAL_STORAGE)
        }

        setContent {
            MaterialTheme(colorScheme = darkColorScheme(primary = Color(0xFF7EC8E3))) {
                Surface(modifier = Modifier.fillMaxSize()) {
                    LauncherScreen(viewModel)
                }
            }
        }
    }

    override fun onResume() {
        super.onResume()
        viewModel.refresh()
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun LauncherScreen(vm: LauncherViewModel) {
    val state by vm.ui.collectAsState()
    var pathInput by remember { mutableStateOf(state.wowPath.ifBlank { vm.suggestedClientPath }) }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Text("Sylvania Launcher", fontSize = 28.sp, fontWeight = FontWeight.Bold)
        Text(
            "World of Warcraft 3.3.5a — port Android (logique launcher)",
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )

        Card(modifier = Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Statut", fontWeight = FontWeight.SemiBold)
                Text(state.status)
                if (state.activeRealm.isNotBlank()) Text("Realmlist : ${state.activeRealm}")
                Text("Patch HD : ${if (state.hdInstalled) "installé" else "non installé"}")
                if (state.runtimeStatus.isNotBlank()) Text(state.runtimeStatus)
            }
        }

        OutlinedTextField(
            value = pathInput,
            onValueChange = { pathInput = it },
            label = { Text("Dossier du client (contenant Wow.exe)") },
            modifier = Modifier.fillMaxWidth(),
            singleLine = true,
        )
        OutlinedButton(onClick = { vm.setWowPath(pathInput) }, modifier = Modifier.fillMaxWidth()) {
            Text("Enregistrer le chemin du client")
        }

        state.patchProgress?.let { p ->
            if (p.percent in 0..100) {
                LinearProgressIndicator(progress = { p.percent / 100f }, modifier = Modifier.fillMaxWidth())
            } else {
                LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
            }
        }

        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Button(
                onClick = { vm.play() },
                enabled = state.clientReady,
                modifier = Modifier.weight(1f),
            ) { Text("JOUER") }

            OutlinedButton(
                onClick = { vm.installHdPatch() },
                enabled = state.clientReady && state.patchProgress == null,
                modifier = Modifier.weight(1f),
            ) { Text("Patch HD") }
        }
    }
}
