package fr.divewatch.app

import android.app.Application
import androidx.test.core.app.ApplicationProvider
import java.time.Instant
import java.time.ZoneId
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[28])
class SettingsTest {
    @Test fun receptionUsesNumericDateAndLocalTime() {
        val instant=Instant.parse("2026-10-25T21:46:00Z").toEpochMilli()
        assertEquals("25/10/2026 22:46",receptionDate(instant,ZoneId.of("Europe/Paris")))
        assertEquals("25/10/2026 21:46",receptionDate(instant,ZoneId.of("UTC")))
        assertEquals("Date inconnue",receptionDate(null))
    }
    @Test fun appearanceSurvivesANewModelAndIgnoresUnknownValues() {
        val app=ApplicationProvider.getApplicationContext<Application>()
        val prefs=app.getSharedPreferences("appearance",0)
        prefs.edit().clear().commit()
        val first=AppModel(app)
        assertEquals(AppTheme.NIGHT,first.theme.value)
        first.setTheme(AppTheme.OCEAN)
        val second=AppModel(app)
        assertEquals(AppTheme.OCEAN,second.theme.value)
        prefs.edit().putString("theme","future-theme").commit()
        val fallback=AppModel(app)
        assertEquals(AppTheme.NIGHT,fallback.theme.value)
        first.db.close();second.db.close();fallback.db.close()
    }
}
