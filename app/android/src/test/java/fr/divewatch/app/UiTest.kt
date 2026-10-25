package fr.divewatch.app

import androidx.compose.ui.test.*
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
    @Test fun demoCanBeImportedAndBrowsedWithoutBluetoothPermission() {
        compose.onNodeWithText("Ton carnet").assertIsDisplayed()
        compose.onNodeWithText("Montre",useUnmergedTree=true).performClick()
        compose.onNodeWithText("Connecter la démonstration").performScrollTo().performClick()
        compose.waitUntil(15000) { compose.onAllNodesWithText("Montre simulée connectée — données fictives").fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithText("Synchroniser les plongées terminées").performScrollTo().performClick()
        compose.waitUntil(15000) { compose.onAllNodesWithText("3 plongée(s) importée(s). Carnet disponible hors ligne.").fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithText("Carnet",useUnmergedTree=true).performClick()
        compose.waitUntil(15000) { compose.onAllNodesWithText("Apnée").fetchSemanticsNodes().size==2 }
        compose.onAllNodesWithText("Apnée").onFirst().performScrollTo().performClick()
        compose.onNodeWithText("PROFONDEUR MAX.").assertExists()
        compose.onNodeWithText("SIMULATION · données fictives").assertExists()
        val folder=java.io.File("build/reports/screenshots").apply { mkdirs() }
        compose.runOnIdle {
            val view=compose.activity.window.decorView
            val bitmap=android.graphics.Bitmap.createBitmap(view.width,view.height,android.graphics.Bitmap.Config.ARGB_8888)
            view.draw(android.graphics.Canvas(bitmap))
            java.io.File(folder,"dive-detail.png").outputStream().use { output ->
                bitmap.compress(android.graphics.Bitmap.CompressFormat.PNG,100,output)
            }
            bitmap.recycle()
        }
    }
}
