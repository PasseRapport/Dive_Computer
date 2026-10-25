package fr.divewatch.app

import android.app.Application
import androidx.compose.ui.graphics.toArgb
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
    @Test fun lightPaletteHasReadableTextOnEveryMainSurface() {
        val c=AppTheme.LIGHT.colors()
        val pairs=listOf(c.onBackground to c.background,c.onSurface to c.surfaceContainerLow,
            c.onSurface to c.surface,c.onSurfaceVariant to c.surface,c.primary to c.background,
            c.tertiary to c.surfaceContainerLow,c.onPrimaryContainer to c.primaryContainer,
            c.onSecondaryContainer to c.secondaryContainer,c.onSurface to c.secondaryContainer)
        pairs.forEach { (text,background) ->
            assertTrue("Contraste du texte insuffisant",androidx.core.graphics.ColorUtils.calculateContrast(text.toArgb(),background.toArgb())>=4.5)
        }
    }
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
