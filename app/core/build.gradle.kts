plugins { kotlin("jvm") }
kotlin { jvmToolchain(17) }
dependencies { implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.10.1"); testImplementation(kotlin("test-junit")); testImplementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.10.1") }

tasks.test { systemProperty("fixtures", rootProject.file("protocol/fixtures").absolutePath) }
