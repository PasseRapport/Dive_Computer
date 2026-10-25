package fr.divewatch.core

import java.io.File
import java.io.FileOutputStream
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.util.UUID
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock

interface WatchLink { suspend fun exchange(frame: Frame): Frame }
class WatchCommandException(val code: Int): IllegalStateException(when(code) {
    3 -> "Effacement refusé : une plongée est en cours."
    4 -> "Le contenu de la montre a changé. Synchronise à nouveau avant de vider sa mémoire."
    else -> "Erreur de la montre : $code"
})
data class ErasePlan(val watchId: UUID,val count: Int,val fingerprint: ByteArray)
fun erasePlan(watchId: UUID,entries: List<Manifest>): ErasePlan {
    require(entries.size<=10000)
    val w=Writer().u16(entries.size)
    entries.forEach { w.bytes(Messages.manifest(it)) }
    return ErasePlan(watchId,entries.size,sha256(w.result()))
}
class WatchClient(private val link: WatchLink) {
    private var sequence=0
    private val mutex=Mutex()
    private suspend fun request(op: Int, payload: ByteArray=byteArrayOf()): ByteArray = mutex.withLock {
        sequence=sequence%65535+1
        val response=link.exchange(Frame(op,sequence,payload=payload))
        require(response.opcode==op && response.requestId==sequence && response.flags and 1==1) { "Réponse inattendue" }
        if(response.flags and 2!=0) { val r=Reader(response.payload);val code=r.u16();r.end();throw WatchCommandException(code) }
        response.payload
    }
    suspend fun hello()=Messages.hello(request(1))
    suspend fun status()=Messages.status(request(2))
    suspend fun supportsErase(): Boolean = try {
        val r=Reader(request(7));val capabilities=r.u32();r.end();capabilities and 1L!=0L
    } catch(e: WatchCommandException) { if(e.code==1)false else throw e }
    suspend fun erase(plan: ErasePlan): Int {
        require(plan.fingerprint.size==32 && plan.count in 1..10000)
        val r=Reader(request(6,Writer().uuid(plan.watchId).bytes(plan.fingerprint).result()))
        val count=r.u16();r.end();require(count==plan.count) { "Confirmation d’effacement incohérente. Reconnecte la montre pour vérifier son contenu." };return count
    }
    suspend fun catalog(): List<Manifest> {
        var cursor=0L;var revision: Long?=null;val result=mutableListOf<Manifest>()
        val seen=mutableSetOf<Long>()
        while(true) {
            require(seen.add(cursor) && seen.size<=1000) { "Catalogue cyclique" }
            val r=Reader(request(3,Writer().u32(cursor).result()))
            val currentRevision=r.u32();require(revision==null||revision==currentRevision) { "Catalogue modifié : relancer la synchronisation" };revision=currentRevision
            val next=r.u32();val count=r.u16();require(count<=50)
            repeat(count) { result+=Messages.readManifest(r) };r.end()
            require(result.size<=10000 && result.map { it.id }.distinct().size==result.size)
            if(next==0xffffffffL) return result
            cursor=next
        }
    }
    suspend fun manifest(id: UUID): Manifest { val r=Reader(request(4,Writer().uuid(id).result()));val m=Messages.readManifest(r);r.end();require(m.id==id);return m }
    suspend fun block(m: Manifest, offset: Int): ByteArray {
        val r=Reader(request(5,Writer().bytes(m.hash).u32(offset.toLong()).result()))
        require(r.u32()==offset.toLong());val size=r.u16();val crc=r.u32();val bytes=r.bytes(size);r.end()
        require(size==minOf(m.blockSize,m.size-offset) && crc32(bytes)==crc) { "Bloc incorrect ou corrompu" }
        return bytes
    }
}
/** A block becomes resumable only after CRC validation, fsync and atomic rename. */
class BlockStore(private val root: File) {
    init { root.mkdirs() }
    private fun dir(m: Manifest)=File(root,m.hash.hex()).apply { mkdirs() }
    fun nextOffset(m: Manifest): Int {
        var offset=0
        while(offset<m.size) {
            val file=File(dir(m),"$offset.block")
            if(!file.exists()) break
            val bytes=file.readBytes()
            if(bytes.size<4) { file.delete();break }
            val r=Reader(bytes);val expected=r.u32();val data=r.bytes(r.remaining)
            if(data.size!=minOf(m.blockSize,m.size-offset)||crc32(data)!=expected) { file.delete();break }
            offset+=data.size
        }
        dir(m).listFiles()?.filter { it.name.endsWith(".block") && (it.name.removeSuffix(".block").toIntOrNull()?:-1)>=offset }?.forEach { it.delete() }
        return offset
    }
    fun append(m: Manifest, offset: Int, bytes: ByteArray) {
        require(offset==nextOffset(m) && bytes.size==minOf(m.blockSize,m.size-offset))
        atomicWrite(File(dir(m),"$offset.block"),Writer().u32(crc32(bytes)).bytes(bytes).result())
    }
    fun complete(m: Manifest): ByteArray {
        require(nextOffset(m)==m.size)
        val bytes=ByteArray(m.size);var offset=0
        while(offset<m.size) { val r=Reader(File(dir(m),"$offset.block").readBytes());r.u32();val b=r.bytes(r.remaining);b.copyInto(bytes,offset);offset+=b.size }
        require(sha256(bytes).contentEquals(m.hash)) { "Empreinte finale incorrecte" }
        return bytes
    }
    fun remove(m: Manifest) { dir(m).deleteRecursively() }
}
fun atomicWrite(file: File, bytes: ByteArray) {
    file.parentFile?.mkdirs()
    val tmp=File(file.parentFile,file.name+".tmp")
    FileOutputStream(tmp).use { it.write(bytes);it.fd.sync() }
    Files.move(tmp.toPath(),file.toPath(),StandardCopyOption.ATOMIC_MOVE,StandardCopyOption.REPLACE_EXISTING)
}
class Importer(private val client: WatchClient, private val store: BlockStore) {
    suspend fun download(watch: WatchInfo, m: Manifest, progress: suspend (Int,Int)->Unit={_,_->}): Dive {
        var offset=store.nextOffset(m);progress(offset,m.size)
        while(offset<m.size) { currentCoroutineContext().ensureActive();val bytes=client.block(m,offset);store.append(m,offset,bytes);offset+=bytes.size;progress(offset,m.size) }
        val dive=DiveCodec.decode(store.complete(m))
        require(dive.watchId==watch.id && dive.id==m.id && dive.mode==m.mode && dive.durationMs==m.durationMs && dive.startedAtMs==m.startedAtMs && dive.simulated==watch.simulated) { "Métadonnées incompatibles" }
        return dive
    }
}
