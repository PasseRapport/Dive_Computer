package fr.divewatch.app

import androidx.compose.ui.test.*
import androidx.lifecycle.ViewModelProvider
import fr.divewatch.core.DemoLink
import org.robolectric.annotation.GraphicsMode
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

/** Real Compose screens under Robolectric; no Bluetooth radio is involved. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk=[28],qualifiers="w411dp-h891dp-mdpi")
@GraphicsMode(GraphicsMode.Mode.NATIVE)
class UiTest {
    @get:Rule val compose=createAndroidComposeRule<MainActivity>()
    @Test fun connectedWatchCanImportAndEraseWithoutLosingThePhoneLogbook() {
        compose.onNodeWithText("Ton carnet").assertIsDisplayed()
        compose.onNodeWithText("Montre",useUnmergedTree=true).performClick()
        compose.onNodeWithText("Connecter la démonstration").assertDoesNotExist()
        compose.onNodeWithText("Sans périphérique").assertDoesNotExist()
        compose.runOnIdle { ViewModelProvider(compose.activity)[AppModel::class.java].connectForTest(DemoLink()) }
        compose.waitUntil(15000) { compose.onAllNodesWithText("Montre simulée connectée — données fictives").fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithContentDescription("Ta montre connectée").assertExists()
        compose.onNodeWithText("État : surface").assertDoesNotExist()
        compose.onNodeWithText("Vider la mémoire").assertIsNotEnabled()
        compose.onNodeWithText("Synchroniser les plongées terminées").performScrollTo().performClick()
        compose.waitUntil(15000) { compose.onAllNodesWithText("3 plongée(s) importée(s). Carnet disponible hors ligne.").fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithText("Ta montre").performScrollTo()
        screenshot("watch-connected.png")
        compose.onNodeWithText("Vider la mémoire").performScrollTo().performClick()
        compose.waitUntil(15000) { compose.onAllNodesWithText("Vider la mémoire de la montre ?").fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithText("Annuler").performClick()
        compose.onNodeWithText("Vider la mémoire de la montre ?").assertDoesNotExist()
        compose.onNodeWithText("Vider la mémoire").performScrollTo().performClick()
        compose.waitUntil(15000) { compose.onAllNodesWithText("Vider la mémoire de la montre ?").fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithText("Vider la mémoire de la montre").performClick()
        compose.waitUntil(15000) { compose.onAllNodesWithText("Mémoire de la montre vidée. Tes plongées restent dans le carnet du téléphone.").fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithText("Plongées disponibles : 0").assertExists()
        compose.onNodeWithText("Carnet",useUnmergedTree=true).performClick()
        compose.waitUntil(15000) { compose.onAllNodesWithText("Apnée").fetchSemanticsNodes().size==2 }
        compose.onAllNodesWithText("Apnée").onFirst().performScrollTo().performClick()
        compose.onNodeWithText("PROFONDEUR MAX.").assertExists()
        compose.onNodeWithText("SIMULATION · données fictives").assertExists()
        screenshot("dive-detail.png")
    }
    @Test fun searchShowsAnAccessibleLoadingIndicator() {
        compose.onNodeWithText("Montre",useUnmergedTree=true).performClick()
        compose.runOnIdle { ViewModelProvider(compose.activity)[AppModel::class.java].scanning.value=true }
        compose.onNodeWithContentDescription("Recherche Bluetooth en cours").assertExists()
        compose.onNodeWithText("Recherche en cours…").assertIsNotEnabled()
        compose.runOnIdle { ViewModelProvider(compose.activity)[AppModel::class.java].scanning.value=false }
        compose.onNodeWithContentDescription("Recherche Bluetooth en cours").assertDoesNotExist()
    }
    @Test fun settingsSwitchesThemeAndKeepsTheChoiceAcrossActivityRecreation() {
        compose.onNodeWithText("Paramètres",useUnmergedTree=true).performClick()
        compose.onNodeWithText("Nuit OLED").assertExists()
        screenshot("settings-night.png")
        compose.onNodeWithText("Clair").performClick()
        compose.runOnIdle { org.junit.Assert.assertEquals(AppTheme.LIGHT,ViewModelProvider(compose.activity)[AppModel::class.java].theme.value) }
        screenshot("settings-light.png")
        compose.activityRule.scenario.recreate()
        compose.waitForIdle()
        compose.runOnIdle { org.junit.Assert.assertEquals(AppTheme.LIGHT,ViewModelProvider(compose.activity)[AppModel::class.java].theme.value) }
        compose.onNodeWithText("Océan sombre").performClick()
        screenshot("settings-ocean.png")
        compose.onNodeWithText("Mettre à jour la montre").performScrollTo().assertIsNotEnabled()
        compose.onNodeWithText("Manuel",useUnmergedTree=true).performClick()
        compose.onAllNodesWithText("Manuel").assertCountEquals(2)
    }
    private fun screenshot(name: String) {
        val folder=java.io.File("build/reports/screenshots").apply { mkdirs() }
        compose.runOnIdle {
            val view=compose.activity.window.decorView
            val bitmap=android.graphics.Bitmap.createBitmap(view.width,view.height,android.graphics.Bitmap.Config.ARGB_8888)
            view.draw(android.graphics.Canvas(bitmap))
            java.io.File(folder,name).outputStream().use { output ->
                bitmap.compress(android.graphics.Bitmap.CompressFormat.PNG,100,output)
            }
            bitmap.recycle()
        }
    }
}
