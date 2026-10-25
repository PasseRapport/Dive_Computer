package fr.divewatch.app

import android.Manifest
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.lifecycle.viewmodel.compose.viewModel

class MainActivity: ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            val model: AppModel=viewModel()
            val permissions=androidx.activity.compose.rememberLauncherForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { granted ->
                if(granted.values.all { it })model.search() else model.report("Autorisation Bluetooth refusée. Le carnet reste disponible hors ligne.")
            }
            val backup=androidx.activity.compose.rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/zip")) { uri ->
                uri?.let { runCatching { contentResolver.openOutputStream(it)?.let(model::backup) }.onFailure { model.report(it.message?:"Impossible d’ouvrir le fichier") } }
            }
            val restore=androidx.activity.compose.rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
                uri?.let { runCatching { contentResolver.openInputStream(it)?.let(model::restore) }.onFailure { model.report(it.message?:"Impossible d’ouvrir le fichier") } }
            }
            val csv=androidx.activity.compose.rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/zip")) { uri ->
                uri?.let { runCatching { contentResolver.openOutputStream(it)?.let(model::csv) }.onFailure { model.report(it.message?:"Impossible d’ouvrir le fichier") } }
            }
            DiveWatchApp(model,
                search={ permissions.launch(if(Build.VERSION.SDK_INT>=31)arrayOf(Manifest.permission.BLUETOOTH_SCAN,Manifest.permission.BLUETOOTH_CONNECT) else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)) },
                backup={backup.launch("dive-watch-backup.zip")},restore={restore.launch(arrayOf("application/zip","application/octet-stream"))},
                csv={csv.launch("dive-watch-csv.zip")})
        }
    }
}
