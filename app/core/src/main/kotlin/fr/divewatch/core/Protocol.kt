package fr.divewatch.core

import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.MessageDigest
import java.util.UUID
import java.util.zip.CRC32

class Writer {
    private val out = ByteArrayOutputStream()
    fun u8(v: Int) = apply { out.write(v) }
    fun u16(v: Int) = apply { u8(v); u8(v ushr 8) }
    fun u32(v: Long) = apply { repeat(4) { u8((v ushr (it*8)).toInt()) } }
    fun i64(v: Long) = apply { repeat(8) { u8((v ushr (it*8)).toInt()) } }
    fun bytes(v: ByteArray) = apply { out.write(v) }
    fun uuid(v: UUID) = bytes(ByteBuffer.allocate(16).putLong(v.mostSignificantBits).putLong(v.leastSignificantBits).array())
    fun text(v: String) = apply { val b=v.toByteArray(Charsets.UTF_8); require(b.size<=255); u8(b.size); bytes(b) }
    fun result(): ByteArray = out.toByteArray()
}
class Reader(private val data: ByteArray) {
    private var pos=0
    val remaining get() = data.size-pos
    fun bytes(n: Int): ByteArray { require(n>=0 && n<=remaining) { "Message tronqué" }; return data.copyOfRange(pos,pos+n).also { pos+=n } }
    fun u8() = bytes(1)[0].toInt() and 255
    fun u16() = u8() or (u8() shl 8)
    fun i16() = u16().toShort().toInt()
    fun u32(): Long { var v=0L; repeat(4) { v=v or (u8().toLong() shl (it*8)) }; return v }
    fun i64() = ByteBuffer.wrap(bytes(8)).order(ByteOrder.LITTLE_ENDIAN).long
    fun uuid(): UUID = ByteBuffer.wrap(bytes(16)).let { UUID(it.long,it.long) }
    fun text() = bytes(u8()).toString(Charsets.UTF_8)
    fun end() = require(remaining==0) { "Octets inattendus" }
}
data class Frame(val opcode: Int, val requestId: Int, val flags: Int=0, val payload: ByteArray=byteArrayOf()) {
    fun encode(): ByteArray { require(payload.size<=4088 && requestId in 1..65535 && flags in 0..3 && opcode in 1..255)
        return Writer().u16(8+payload.size).u8(1).u8(opcode).u16(requestId).u16(flags).bytes(payload).result() }
}
class FrameDecoder {
    private var pending=byteArrayOf()
    fun clear() { pending=byteArrayOf() }
    fun feed(fragment: ByteArray): List<Frame> {
        require(fragment.size<=4096)
        pending+=fragment
        val result=mutableListOf<Frame>()
        while(pending.size>=2) {
            val n=(pending[0].toInt() and 255) or ((pending[1].toInt() and 255) shl 8)
            require(n in 8..4096) { "Taille de trame invalide" }
            if(pending.size<n) break
            val r=Reader(pending.copyOfRange(0,n));r.u16();require(r.u8()==1) { "Version incompatible" }
            val opcode=r.u8();val id=r.u16();val flags=r.u16();require(id!=0 && flags and 3==flags)
            result+=Frame(opcode,id,flags,r.bytes(r.remaining));pending=pending.copyOfRange(n,pending.size)
        }
        require(pending.size<=4096)
        return result
    }
}
fun sha256(bytes: ByteArray): ByteArray = MessageDigest.getInstance("SHA-256").digest(bytes)
fun crc32(bytes: ByteArray): Long = CRC32().apply { update(bytes) }.value
fun ByteArray.hex() = joinToString("") { "%02x".format(it) }
object DiveCodec {
    fun encode(d: Dive): ByteArray {
        val w=Writer().bytes("DWLG".toByteArray()).u8(1).u8(d.mode.wire).u16(if(d.simulated)1 else 0)
            .uuid(d.watchId).uuid(d.id).i64(d.startedAtMs?:-1).u32(d.durationMs).u32(d.intervalMs).u32(d.samples.size.toLong()).u16(d.events.size)
        d.samples.forEach { w.u32(it.elapsedMs).u16(it.depthCm?:65535).u16(it.temperatureCentiC?:-32768) }
        d.events.forEach { w.u32(it.id).u32(it.elapsedMs).u16(it.code).u8(it.severity).u32((it.value?:Int.MIN_VALUE).toLong()) }
        return w.result()
    }
    fun decode(data: ByteArray): Dive {
        val r=Reader(data);require(r.bytes(4).contentEquals("DWLG".toByteArray()));require(r.u8()==1)
        val mode=DiveMode.from(r.u8());val flags=r.u16();require(flags in 0..1)
        val watch=r.uuid();val id=r.uuid();val start=r.i64().let { if(it == -1L)null else it }
        val duration=r.u32();val interval=r.u32();val n=r.u32();val ne=r.u16()
        require(n in 1..1_000_000 && n*8+ne*15==r.remaining.toLong()) { "Taille du profil incohérente" }
        val samples=List(n.toInt()) { Sample(r.u32(),r.u16().let { if(it==65535)null else it },r.i16().let { if(it == -32768)null else it }) }
        val events=List(ne) { DiveEvent(r.u32(),r.u32(),r.u16(),r.u8(),r.u32().toInt().let { if(it==Int.MIN_VALUE)null else it }) }
        r.end();return Dive(watch,id,mode,start,duration,interval,flags==1,samples,events)
    }
    fun manifest(d: Dive): Manifest { val b=encode(d);return Manifest(d.id,d.mode,d.startedAtMs,d.durationMs,b.size,sha256(b)) }
}
object Messages {
    fun manifest(m: Manifest): ByteArray = Writer().uuid(m.id).u8(m.mode.wire).i64(m.startedAtMs?:-1).u32(m.durationMs).u32(m.size.toLong()).bytes(m.hash).u16(m.blockSize).result()
    fun readManifest(r: Reader): Manifest = Manifest(r.uuid(),DiveMode.from(r.u8()),r.i64().let { if(it == -1L)null else it },r.u32(),r.u32().toInt(),r.bytes(32),r.u16())
    fun hello(data: ByteArray): WatchInfo { val r=Reader(data);val id=r.uuid();val origin=r.u8();require(origin in 0..1);val w=WatchInfo(id,origin==1,r.text(),r.text());r.end();return w }
    fun status(data: ByteArray): WatchStatus { val r=Reader(data);val state=r.u8();val b=r.u8();val n=r.u16();r.end();require(state in 0..2 && (b<=100||b==255));return WatchStatus(state,if(b==255)null else b,n) }
}
