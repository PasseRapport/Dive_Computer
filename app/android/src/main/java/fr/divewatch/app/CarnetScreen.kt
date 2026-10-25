package fr.divewatch.app

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.selection.toggleable
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.dp
import fr.divewatch.core.*
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.util.Locale

private fun DiveRow.key()="$watchId/$diveId"

@OptIn(androidx.compose.foundation.ExperimentalFoundationApi::class)
@Composable
internal fun CarnetScreen(dives: List<DiveRow>,outings: List<OutingRow>,imports: List<ImportRow>,model: AppModel,busy: Boolean,backup: ()->Unit,restore: ()->Unit,csv: ()->Unit,category: String?,onCategory: (String?)->Unit,onOpen: (DiveRow)->Unit) {
    var selecting by rememberSaveable { mutableStateOf(false) }
    var checked by rememberSaveable { mutableStateOf(emptyList<String>()) }
    var confirm by remember { mutableStateOf<List<DiveRow>?>(null) }
    val decoded=remember(dives) { dives.associate { it.key() to DiveCodec.decode(it.content) } }
    val visible=dives.filter { decoded.getValue(it.key()).mode.name==category }
    val chosen=visible.filter { it.key() in checked }
    fun leaveSelection() { selecting=false;checked=emptyList();confirm=null }
    fun toggle(row: DiveRow) { checked=if(row.key() in checked)checked-row.key() else checked+row.key() }
    BackHandler(selecting||category!=null) { if(selecting)leaveSelection() else onCategory(null) }
    Text("Ton carnet",style=MaterialTheme.typography.headlineLarge)
    if(category==null) {
        Text("Choisis ton type de plongée",color=MaterialTheme.colorScheme.onSurfaceVariant)
        listOf(DiveMode.APNEA to "Apnée",DiveMode.AIR to "Bouteille").forEach { (mode,label) ->
            val count=decoded.values.count { it.mode==mode }
            Card(onClick={onCategory(mode.name);leaveSelection()},modifier=Modifier.fillMaxWidth().heightIn(min=112.dp).testTag("category-${mode.name}"),colors=CardDefaults.cardColors(containerColor=MaterialTheme.colorScheme.primaryContainer,contentColor=MaterialTheme.colorScheme.onPrimaryContainer)) {
                Column(Modifier.padding(22.dp),verticalArrangement=Arrangement.spacedBy(6.dp)) {
                    Text(label,style=MaterialTheme.typography.headlineMedium)
                    Text("$count plongée(s)"+(if(mode==DiveMode.AIR)" · air" else ""))
                }
            }
        }
    } else {
        Text(if(category==DiveMode.APNEA.name)"Apnée" else "Bouteille · air",style=MaterialTheme.typography.headlineMedium)
        if(selecting) {
            Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.SpaceBetween) {
                Text("${chosen.size} sélectionnée(s)")
                TextButton(onClick={confirm=chosen.toList()},enabled=!busy&&chosen.isNotEmpty()) { Text("Supprimer",color=if(!busy&&chosen.isNotEmpty())MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurfaceVariant) }
            }
            Row(Modifier.fillMaxWidth().testTag("select-all").toggleable(value=visible.isNotEmpty()&&chosen.size==visible.size,role=Role.Checkbox,onValueChange={checked=if(it)visible.map { row->row.key() } else emptyList()}).padding(vertical=8.dp),verticalAlignment=Alignment.CenterVertically) {
                Checkbox(checked=visible.isNotEmpty()&&chosen.size==visible.size,onCheckedChange=null)
                Spacer(Modifier.width(10.dp));Text("Sélectionner tous")
            }
            TextButton(onClick=::leaveSelection) { Text("Annuler la sélection") }
        } else TextButton(onClick={onCategory(null)}) { Text("‹ Catégories") }
        if(visible.isEmpty())Text("Aucune plongée dans cette catégorie. Connecte ta montre pour synchroniser tes plongées.")
        outings.forEach { outing ->
            val members=visible.filter { it.outingId==outing.id }
            if(members.isNotEmpty()) {
                Text(outing.title,style=MaterialTheme.typography.titleLarge)
                if(outing.site.isNotBlank())Text(outing.site,color=MaterialTheme.colorScheme.primary)
                members.forEach { row ->
                    val dive=decoded.getValue(row.key())
                    Card(modifier=Modifier.fillMaxWidth().testTag("dive-${row.diveId}").combinedClickable(
                        onClick={if(selecting)toggle(row) else onOpen(row)},
                        onLongClick={selecting=true;if(row.key() !in checked)checked=checked+row.key()},
                        onLongClickLabel="Sélectionner cette plongée"),
                        colors=CardDefaults.cardColors(containerColor=if(row.key() in checked)MaterialTheme.colorScheme.secondaryContainer else MaterialTheme.colorScheme.surfaceContainerLow,contentColor=MaterialTheme.colorScheme.onSurface)) {
                        Row(Modifier.padding(18.dp),verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(10.dp)) {
                            if(selecting)Checkbox(checked=row.key() in checked,onCheckedChange={toggle(row)},modifier=Modifier.testTag("check-${row.diveId}"))
                            Column(Modifier.weight(1f),verticalArrangement=Arrangement.spacedBy(6.dp)) {
                                Text(dive.startedAtMs?.let { DateTimeFormatter.ofPattern("dd/MM/yyyy HH:mm",Locale.FRANCE).withZone(ZoneId.systemDefault()).format(Instant.ofEpochMilli(it)) }?:"Date inconnue",style=MaterialTheme.typography.titleMedium)
                                if(dive.simulated)Text("SIMULATION · données fictives",color=MaterialTheme.colorScheme.tertiary,style=MaterialTheme.typography.labelMedium)
                                Text(dive.maximumDepthCm?.let { String.format(Locale.FRANCE,"%.1f m",it/100.0) }?:"Profondeur indisponible",style=MaterialTheme.typography.headlineSmall)
                                Text("${dive.durationMs/60000} min ${(dive.durationMs/1000)%60} s · ${dive.events.size} événement(s)",color=MaterialTheme.colorScheme.onSurfaceVariant)
                            }
                        }
                    }
                }
            }
        }
    }
    confirm?.let { snapshot ->
        AlertDialog(onDismissRequest={confirm=null},title={Text("Supprimer ${snapshot.size} plongée(s) ?")},
            text={Text("Ces plongées seront supprimées du carnet du téléphone. Elles restent sur la montre et pourront être réimportées par synchronisation si elles y sont encore présentes.")},
            dismissButton={TextButton(onClick={confirm=null}){Text("Annuler")}},
            confirmButton={TextButton(enabled=!busy,onClick={model.deleteDives(snapshot);leaveSelection()}){Text("Supprimer du carnet")}})
    }
    if(!selecting) {
        if(imports.isNotEmpty()) { Text("Imports à reprendre",color=MaterialTheme.colorScheme.tertiary);imports.forEach { Text("${it.diveId.takeLast(8)} · ${it.state}") };Text("Reconnecte la même montre, puis Synchroniser.") }
        OutlinedButton(onClick={model.createOuting("Nouvelle sortie")},enabled=!busy){Text("Créer une sortie")}
        Row(horizontalArrangement=Arrangement.spacedBy(8.dp)) { OutlinedButton(onClick=backup,enabled=!busy){Text("Sauvegarder")};OutlinedButton(onClick=restore,enabled=!busy){Text("Restaurer")} }
        OutlinedButton(onClick=csv,enabled=!busy&&dives.isNotEmpty()){Text("Exporter les CSV")}
    }
}
