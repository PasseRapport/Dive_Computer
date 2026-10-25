package fr.divewatch.app

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.content.Context
import fr.divewatch.core.*
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeout
import no.nordicsemi.android.ble.BleManager
import no.nordicsemi.android.ble.ktx.suspend

@SuppressLint("MissingPermission")
class NordicLink(context: Context, private val lost: ()->Unit): BleManager(context), WatchLink {
    private var rx: BluetoothGattCharacteristic?=null
    private var tx: BluetoothGattCharacteristic?=null
    private val decoder=FrameDecoder()
    private val mutex=Mutex()
    @Volatile private var pending: CompletableDeferred<Frame>?=null
    @Volatile private var notificationsReady=false
    @Volatile private var notificationFailure: Int?=null
    @Volatile private var clearStaleCache=false
    override fun shouldClearCacheWhenDisconnected()=clearStaleCache
    override fun isRequiredServiceSupported(gatt: BluetoothGatt): Boolean {
        val service=gatt.getService(Nus.SERVICE)
        rx=service?.getCharacteristic(Nus.RX);tx=service?.getCharacteristic(Nus.TX)
        return rx?.properties?.and(BluetoothGattCharacteristic.PROPERTY_WRITE)!=0 && rx!=null &&
            tx?.properties?.and(BluetoothGattCharacteristic.PROPERTY_NOTIFY)!=0 && tx!=null
    }
    override fun initialize() {
        notificationsReady=false;notificationFailure=null
        requestMtu(247).enqueue()
        setNotificationCallback(tx).with { _, data ->
            try { data.value?.let { bytes -> decoder.feed(bytes).forEach { pending?.complete(it) } } }
            catch(e: Exception) { pending?.completeExceptionally(e) }
        }
        // ConnectRequest.suspend() resumes inline. Publish readiness inline too,
        // before Nordic advances the initialization queue and completes connection.
        enableNotifications(tx).setHandler(null)
            .done { notificationsReady=true }
            .fail { _, status -> notificationFailure=status }
            .enqueue()
    }
    override fun onServicesInvalidated() {
        rx=null;tx=null;notificationsReady=false;decoder.clear()
        pending?.completeExceptionally(IllegalStateException("Connexion interrompue — import conservé"));lost()
    }
    suspend fun open(device: BluetoothDevice) {
        connect(device).useAutoConnect(false).timeout(15000).suspend()
        // ATT Invalid Handle means cached attributes no longer exist on this server.
        // Nordic refreshes this device's cache before closing its GATT connection.
        if(!notificationsReady && notificationFailure==1) {
            clearStaleCache=true
            try { disconnect().timeout(10000).suspend() }
            finally { clearStaleCache=false }
            connect(device).useAutoConnect(false).timeout(15000).suspend()
        }
        check(notificationsReady) {
            if(notificationFailure==1) "Cache Bluetooth périmé (code 1). Oublie DiveWatch-SIM dans les paramètres Bluetooth puis réessaie."
            else "Notifications Bluetooth indisponibles${notificationFailure?.let { " (code $it)" }?:""}"
        }
    }
    override suspend fun exchange(frame: Frame): Frame = mutex.withLock {
        check(isConnected && notificationsReady) { "Montre déconnectée" }
        val response=CompletableDeferred<Frame>();pending=response
        try {
            withTimeout(15000) {
                writeCharacteristic(rx,frame.encode(),BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT).split().suspend()
                response.await()
            }
        } catch(e: Exception) { disconnect().enqueue();throw e }
        finally { pending=null }
    }
    fun shutdown() { if(isConnected)disconnect().done { close() }.enqueue() else close() }
}
