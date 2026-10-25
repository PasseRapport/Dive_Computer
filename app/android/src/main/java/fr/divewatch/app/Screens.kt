package fr.divewatch.app

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.gestures.detectTransformGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import fr.divewatch.core.*
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.util.Locale
import kotlin.math.abs

private val Ocean=Color(0xff071b24)
private val Aqua=Color(0xff67e3cf)
private val Coral=Color(0xffffb59e)
private fun depth(cm: Number?)=cm?.let { String.format(Locale.FRANCE,"%.1f m",it.toDouble()/100) }?:"Indisponible"
private fun duration(ms: Long)="${ms/60000} min ${(ms/1000)%60} s"
private fun date(ms: Long?)=ms?.let { DateTimeFormatter.ofPattern("dd MMM yyyy · HH:mm",Locale.FRANCE).withZone(ZoneId.systemDefault()).format(Instant.ofEpochMilli(it)) }?:"Date inconnue"
private fun mode(d: Dive)=if(d.mode==DiveMode.APNEA)"Apnée" else "Bouteille · air"

@Composable
fun DiveWatchApp(model: AppModel,search: ()->Unit,backup: ()->Unit,restore: ()->Unit,csv: ()->Unit) {
    val dives by model.dives.collectAsStateWithLifecycle();val outings by model.outings.collectAsStateWithLifecycle()
    val imports by model.imports.collectAsStateWithLifecycle();val connection by model.connection.collectAsStateWithLifecycle()
    val busy by model.busy.collectAsStateWithLifecycle();val message by model.message.collectAsStateWithLifecycle()
    var tab by rememberSaveable { mutableIntStateOf(0) };var selected by rememberSaveable { mutableStateOf<String?>(null) }
    val row=dives.firstOrNull { it.watchId+it.diveId==selected }
    BackHandler(selected!=null) { selected=null }
    MaterialTheme(colorScheme=darkColorScheme(primary=Aqua,secondary=Coral,background=Ocean,surface=Color(0xff102b36),surfaceVariant=Color(0xff173b47))) {
        Scaffold(bottomBar={NavigationBar { listOf("Carnet","Montre","Manuel").forEachIndexed { i,label ->
            NavigationBarItem(selected=tab==i,onClick={tab=i;selected=null},icon={Text(listOf("≋","◉","?")[i])},label={Text(label)})
        } } }) { padding ->
            Column(Modifier.fillMaxSize().padding(padding).padding(horizontal=20.dp).verticalScroll(rememberScrollState()),verticalArrangement=Arrangement.spacedBy(14.dp)) {
                Spacer(Modifier.height(10.dp));Text("DIVE WATCH",style=MaterialTheme.typography.labelLarge,color=Aqua)
                if(row!=null) Detail(row,outings,model)
                else when(tab) {
                    0 -> {
                        Text("Ton carnet",style=MaterialTheme.typography.headlineLarge)
                        Text("Apnée & bouteille à l’air · disponible hors ligne",color=MaterialTheme.colorScheme.onSurfaceVariant)
                        if(dives.isEmpty()) CardBlock { Text("La première plongée t’attend",style=MaterialTheme.typography.titleLarge);Text("Connecte le simulateur PC, ou ouvre le mode démonstration depuis Montre pour importer des plongées fictives.") }
                        outings.forEach { outing ->
                            val members=dives.filter { it.outingId==outing.id }
                            if(members.isNotEmpty()) {
                                Text(outing.title,style=MaterialTheme.typography.titleLarge)
                                if(outing.site.isNotBlank())Text(outing.site,color=Aqua)
                                members.forEach { saved ->
                                    val dive=remember(saved.hash) { DiveCodec.decode(saved.content) }
                                    Card(onClick={selected=saved.watchId+saved.diveId},modifier=Modifier.fillMaxWidth()) {
                                        Column(Modifier.padding(18.dp),verticalArrangement=Arrangement.spacedBy(6.dp)) {
                                            Text(mode(dive),style=MaterialTheme.typography.titleMedium)
                                            if(dive.simulated)Text("SIMULATION · données fictives",color=Coral,style=MaterialTheme.typography.labelMedium)
                                            Text(date(dive.startedAtMs))
                                            Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.SpaceBetween) { Text(depth(dive.maximumDepthCm),style=MaterialTheme.typography.headlineSmall);Text(duration(dive.durationMs)) }
                                            Text("${dive.events.size} événement(s)",color=Aqua)
                                        }
                                    }
                                }
                            }
                        }
                        if(imports.isNotEmpty())CardBlock { Text("Imports à reprendre",color=Coral);imports.forEach { Text("${it.diveId.takeLast(8)} · ${it.state}") };Text("Reconnecte la même montre, puis Synchroniser.") }
                        OutlinedButton(onClick={model.createOuting("Nouvelle sortie")},enabled=!busy){Text("Créer une sortie")}
                        Row(horizontalArrangement=Arrangement.spacedBy(8.dp)) { OutlinedButton(onClick=backup,enabled=!busy){Text("Sauvegarder")};OutlinedButton(onClick=restore,enabled=!busy){Text("Restaurer")} }
                        OutlinedButton(onClick=csv,enabled=!busy&&dives.isNotEmpty()){Text("Exporter les CSV")}
                    }
                    1 -> WatchScreen(model,connection,busy,search)
                    else -> {
                        Text("Manuel",style=MaterialTheme.typography.headlineLarge)
                        val context=LocalContext.current
                        val manual=remember { context.assets.open("manual.txt").bufferedReader().use { it.readText() } }
                        Text(manual,style=MaterialTheme.typography.bodyLarge)
                    }
                }
                HorizontalDivider();Text(message,style=MaterialTheme.typography.bodySmall,color=Aqua)
                Spacer(Modifier.height(16.dp))
            }
        }
    }
}
@Composable
private fun CardBlock(content: @Composable ColumnScope.()->Unit) {
    Card(Modifier.fillMaxWidth()) { Column(Modifier.padding(18.dp),verticalArrangement=Arrangement.spacedBy(10.dp),content=content) }
}
@Composable
private fun WatchScreen(model: AppModel,c: ConnectionUi,busy: Boolean,search: ()->Unit) {
    val devices by model.devices.collectAsStateWithLifecycle();val progress by model.progress.collectAsStateWithLifecycle()
    Text("Ta montre",style=MaterialTheme.typography.headlineLarge)
    Text(c.label,color=Aqua,style=MaterialTheme.typography.titleLarge)
    c.info?.let { info ->
        CardBlock {
            Text(info.model,style=MaterialTheme.typography.titleLarge)
            Text("Firmware ${info.firmware}")
            if(info.simulated)Text("SIMULATION — données fictives",color=Coral)
            Text("État : "+when(c.status?.state){0->"surface";1->"plongée en cours";else->"inconnu"})
            Text("Batterie : ${c.status?.battery?.let { "$it %" }?:"indisponible"}")
            Text("Plongées disponibles : ${c.status?.completedDives?:"inconnu"}")
            Text("Restant à importer : ${c.remaining?:"à vérifier lors de la synchronisation"}")
            Text("Dernière réception : ${date(c.receivedAt)}")
            if(c.stale)Text("Informations anciennes",color=Coral)
        }
    }
    Button(onClick=model::sync,enabled=!busy&&!c.stale&&c.info!=null,modifier=Modifier.fillMaxWidth()){Text("Synchroniser les plongées terminées")}
    if(busy){LinearProgressIndicator(progress={progress},modifier=Modifier.fillMaxWidth());OutlinedButton(onClick=model::cancelImport){Text("Interrompre l’import")}}
    OutlinedButton(onClick=model::disconnect,enabled=!busy){Text("Déconnecter")}
    HorizontalDivider()
    Text("Bluetooth réel",style=MaterialTheme.typography.titleLarge)
    Text("Démarre DiveWatch-SIM sur le PC, puis recherche-le ici.")
    Button(onClick=search,enabled=!busy){Text("Rechercher une montre")}
    devices.forEach { choice -> OutlinedButton(onClick={model.connect(choice)},enabled=!busy,modifier=Modifier.fillMaxWidth()){Text("${choice.name} · ${choice.address}")} }
    HorizontalDivider();Text("Sans périphérique",style=MaterialTheme.typography.titleLarge)
    Text("La démonstration utilise le même format de données et le même moteur d’import, sans passer par la radio Bluetooth.")
    OutlinedButton(onClick=model::connectDemo,enabled=!busy){Text("Connecter la démonstration")}
}
@Composable
private fun Detail(row: DiveRow,outings: List<OutingRow>,model: AppModel) {
    val dive=remember(row.hash) { DiveCodec.decode(row.content) }
    var cursor by remember(row.hash) { mutableLongStateOf(0L) }
    var alert by remember { mutableStateOf<DiveEvent?>(null) }
    Text(mode(dive),style=MaterialTheme.typography.headlineLarge)
    Text(date(dive.startedAtMs));if(dive.simulated)Text("SIMULATION · données fictives",color=Coral)
    Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.SpaceBetween) {
        Column { Text("PROFONDEUR MAX.",style=MaterialTheme.typography.labelSmall);Text(depth(dive.maximumDepthCm),style=MaterialTheme.typography.headlineMedium) }
        Column { Text("DURÉE",style=MaterialTheme.typography.labelSmall);Text(duration(dive.durationMs),style=MaterialTheme.typography.titleLarge) }
    }
    Text("Moyenne : ${depth(dive.meanDepthCm)}")
    DepthChart(dive,cursor,{cursor=it})
    val sample=dive.samples.minByOrNull { abs(it.elapsedMs-cursor) }
    CardBlock {
        Text("${duration(sample?.elapsedMs?:0)} · ${depth(sample?.depthCm)}",style=MaterialTheme.typography.titleLarge)
        Text("Température : "+(sample?.temperatureCentiC?.let { String.format(Locale.FRANCE,"%.1f °C",it/100.0) }?:"indisponible"))
    }
    Text("Événements enregistrés",style=MaterialTheme.typography.titleLarge)
    dive.events.forEach { event ->
        OutlinedButton(onClick={cursor=event.elapsedMs;alert=event},modifier=Modifier.fillMaxWidth()) { Text("${duration(event.elapsedMs)} · "+if(dive.simulated&&event.code==1)"Alerte de profondeur" else "Code ${event.code}") }
    }
    alert?.let { event -> AlertDialog(onDismissRequest={alert=null},confirmButton={TextButton(onClick={alert=null}){Text("Fermer")}},title={Text("Événement ${event.code}")},text={Text("Enregistré par la montre à ${duration(event.elapsedMs)}.\nNiveau : ${event.severity}\nValeur brute : ${event.value?:"indisponible"}"+(if(dive.simulated)"\nÉvénement fictif du simulateur." else ""))}) }
    val outing=outings.firstOrNull { it.id==row.outingId }
    if(outing!=null) {
        var title by remember(outing) { mutableStateOf(outing.title) };var site by remember(outing){mutableStateOf(outing.site)};var notes by remember(outing){mutableStateOf(outing.notes)}
        Text("Sortie et notes",style=MaterialTheme.typography.titleLarge)
        OutlinedTextField(title,{title=it},label={Text("Titre de la sortie")},modifier=Modifier.fillMaxWidth())
        OutlinedTextField(site,{site=it},label={Text("Site")},modifier=Modifier.fillMaxWidth())
        OutlinedTextField(notes,{notes=it},label={Text("Notes personnelles")},modifier=Modifier.fillMaxWidth(),minLines=3)
        Button(onClick={model.editOuting(outing.copy(title=title,site=site,notes=notes))}){Text("Enregistrer les notes")}
        if(outings.size>1) {
            var expand by remember{mutableStateOf(false)}
            Box { OutlinedButton(onClick={expand=true}){Text("Déplacer vers une autre sortie")};DropdownMenu(expand,{expand=false}) { outings.filter{it.id!=outing.id}.forEach { target -> DropdownMenuItem(text={Text(target.title)},onClick={model.moveDive(row,target.id);expand=false}) } } }
        }
    }
}
@Composable
private fun DepthChart(dive: Dive,cursor: Long,onCursor: (Long)->Unit) {
    var zoom by remember(dive.id){mutableFloatStateOf(1f)};var pan by remember(dive.id){mutableFloatStateOf(0f)}
    Row(horizontalArrangement=Arrangement.spacedBy(8.dp)) { OutlinedButton(onClick={zoom=(zoom/1.5f).coerceAtLeast(1f);pan=0f}){Text("−")};OutlinedButton(onClick={zoom=(zoom*1.5f).coerceAtMost(20f)}){Text("+")};TextButton(onClick={zoom=1f;pan=0f}){Text("Vue complète")} }
    Canvas(Modifier.fillMaxWidth().height(260.dp).semantics { contentDescription="Profil de profondeur. Utilise le zoom et touche la courbe pour lire une mesure." }
        .pointerInput(dive.id,zoom,pan) { detectTapGestures { p -> val w=size.width-64.dp.toPx();val ratio=((p.x-44.dp.toPx())/w/zoom+pan).coerceIn(0f,1f);onCursor((ratio*dive.durationMs).toLong()) } }
        .pointerInput(dive.id) { detectTransformGestures { _,movement,z,_ -> zoom=(zoom*z).coerceIn(1f,20f);pan=(pan-movement.x/(size.width*zoom)).coerceIn(0f,1f-1f/zoom) } }) {
        val left=44.dp.toPx();val top=20.dp.toPx();val w=size.width-left-20.dp.toPx();val h=size.height-top-30.dp.toPx()
        val max=((dive.maximumDepthCm?:100).coerceAtLeast(100)*1.1).toFloat()
        fun x(t: Long)=left+(t.toFloat()/dive.durationMs-pan)*zoom*w
        fun y(d: Int)=top+d/max*h
        val paint=android.graphics.Paint().apply { color=android.graphics.Color.LTGRAY;textSize=11.dp.toPx();isAntiAlias=true }
        repeat(5) { i -> val yy=top+h*i/4;drawLine(Color(0xff315461),Offset(left,yy),Offset(left+w,yy),1f);drawContext.canvas.nativeCanvas.drawText(String.format(Locale.FRANCE,"%.0f",max*i/400),0f,yy+4.dp.toPx(),paint) }
        drawContext.canvas.nativeCanvas.drawText("m",0f,top-7.dp.toPx(),paint)
        val path=Path();var previous: Sample?=null
        dive.samples.forEach { s ->
            val xx=x(s.elapsedMs);val depthValue=s.depthCm
            if(depthValue==null||xx<left||xx>left+w)previous=null
            else { val yy=y(depthValue);if(previous==null||s.elapsedMs-previous!!.elapsedMs>dive.intervalMs*1.5)path.moveTo(xx,yy) else path.lineTo(xx,yy);previous=s }
        }
        drawPath(path,Aqua,style=Stroke(2.dp.toPx()))
        dive.events.forEach { e -> val s=dive.samples.minByOrNull{abs(it.elapsedMs-e.elapsedMs)};val xx=x(e.elapsedMs);s?.depthCm?.let { depthValue -> if(xx in left..left+w)drawCircle(Coral,5.dp.toPx(),Offset(xx,y(depthValue))) } }
        val xx=x(cursor);if(xx in left..left+w)drawLine(Color.White,Offset(xx,top),Offset(xx,top+h),1.dp.toPx())
        drawContext.canvas.nativeCanvas.drawText(duration((pan*dive.durationMs).toLong()),left,size.height-4.dp.toPx(),paint)
        drawContext.canvas.nativeCanvas.drawText(duration(((pan+1f/zoom)*dive.durationMs).toLong()),(left+w-85.dp.toPx()).coerceAtLeast(left),size.height-4.dp.toPx(),paint)
    }
}
