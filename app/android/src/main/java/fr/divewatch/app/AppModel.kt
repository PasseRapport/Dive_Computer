package fr.divewatch.app

import android.annotation.SuppressLint
import android.app.Application
import android.bluetooth.BluetoothManager
import android.bluetooth.le.*
import android.location.LocationManager
import android.os.Build
import android.os.ParcelUuid
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import androidx.room.Room
import androidx.room.withTransaction
import fr.divewatch.core.*
import java.io.File
import java.util.UUID
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.*

data class DeviceChoice(val name: String,val address: String)
data class ConnectionUi(val label: String="Déconnectée",val info: WatchInfo?=null,val status: WatchStatus?=null,val receivedAt: Long?=null,val stale: Boolean=true,val remaining: Int?=null,val canErase: Boolean=false)
@SuppressLint("MissingPermission")
class AppModel(application: Application): AndroidViewModel(application) {
    private val settings=application.getSharedPreferences("appearance",android.content.Context.MODE_PRIVATE)
    val theme=MutableStateFlow(AppTheme.entries.firstOrNull { it.name==settings.getString("theme",null) }?:AppTheme.NIGHT)
    fun setTheme(value: AppTheme) { settings.edit().putString("theme",value.name).apply();theme.value=value }
    val db=Room.databaseBuilder(application,CarnetDatabase::class.java,"carnet.db").build()
    val dao=db.carnet()
    val dives=dao.observeDives().stateIn(viewModelScope,SharingStarted.WhileSubscribed(5000),emptyList())
    val outings=dao.observeOutings().stateIn(viewModelScope,SharingStarted.WhileSubscribed(5000),emptyList())
    val imports=dao.observeImports().stateIn(viewModelScope,SharingStarted.WhileSubscribed(5000),emptyList())
    val connection=MutableStateFlow(ConnectionUi())
    val devices=MutableStateFlow<List<DeviceChoice>>(emptyList())
    val busy=MutableStateFlow(false)
    val scanning=MutableStateFlow(false)
    val importing=MutableStateFlow(false)
    val eraseConfirmation=MutableStateFlow<ErasePlan?>(null)
    val message=MutableStateFlow("Prêt. Recherche ta montre pour te connecter.")
    val progress=MutableStateFlow(0f)
    private val adapter get()=getApplication<Application>().getSystemService(BluetoothManager::class.java)?.adapter
    private var real: NordicLink?=null
    private var client: WatchClient?=null
    private var importJob: Job?=null
    private var pollJob: Job?=null
    private var scan: ScanCallback?=null
    private var scanJob: Job?=null
    private val store=BlockStore(File(application.filesDir,"blocks"))
    private val downloads=File(application.filesDir,"blocks")
    fun report(text: String) { message.value=text }
    fun search() {
        if(busy.value)return
        stopScan();devices.value=emptyList()
        val bluetooth=adapter
        if(bluetooth==null||!bluetooth.isEnabled) { report("Active le Bluetooth du téléphone.");return }
        if(Build.VERSION.SDK_INT<=30) {
            val location=getApplication<Application>().getSystemService(LocationManager::class.java)
            val enabled=if(Build.VERSION.SDK_INT>=28) location?.isLocationEnabled==true else
                location?.let { it.isProviderEnabled(LocationManager.GPS_PROVIDER)||it.isProviderEnabled(LocationManager.NETWORK_PROVIDER) }==true
            if(!enabled) { report("Active la localisation du téléphone pour rechercher une montre sur Android 11 ou antérieur.");return }
        }
        val scanner=bluetooth.bluetoothLeScanner
        if(scanner==null) { report("Recherche Bluetooth indisponible. Vérifie que le Bluetooth est activé.");return }
        scan=object: ScanCallback() {
            override fun onScanResult(type: Int,result: ScanResult) {
                if(scan!==this)return
                val item=DeviceChoice(result.scanRecord?.deviceName?:result.device.name?:"Montre NUS",result.device.address)
                devices.update { previous -> (previous.filter { it.address!=item.address }+item).sortedBy { it.name } }
            }
            override fun onScanFailed(errorCode: Int) {
                if(scan!==this)return
                stopScan();report("Recherche Bluetooth impossible ($errorCode)")
            }
        }
        report("Recherche pendant 15 secondes…")
        scanning.value=true
        try {
            scanner.startScan(listOf(ScanFilter.Builder().setServiceUuid(ParcelUuid(Nus.SERVICE)).build()),ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(),scan)
            scanJob=viewModelScope.launch {
                delay(15000)
                scanJob=null;stopScan()
                report(if(devices.value.isEmpty()) "Aucune montre trouvée. Vérifie que DiveWatch-SIM est lancé sur le PC, puis relance la recherche." else "Recherche terminée. Sélectionne ta montre.")
            }
        } catch(e: SecurityException) { stopScan();report("Autorise l’accès Bluetooth et à la localisation dans les paramètres de l’application.") }
    }
    fun stopScan() { scanning.value=false;scanJob?.cancel();scanJob=null;scan?.let { runCatching { adapter?.bluetoothLeScanner?.stopScan(it) } };scan=null }
    fun connect(choice: DeviceChoice)=connectUsing {
        NordicLink(getApplication()) {
            eraseConfirmation.value=null
            connection.update { it.copy(label="Déconnectée",stale=true) }
        }.also { real=it;it.open(requireNotNull(adapter).getRemoteDevice(choice.address)) }
    }
    internal fun connectForTest(link: WatchLink)=connectUsing { link }
    private fun connectUsing(open: suspend ()->WatchLink) {
        if(busy.value)return
        disconnect();stopScan();busy.value=true;connection.value=ConnectionUi(label="Connexion…")
        viewModelScope.launch {
            try {
                val link=open()
                client=WatchClient(link)
                val info=withContext(Dispatchers.IO) { client!!.hello() }
                connection.value=ConnectionUi(label="Connectée",info=info)
                refresh()
                withContext(Dispatchers.IO) {
                    val catalogue=client!!.catalog()
                    val remaining=catalogue.count { dao.find(info.id.toString(),it.id.toString())?.hash!=it.hash.hex() }
                    connection.update { it.copy(remaining=remaining) }
                    val canErase=client!!.supportsErase()
                    connection.update { it.copy(canErase=canErase) }
                }
                pollJob=viewModelScope.launch { while(isActive) { delay(5000);if(!busy.value)runCatching { refresh() }.onFailure { connection.update { c -> c.copy(stale=true) } } } }
                report(if(info.simulated)"Montre simulée connectée — données fictives" else "Montre connectée")
            } catch(e: Exception) { disconnect();report(e.message?:"Échec de connexion") }
            finally { busy.value=false }
        }
    }
    private suspend fun refresh() {
        val c=client?:return
        val status=withContext(Dispatchers.IO) { c.status() }
        connection.update { it.copy(status=status,receivedAt=System.currentTimeMillis(),stale=false) }
    }
    fun disconnect() {
        eraseConfirmation.value=null
        importJob?.cancel();pollJob?.cancel();real?.shutdown();real=null;client=null
        connection.update { it.copy(label="Déconnectée",stale=true) }
    }
    fun sync() {
        val c=client?:return
        val info=connection.value.info?:return
        if(busy.value)return
        busy.value=true;importing.value=true
        importJob=viewModelScope.launch {
            try {
                withContext(Dispatchers.IO) {
                    val manifests=c.catalog()
                    val missing=manifests.count { dao.find(info.id.toString(),it.id.toString())?.hash!=it.hash.hex() }
                    connection.update { it.copy(remaining=missing) }
                    var imported=0
                    val outing=OutingRow(UUID.randomUUID().toString(),"Sortie importée")
                    for((index,entry) in manifests.withIndex()) {
                        ensureActive()
                        val existing=dao.find(info.id.toString(),entry.id.toString())
                        if(existing!=null) {
                            check(existing.hash==entry.hash.hex()) { "Conflit : le contenu d’une plongée déjà importée a changé. Original conservé." }
                            continue
                        }
                        val manifest=c.manifest(entry.id)
                        check(manifest.hash.contentEquals(entry.hash)) { "Catalogue modifié : relancer l’import" }
                        fun row(state: String,bytes: Int)=ImportRow(info.id.toString(),entry.id.toString(),manifest.hash.hex(),bytes,manifest.size,state)
                        dao.saveImport(row("incomplet",store.nextOffset(manifest)))
                        val dive=Importer(c,store).download(info,manifest) { done,total ->
                            progress.value=(index+done.toFloat()/total)/manifests.size
                            dao.saveImport(row("incomplet",done))
                        }
                        val blob=store.complete(manifest)
                        db.withTransaction {
                            if(imported==0)dao.saveOuting(outing)
                            dao.insertDive(DiveRow(dive.watchId.toString(),dive.id.toString(),manifest.hash.hex(),blob,outing.id))
                            dao.saveImport(row("importé",manifest.size))
                        }
                        store.remove(manifest);imported++
                        connection.update { it.copy(remaining=missing-imported) }
                    }
                    report("$imported plongée(s) importée(s). Carnet disponible hors ligne.")
                }
            } catch(e: CancellationException) { report("Import interrompu. Les blocs vérifiés sont conservés.");throw e }
            catch(e: Exception) { report(e.message?:"Import interrompu — reprise disponible") }
            finally { importing.value=false;busy.value=false;progress.value=0f }
        }
    }
    fun prepareErase() {
        val c=client?:return
        val info=connection.value.info?:return
        if(busy.value||connection.value.stale||!connection.value.canErase)return
        busy.value=true;stopScan()
        viewModelScope.launch {
            try {
                val plan=withContext(Dispatchers.IO) {
                    check(c.status().state==0) { "Effacement indisponible pendant une plongée ou si l’état de la montre est inconnu." }
                    val entries=c.catalog()
                    check(entries.isNotEmpty()) { "La mémoire de la montre est déjà vide." }
                    check(entries.all { dao.find(info.id.toString(),it.id.toString())?.hash==it.hash.hex() }) { "Synchronise toutes les plongées avant de vider la mémoire de la montre." }
                    erasePlan(info.id,entries)
                }
                check(client===c&&!connection.value.stale) { "Montre déconnectée" }
                eraseConfirmation.value=plan
            } catch(e: Exception) {report(e.message?:"Impossible de préparer l’effacement")}
            finally {busy.value=false}
        }
    }
    fun cancelErase() {eraseConfirmation.value=null}
    fun confirmErase() {
        val plan=eraseConfirmation.value?:return
        val c=client?:return
        if(busy.value)return
        eraseConfirmation.value=null;busy.value=true
        viewModelScope.launch {
            try {
                withContext(Dispatchers.IO) {
                    check(connection.value.info?.id==plan.watchId&&!connection.value.stale) { "La montre connectée a changé. Recommence l’opération." }
                    check(c.status().state==0) { "Effacement indisponible pendant une plongée." }
                    c.erase(plan)
                    check(c.catalog().isEmpty()) { "La mémoire n’est pas vide. Reconnecte la montre pour vérifier." }
                }
                refresh();connection.update {it.copy(remaining=0)}
                report("Mémoire de la montre vidée. Tes plongées restent dans le carnet du téléphone.")
            } catch(e: Exception) {
                connection.update {it.copy(stale=true)}
                report((e.message?:"Effacement non confirmé")+" Reconnecte la montre pour vérifier son contenu.")
            } finally {busy.value=false}
        }
    }
    fun cancelImport() { importJob?.cancel() }
    fun editOuting(row: OutingRow) { viewModelScope.launch(Dispatchers.IO) { dao.saveOuting(row) } }
    fun moveDive(row: DiveRow,outing: String) { viewModelScope.launch(Dispatchers.IO) { dao.moveDive(row.watchId,row.diveId,outing) } }
    fun createOuting(title: String) { viewModelScope.launch(Dispatchers.IO) { dao.saveOuting(OutingRow(UUID.randomUUID().toString(),title.ifBlank { "Nouvelle sortie" })) } }
    fun backup(output: java.io.OutputStream) {
        if(busy.value){output.close();return};busy.value=true
        viewModelScope.launch(Dispatchers.IO) { try { output.use { Archives(db,downloads).write(it) };report("Sauvegarde créée") } catch(e: Exception){report("Sauvegarde : ${e.message}")} finally {busy.value=false} }
    }
    fun restore(input: java.io.InputStream) {
        if(busy.value){input.close();return};busy.value=true
        viewModelScope.launch(Dispatchers.IO) { try { input.use { Archives(db,downloads).restore(it) };report("Sauvegarde restaurée — doublons évités") } catch(e: Exception){report("Restauration refusée : ${e.message}")} finally {busy.value=false} }
    }
    fun csv(output: java.io.OutputStream) { viewModelScope.launch(Dispatchers.IO) { try { output.use { Archives(db,downloads).csv(it) };report("Archive CSV créée") } catch(e: Exception) {report("Export : ${e.message}")} } }
    override fun onCleared() { stopScan();disconnect();db.close();super.onCleared() }
}
