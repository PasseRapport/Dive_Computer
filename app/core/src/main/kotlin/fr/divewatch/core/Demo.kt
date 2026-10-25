package fr.divewatch.core

import java.util.UUID
import kotlin.math.sin

object Demo {
    val watch=WatchInfo(UUID.fromString("00000000-0000-4000-8000-000000000001"),true,"DiveWatch-SIM","sim-0.1")
    fun dives(): List<Dive> = listOf(90,120,1800).mapIndexed { index, seconds ->
        val mode=if(index<2)DiveMode.APNEA else DiveMode.AIR
        val max=if(index<2)1700 else 2400
        Dive(watch.id,UUID(0x4000L,0x8000000000000000UL.toLong()+index+2),mode,1792836000000L+index*600000L,seconds*1000L,1000,true,
            (0..seconds).map { t -> Sample(t*1000L,(sin(Math.PI*t/seconds)*max).toInt().coerceAtLeast(0),if(index==1)null else 1820) },
            listOf(DiveEvent(1,seconds*500L,1,1,max)))
    }
}
class DemoLink(dives: List<Dive> = Demo.dives()): WatchLink {
    private val dives=dives.toMutableList()
    private var revision=1L
    override suspend fun exchange(frame: Frame): Frame {
        val body=when(frame.opcode) {
            1 -> Writer().uuid(Demo.watch.id).u8(1).text(Demo.watch.model).text(Demo.watch.firmware).result()
            2 -> Writer().u8(0).u8(87).u16(dives.size).result()
            3 -> { val r=Reader(frame.payload);val cursor=r.u32();r.end();require(cursor==0L);val w=Writer().u32(revision).u32(0xffffffffL).u16(dives.size);dives.forEach { w.bytes(Messages.manifest(DiveCodec.manifest(it))) };w.result() }
            4 -> { val r=Reader(frame.payload);val id=r.uuid();r.end();Messages.manifest(DiveCodec.manifest(dives.first { it.id==id })) }
            5 -> { val r=Reader(frame.payload);val hash=r.bytes(32);val offset=r.u32().toInt();r.end();val data=dives.map { DiveCodec.encode(it) }.first { sha256(it).contentEquals(hash) };require(offset>=0&&offset<data.size&&offset%1024==0);val part=data.copyOfRange(offset,minOf(offset+1024,data.size));Writer().u32(offset.toLong()).u16(part.size).u32(crc32(part)).bytes(part).result() }
            7 -> Writer().u32(1).result()
            6 -> {
                val r=Reader(frame.payload);require(r.uuid()==Demo.watch.id)
                val fingerprint=r.bytes(32);r.end()
                require(fingerprint.contentEquals(erasePlan(Demo.watch.id,dives.map(DiveCodec::manifest)).fingerprint))
                val count=dives.size;dives.clear();revision++;Writer().u16(count).result()
            }
            else -> error("Commande inconnue")
        }
        // Exercise the same framing and incremental decoder as the real BLE path.
        val bytes=Frame(frame.opcode,frame.requestId,1,body).encode();val decoder=FrameDecoder()
        return bytes.toList().chunked(20).flatMap { decoder.feed(it.toByteArray()) }.single()
    }
}
