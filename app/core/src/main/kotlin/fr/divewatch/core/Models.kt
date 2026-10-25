package fr.divewatch.core

import java.util.UUID
import kotlin.math.abs

enum class DiveMode(val wire: Int) { APNEA(0), AIR(1);
    companion object { fun from(value: Int) = entries.firstOrNull { it.wire == value } ?: error("Mode inconnu : $value") }
}
data class Sample(val elapsedMs: Long, val depthCm: Int?, val temperatureCentiC: Int?)
data class DiveEvent(val id: Long, val elapsedMs: Long, val code: Int, val severity: Int, val value: Int?)
data class Dive(val watchId: UUID, val id: UUID, val mode: DiveMode, val startedAtMs: Long?, val durationMs: Long,
    val intervalMs: Long, val simulated: Boolean, val samples: List<Sample>, val events: List<DiveEvent>) {
    init {
        require(durationMs in 1..0xffffffffL && intervalMs in 1..0xffffffffL)
        require(samples.isNotEmpty() && samples.size <= 1_000_000 && events.size <= 65535)
        require(samples.all { it.elapsedMs in 0..durationMs && (it.depthCm == null || it.depthCm in 0..65534) &&
            (it.temperatureCentiC == null || it.temperatureCentiC in -32767..32767) })
        require(samples.zipWithNext().all { (a,b) -> b.elapsedMs > a.elapsedMs })
        require(events.all { it.elapsedMs in 0..durationMs && it.id in 0..0xffffffffL && it.code in 0..65535 && it.severity in 0..255 && it.value!=Int.MIN_VALUE } && events.map { it.id }.distinct().size == events.size)
    }
    val maximumDepthCm: Int? get() = samples.mapNotNull { it.depthCm }.maxOrNull()
    val meanDepthCm: Double? get() {
        if (samples.first().elapsedMs != 0L || samples.last().elapsedMs != durationMs || samples.any { it.depthCm == null }) return null
        if (samples.zipWithNext().any { (a,b) -> abs((b.elapsedMs-a.elapsedMs)-intervalMs) > 1 }) return null
        return samples.zipWithNext().sumOf { (a,b) -> (a.depthCm!!+b.depthCm!!)/2.0 * (b.elapsedMs-a.elapsedMs) } / durationMs
    }
}
data class Manifest(val id: UUID, val mode: DiveMode, val startedAtMs: Long?, val durationMs: Long, val size: Int, val hash: ByteArray, val blockSize: Int = 1024) {
    init { require(size in 1..16*1024*1024 && hash.size == 32 && blockSize in 1..1024) }
}
data class WatchInfo(val id: UUID, val simulated: Boolean, val model: String, val firmware: String)
data class WatchStatus(val state: Int, val battery: Int?, val completedDives: Int)
object Nus {
    val SERVICE: UUID = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
    val RX: UUID = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e")
    val TX: UUID = UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e")
}
