package fr.divewatch.app

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGattCharacteristic
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
@Config(sdk=[28])
@LooperMode(LooperMode.Mode.PAUSED)
class NordicLinkTest {
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
