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
    override fun isRequiredServiceSupported(gatt: BluetoothGatt): Boolean {
        val service=gatt.getService(Nus.SERVICE)
        rx=service?.getCharacteristic(Nus.RX);tx=service?.getCharacteristic(Nus.TX)
        return rx?.properties?.and(BluetoothGattCharacteristic.PROPERTY_WRITE)!=0 && rx!=null &&
            tx?.properties?.and(BluetoothGattCharacteristic.PROPERTY_NOTIFY)!=0 && tx!=null
    }
    override fun initialize() {
        requestMtu(247).enqueue()
        setNotificationCallback(tx).with { _, data ->
            try { data.value?.let { bytes -> decoder.feed(bytes).forEach { pending?.complete(it) } } }
            catch(e: Exception) { pending?.completeExceptionally(e) }
        }
        // ConnectRequest.suspend() resumes inline. Publish readiness inline too,
        // before Nordic advances the initialization queue and completes connection.
        enableNotifications(tx).setHandler(null).done { notificationsReady=true }.enqueue()
    }
    override fun onServicesInvalidated() {
        rx=null;tx=null;notificationsReady=false;decoder.clear()
        pending?.completeExceptionally(IllegalStateException("Connexion interrompue — import conservé"));lost()
    }
    suspend fun open(device: BluetoothDevice) {
        connect(device).useAutoConnect(false).timeout(15000).suspend()
        check(notificationsReady) { "Notifications Bluetooth indisponibles" }
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
