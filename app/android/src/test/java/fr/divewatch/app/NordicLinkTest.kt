package fr.divewatch.app

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattService
import android.bluetooth.BluetoothProfile
import android.os.Looper
import android.os.Handler
import kotlinx.coroutines.*
import org.robolectric.Shadows.shadowOf
import java.util.UUID
import org.robolectric.annotation.Implements
import org.robolectric.annotation.Implementation
import org.robolectric.annotation.RealObject
import org.robolectric.shadows.ShadowBluetoothGatt
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import fr.divewatch.core.Nus
import java.util.concurrent.LinkedBlockingDeque
import no.nordicsemi.android.ble.BleManager
import no.nordicsemi.android.ble.Request
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config
import org.robolectric.annotation.LooperMode

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[28],shadows=[CachedGatt::class])
@LooperMode(LooperMode.Mode.PAUSED)
class NordicLinkTest {
    @Test fun fullConnectionEnablesNotificationsBeforeOpenReturns() {
        connectThroughGatt(false).getOrThrow()
        assertEquals(0,CachedGatt.refreshes)
    }
    @Test fun invalidHandleRefreshesCacheAndRetriesConnectionOnce() {
        connectThroughGatt(true).getOrThrow()
        assertEquals(1,CachedGatt.refreshes)
    }
    @Test fun persistentInvalidHandleStopsAfterOneRetryWithActionableError() {
        val error=connectThroughGatt(true,repairsCache=false).exceptionOrNull()
        assertNotNull(error)
        assertTrue(error!!.message.orEmpty().contains("code 1"))
        assertEquals(1,CachedGatt.refreshes)
    }
    private fun connectThroughGatt(stale: Boolean,repairsCache: Boolean=true): Result<Unit> {
        CachedGatt.stale=stale;CachedGatt.refreshes=0;CachedGatt.repairsCache=repairsCache
        val app=RuntimeEnvironment.getApplication()
        shadowOf(app).grantPermissions(android.Manifest.permission.BLUETOOTH,android.Manifest.permission.BLUETOOTH_ADMIN)
        shadowOf(BluetoothAdapter.getDefaultAdapter()).setState(BluetoothAdapter.STATE_ON)
        val device=BluetoothAdapter.getDefaultAdapter().getRemoteDevice("00:11:22:33:44:56")
        val link=NordicLink(app) {}
        val tx=BluetoothGattCharacteristic(Nus.TX,BluetoothGattCharacteristic.PROPERTY_NOTIFY,0)
        tx.addDescriptor(BluetoothGattDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"),BluetoothGattDescriptor.PERMISSION_WRITE))
        val service=BluetoothGattService(Nus.SERVICE,BluetoothGattService.SERVICE_TYPE_PRIMARY)
        service.addCharacteristic(tx)
        service.addCharacteristic(BluetoothGattCharacteristic(Nus.RX,BluetoothGattCharacteristic.PROPERTY_WRITE,BluetoothGattCharacteristic.PERMISSION_WRITE))
        val scope=CoroutineScope(Dispatchers.Unconfined)
        shadowOf(device).setGattConnectionInterceptor { gatt ->
            shadowOf(gatt).addDiscoverableService(service)
            shadowOf(gatt).allowCharacteristicNotification(tx)
            Handler(Looper.getMainLooper()).post {
                shadowOf(gatt).gattCallback.onConnectionStateChange(gatt,BluetoothGatt.GATT_SUCCESS,BluetoothProfile.STATE_CONNECTED)
            }
        }
        try {
            val opened=scope.async { runCatching { link.open(device) } }
            shadowOf(Looper.getMainLooper()).idle()
            // Nordic uses a real Timer for its service-discovery delay.
            val deadline=System.nanoTime()+8_000_000_000L
            while(!opened.isCompleted && System.nanoTime()<deadline) {
                shadowOf(Looper.getMainLooper()).idle()
                Thread.sleep(10)
            }
            assertTrue("open() must complete after service discovery and subscription",opened.isCompleted)
            return runBlocking { opened.await() }
        } finally {scope.cancel();link.close()}
    }
    @Test fun notificationSuccessIsVisibleBeforeNordicFinishesInitialization() {
        val link=NordicLink(RuntimeEnvironment.getApplication()) {}
        val handler=BleManager::class.java.getDeclaredField("requestHandler").apply { isAccessible=true }.get(link)
        val handlerClass=Class.forName("no.nordicsemi.android.ble.BleManagerHandler")
        fun field(name: String,value: Any) {
            handlerClass.getDeclaredField(name).apply { isAccessible=true }.set(handler,value)
        }
        // Capture the real Nordic initialization requests without a physical radio.
        val queue=LinkedBlockingDeque<Request>()
        field("initialization",true);field("operationInProgress",true);field("initQueue",queue)
        NordicLink::class.java.getDeclaredField("tx").apply { isAccessible=true }.set(link,
            BluetoothGattCharacteristic(Nus.TX,BluetoothGattCharacteristic.PROPERTY_NOTIFY,0))
        NordicLink::class.java.getDeclaredMethod("initialize").apply { isAccessible=true }.invoke(link)
        val notificationRequest=queue.last
        val device=BluetoothAdapter.getDefaultAdapter().getRemoteDevice("00:11:22:33:44:55")
        // Nordic advances its queue immediately after notifySuccess, before posted callbacks run.
        Request::class.java.getDeclaredMethod("notifySuccess",BluetoothDevice::class.java)
            .apply { isAccessible=true }.invoke(notificationRequest,device)
        val ready=NordicLink::class.java.getDeclaredField("notificationsReady").apply { isAccessible=true }.getBoolean(link)
        assertTrue("open() must not report Notifications Bluetooth indisponibles after successful subscription",ready)
        link.close()
    }
}

/** Replays the ATT Invalid Handle response captured on the Mi 9T. */
@Implements(BluetoothGatt::class)
class CachedGatt: ShadowBluetoothGatt() {
    @RealObject private lateinit var gatt: BluetoothGatt
    companion object {var stale=false;var refreshes=0;var repairsCache=true}
    @Implementation override fun writeDescriptor(descriptor: BluetoothGattDescriptor): Boolean {
        if(!stale)return super.writeDescriptor(descriptor)
        gattCallback.onDescriptorWrite(gatt,descriptor,1)
        return true
    }
    @Implementation fun refresh(): Boolean {refreshes++;if(repairsCache)stale=false;return true}
    @Implementation override fun disconnect() {
        gattCallback.onConnectionStateChange(gatt,BluetoothGatt.GATT_SUCCESS,BluetoothProfile.STATE_DISCONNECTED)
    }
}
