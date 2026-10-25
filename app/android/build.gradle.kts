plugins { id("com.android.application"); kotlin("android"); kotlin("plugin.compose"); kotlin("kapt") }
android {
    namespace = "fr.divewatch.app"
    compileSdk = 35
    defaultConfig { applicationId = "fr.divewatch.app"; minSdk = 26; targetSdk = 35; versionCode = 3; versionName = "0.1.2" }
    buildFeatures { compose = true }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    kotlinOptions { jvmTarget = "17" }
    lint { abortOnError = true }
    testOptions { unitTests.isIncludeAndroidResources = true }
}
kapt { arguments { arg("room.schemaLocation", "$projectDir/schemas") } }
dependencies {
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.robolectric:robolectric:4.14.1")
    testImplementation("androidx.test:core:1.6.1")
    implementation(project(":core"))
    implementation(platform("androidx.compose:compose-bom:2025.04.01"))
    testImplementation(platform("androidx.compose:compose-bom:2025.04.01"))
    testImplementation("androidx.compose.ui:ui-test-junit4")
    debugImplementation("androidx.compose.ui:ui-test-manifest")
    implementation("androidx.activity:activity-compose:1.10.1")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.ui:ui-tooling-preview")
    debugImplementation("androidx.compose.ui:ui-tooling")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.9.0")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.9.0")
    implementation("androidx.room:room-runtime:2.7.2")
    implementation("androidx.room:room-ktx:2.7.2")
    kapt("androidx.room:room-compiler:2.7.2")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.1")
    implementation("no.nordicsemi.android:ble-ktx:2.11.0")
}
