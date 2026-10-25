package fr.divewatch.app

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.selection.selectableGroup
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.dp
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.util.Locale

enum class AppTheme(val label: String,val description: String) {
    NIGHT("Nuit OLED","Fond noir et boutons discrets"),
    OCEAN("Océan sombre","Bleu profond et accents atténués"),
    LIGHT("Clair","Fond clair et texte sombre");

    fun colors(): ColorScheme=when(this) {
        NIGHT -> darkColorScheme(
            primary=Color(0xffa9c6cd),onPrimary=Color(0xff102e36),
            primaryContainer=Color(0xff22373d),onPrimaryContainer=Color(0xffd2e5e9),
            secondary=Color(0xffb6c4ca),tertiary=Color(0xffe4bda9),
            background=Color.Black,onBackground=Color(0xffe1e4e5),
            surface=Color(0xff101416),onSurface=Color(0xffe1e4e5),
            surfaceVariant=Color(0xff252c30),onSurfaceVariant=Color(0xffbcc7cc),
            surfaceContainer=Color(0xff14191c),surfaceContainerLow=Color(0xff101416),
            surfaceContainerHigh=Color(0xff20272b),surfaceContainerHighest=Color(0xff293136))
        OCEAN -> darkColorScheme(
            primary=Color(0xff9bbbc8),onPrimary=Color(0xff16333f),
            primaryContainer=Color(0xff294653),onPrimaryContainer=Color(0xffd0e5ed),
            secondary=Color(0xffb2c6ce),tertiary=Color(0xffe4bda9),
            background=Color(0xff07151e),onBackground=Color(0xffdce5e9),
            surface=Color(0xff0d1d27),onSurface=Color(0xffdce5e9),
            surfaceVariant=Color(0xff23343e),onSurfaceVariant=Color(0xffbccbd2),
            surfaceContainer=Color(0xff14232d),surfaceContainerLow=Color(0xff101f29),
            surfaceContainerHigh=Color(0xff1c2d37),surfaceContainerHighest=Color(0xff263842))
        LIGHT -> lightColorScheme(
            primary=Color(0xff2b5969),onPrimary=Color.White,
            primaryContainer=Color(0xffd2e6ed),onPrimaryContainer=Color(0xff102e38),
            secondary=Color(0xff485f69),tertiary=Color(0xff884e38),
            background=Color(0xfff5f8fa),surface=Color(0xfff5f8fa),
            onBackground=Color(0xff172126),onSurface=Color(0xff172126),
            surfaceVariant=Color(0xffdce5e9),onSurfaceVariant=Color(0xff3d4b52))
    }
}

internal fun receptionDate(ms: Long?,zone: ZoneId=ZoneId.systemDefault()): String=
    ms?.let { DateTimeFormatter.ofPattern("dd/MM/yyyy HH:mm",Locale.FRANCE).withZone(zone).format(Instant.ofEpochMilli(it)) }?:"Date inconnue"

@Composable
internal fun SettingsScreen(theme: AppTheme,onTheme: (AppTheme)->Unit) {
    Text("Paramètres",style=MaterialTheme.typography.headlineLarge)
    Text("Apparence",style=MaterialTheme.typography.titleLarge)
    Text("Le thème s’applique immédiatement et reste mémorisé.")
    Column(Modifier.fillMaxWidth().selectableGroup()) {
        AppTheme.entries.forEach { option ->
            Row(Modifier.fillMaxWidth().selectable(selected=theme==option,onClick={onTheme(option)},role=Role.RadioButton).padding(vertical=12.dp),verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(12.dp)) {
                RadioButton(selected=theme==option,onClick=null)
                Column { Text(option.label,style=MaterialTheme.typography.titleMedium);Text(option.description,style=MaterialTheme.typography.bodyMedium,color=MaterialTheme.colorScheme.onSurfaceVariant) }
            }
        }
    }
    HorizontalDivider()
    Text("Mises à jour de la montre",style=MaterialTheme.typography.titleLarge)
    Text("Les futures mises à jour du logiciel de la montre par Bluetooth (OTA) seront disponibles ici.")
    Text("Bientôt disponible",color=MaterialTheme.colorScheme.onSurfaceVariant)
    OutlinedButton(onClick={},enabled=false,modifier=Modifier.fillMaxWidth()) { Text("Mettre à jour la montre") }
}
