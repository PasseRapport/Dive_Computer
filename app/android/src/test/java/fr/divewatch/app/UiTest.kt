package fr.divewatch.app

import androidx.compose.ui.test.*
import androidx.lifecycle.ViewModelProvider
import fr.divewatch.core.DemoLink
import org.robolectric.annotation.GraphicsMode
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import org.junit.Before
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
    @Before fun resetFixtures() {
        compose.runOnIdle {
            val model=ViewModelProvider(compose.activity)[AppModel::class.java]
            kotlinx.coroutines.runBlocking(kotlinx.coroutines.Dispatchers.IO) { model.db.clearAllTables() }
            model.setTheme(AppTheme.NIGHT)
        }
    }
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
        compose.onNodeWithTag("category-APNEA").performScrollTo().performClick()
        compose.waitUntil(15000) { compose.onAllNodesWithTag("dive-"+fr.divewatch.core.Demo.dives().first().id).fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithTag("dive-"+fr.divewatch.core.Demo.dives().first().id).performScrollTo().performClick()
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
    @Test fun categoriesAndLongPressDeletionAreScopedAndReimportable() {
        val all=fr.divewatch.core.Demo.dives()
        val apnea=all.filter {it.mode==fr.divewatch.core.DiveMode.APNEA}
        val air=all.single {it.mode==fr.divewatch.core.DiveMode.AIR}
        compose.runOnIdle { ViewModelProvider(compose.activity)[AppModel::class.java].connectForTest(DemoLink()) }
        compose.waitUntil(15000) { compose.onAllNodesWithText("Montre simulée connectée — données fictives").fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithText("Montre",useUnmergedTree=true).performClick()
        compose.onNodeWithText("Synchroniser les plongées terminées").performScrollTo().performClick()
        compose.waitUntil(15000) { compose.onAllNodesWithText("3 plongée(s) importée(s). Carnet disponible hors ligne.").fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithText("Carnet",useUnmergedTree=true).performClick()
        screenshot("carnet-categories.png")
        compose.onNodeWithTag("category-APNEA").performScrollTo().performClick()
        compose.onNodeWithTag("dive-${air.id}").assertDoesNotExist()
        compose.onNodeWithTag("dive-${apnea.first().id}").performScrollTo().performTouchInput {longClick()}
        compose.onNodeWithTag("check-${apnea.first().id}").assertIsOn()
        compose.onNodeWithText("1 sélectionnée(s)").performScrollTo().assertExists()
        compose.onNodeWithTag("select-all").performScrollTo().performClick().assertIsOn()
        compose.onNodeWithText("2 sélectionnée(s)").assertExists()
        compose.onNodeWithTag("check-${apnea.first().id}").performScrollTo().performClick().assertIsOff()
        compose.onNodeWithTag("select-all").performScrollTo().assertIsOff().performClick().assertIsOn()
        screenshot("carnet-selection.png")
        compose.onNodeWithText("Supprimer").performClick()
        compose.onNodeWithText("Annuler").performClick()
        compose.onNodeWithTag("dive-${apnea.first().id}").assertExists()
        compose.onNodeWithText("Supprimer").performClick()
        compose.onNodeWithText("Supprimer du carnet").performClick()
        compose.waitUntil(15000) { compose.onAllNodesWithText("2 plongée(s) supprimée(s) du carnet. La mémoire de la montre est conservée.").fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithTag("dive-${apnea.first().id}").assertDoesNotExist()
        compose.onNodeWithText("‹ Catégories").performScrollTo().performClick()
        compose.onNodeWithTag("category-AIR").performClick()
        compose.onNodeWithTag("dive-${air.id}").assertExists()
        compose.onNodeWithText("Montre",useUnmergedTree=true).performClick()
        compose.onNodeWithText("Restant à importer : 2").assertExists()
        compose.onNodeWithText("Vider la mémoire").assertIsNotEnabled()
        compose.onNodeWithText("Synchroniser les plongées terminées").performScrollTo().performClick()
        compose.waitUntil(15000) { compose.onAllNodesWithText("2 plongée(s) importée(s). Carnet disponible hors ligne.").fetchSemanticsNodes().isNotEmpty() }
    }
    @Test @Config(sdk=[29],qualifiers="w411dp-h891dp-night-mdpi")
    fun lightThemeRemainsReadableWithSystemNightMode() {
        compose.runOnIdle { ViewModelProvider(compose.activity)[AppModel::class.java].setTheme(AppTheme.LIGHT) }
        compose.onNodeWithText("Ton carnet").assertIsDisplayed()
        // Android must not recolor a palette already managed by the app.
        compose.runOnIdle { org.junit.Assert.assertFalse(compose.activity.window.decorView.isForceDarkAllowed) }
        assertDarkTextOnLightBackground()
        screenshot("light-carnet.png")
        compose.onNodeWithText("Montre",useUnmergedTree=true).performClick()
        compose.runOnIdle { ViewModelProvider(compose.activity)[AppModel::class.java].connectForTest(DemoLink()) }
        compose.waitUntil(15000) { compose.onAllNodesWithText("Montre simulée connectée — données fictives").fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithText("Synchroniser les plongées terminées").performScrollTo().performClick()
        compose.waitUntil(15000) { compose.onAllNodesWithText("3 plongée(s) importée(s). Carnet disponible hors ligne.").fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithText("Ta montre").performScrollTo()
        screenshot("light-watch.png")
        assertDarkTextOnLightBackground()
        compose.onNodeWithText("Carnet",useUnmergedTree=true).performClick()
        compose.onNodeWithTag("category-APNEA").performScrollTo().performClick()
        screenshot("light-list.png")
        val first=fr.divewatch.core.Demo.dives().first {it.mode==fr.divewatch.core.DiveMode.APNEA}
        compose.onNodeWithTag("dive-${first.id}").performScrollTo().performClick()
        screenshot("light-detail.png")
        compose.onNodeWithText("Manuel",useUnmergedTree=true).performClick()
        screenshot("light-manual.png")
        assertDarkTextOnLightBackground()
    }
    private fun assertDarkTextOnLightBackground() {
        compose.runOnIdle {
            val view=compose.activity.window.decorView
            val bitmap=android.graphics.Bitmap.createBitmap(view.width,view.height,android.graphics.Bitmap.Config.ARGB_8888)
            view.draw(android.graphics.Canvas(bitmap))
            var dark=0
            for(y in 30 until minOf(190,bitmap.height))for(x in 16 until bitmap.width-16) {
                val p=bitmap.getPixel(x,y)
                if(android.graphics.Color.red(p)<120&&android.graphics.Color.green(p)<120&&android.graphics.Color.blue(p)<120)dark++
            }
            bitmap.recycle()
            org.junit.Assert.assertTrue("Le titre doit rester sombre sur fond clair",dark>100)
        }
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
