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
            primary=Color(0xff17645e),onPrimary=Color.White,
            primaryContainer=Color(0xffd5eeea),onPrimaryContainer=Color(0xff103d39),
            secondary=Color(0xff3e6661),onSecondary=Color.White,
            secondaryContainer=Color(0xffdcece7),onSecondaryContainer=Color(0xff153e38),
            tertiary=Color(0xff8a482a),onTertiary=Color.White,
            tertiaryContainer=Color(0xffffdbc9),onTertiaryContainer=Color(0xff341405),
            background=Color(0xfff3f7f5),onBackground=Color(0xff172d29),
            surface=Color(0xfff3f7f5),onSurface=Color(0xff172d29),
            surfaceVariant=Color(0xffe1eae5),onSurfaceVariant=Color(0xff465e55),
            surfaceContainerLowest=Color.White,surfaceContainerLow=Color.White,
            surfaceContainer=Color(0xffeaf1ed),surfaceContainerHigh=Color(0xffe4ece7),
            surfaceContainerHighest=Color(0xffdde7e1),
            outline=Color(0xff6b8177),outlineVariant=Color(0xffbfcec4),
            inverseSurface=Color(0xff263c33),inverseOnSurface=Color(0xffeef5ef),
            inversePrimary=Color(0xff9ed7ca),surfaceTint=Color(0xff17645e),
            error=Color(0xffad2a23),onError=Color.White,
            errorContainer=Color(0xffffdad4),onErrorContainer=Color(0xff400500))
    }
}

internal fun receptionDate(ms: Long?,zone: ZoneId=ZoneId.systemDefault()): String=
    ms?.let { DateTimeFormatter.ofPattern("dd/MM/yyyy HH:mm",Locale.FRANCE).withZone(zone).format(Instant.ofEpochMilli(it)) }?:"Date inconnue"

@Composable
internal fun SettingsScreen(theme: AppTheme,onTheme: (AppTheme)->Unit) {
    Text("Paramètres",style=MaterialTheme.typography.headlineLarge)
    Text("Thème",style=MaterialTheme.typography.titleLarge)
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
