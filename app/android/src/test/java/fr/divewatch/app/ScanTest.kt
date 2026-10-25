package fr.divewatch.app

import android.Manifest
import android.app.Application
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Context
import android.location.LocationManager
import android.os.Looper
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.ViewModelStore
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config
import org.robolectric.annotation.LooperMode
import java.time.Duration

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[28])
@LooperMode(LooperMode.Mode.PAUSED)
class ScanTest {
    private lateinit var app: Application
    private lateinit var store: ViewModelStore
    private lateinit var model: AppModel
    private lateinit var location: LocationManager
    @Before fun setup() {
        app=RuntimeEnvironment.getApplication()
        shadowOf(app).grantPermissions(Manifest.permission.ACCESS_FINE_LOCATION)
        val bluetooth=app.getSystemService(BluetoothManager::class.java).adapter
        shadowOf(bluetooth).setState(BluetoothAdapter.STATE_ON)
        location=app.getSystemService(Context.LOCATION_SERVICE) as LocationManager
        shadowOf(location).setLocationEnabled(true)
        store=ViewModelStore()
        model=ViewModelProvider(store,ViewModelProvider.AndroidViewModelFactory.getInstance(app))[AppModel::class.java]
    }
    @After fun tearDown() {store.clear()}
    @Test fun disabledLocationExplainsWhyLegacyAndroidCannotSearch() {
        shadowOf(location).setLocationEnabled(false)
        model.search()
        assertFalse(model.scanning.value)
        assertTrue("Un message explicite doit remplacer la recherche silencieuse : "+model.message.value,model.message.value.contains("localisation",ignoreCase=true))
    }
    @Test fun noResultsEndsWithAnExplicitMessageAfterTimeout() {
        model.search()
        assertTrue(model.scanning.value)
        shadowOf(Looper.getMainLooper()).idleFor(Duration.ofSeconds(16))
        assertTrue("Le message de recherche doit disparaître : "+model.message.value,model.message.value.startsWith("Aucune montre trouvée"))
        assertFalse(model.scanning.value)
    }
}
