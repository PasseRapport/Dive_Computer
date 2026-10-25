package fr.divewatch.core

import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.File
import java.nio.file.Files
import java.util.UUID
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
import kotlinx.coroutines.runBlocking
import kotlin.test.*

class CoreTest {
    private fun fixtures()=File(System.getProperty("fixtures"))
    private fun temp()=Files.createTempDirectory("divewatch-test").toFile().apply { deleteOnExit() }
    @Test fun helloHasIndependentGoldenBytes() { assertEquals(File(fixtures(),"hello-request.hex").readText().trim(),Frame(1,1).encode().hex()) }
    @Test fun pythonProfilesDecodeAndEncodeExactly() {
        repeat(3) { n ->
            val bytes=File(fixtures(),"dive-$n.bin").readBytes()
            assertEquals(File(fixtures(),"dive-$n.sha256").readText().trim(),sha256(bytes).hex())
            val dive=DiveCodec.decode(bytes)
            assertContentEquals(bytes,DiveCodec.encode(dive))
            assertContentEquals(File(fixtures(),"dive-$n.manifest").readBytes(),Messages.manifest(DiveCodec.manifest(dive)))
            assertTrue(dive.simulated);assertEquals(Demo.watch.id,dive.watchId)
        }
    }
    @Test fun frameDecoderSupportsEverySplit() {
        val f=Frame(5,123,1,ByteArray(1024){it.toByte()});val wire=f.encode()
        for(size in listOf(1,2,7,20,244,4096)) {
            val decoder=FrameDecoder();val out=wire.toList().chunked(size).flatMap { decoder.feed(it.toByteArray()) }
            assertEquals(1,out.size);assertContentEquals(f.payload,out.single().payload)
        }
    }
    @Test fun malformedFrameAndProfileAreRejected() {
        assertFails { FrameDecoder().feed(byteArrayOf(7,0)) }
        val bytes=File(fixtures(),"dive-0.bin").readBytes()
        assertFails { DiveCodec.decode(bytes.copyOf(bytes.size-1)) }
        assertFails { DiveCodec.decode(bytes+byteArrayOf(0)) }
        assertFails { FrameDecoder().feed(byteArrayOf(8,0,2,1,1,0,0,0)) }
    }
    @Test fun missingTemperatureStaysMissing() {
        val dive=DiveCodec.decode(File(fixtures(),"dive-1.bin").readBytes())
        assertTrue(dive.samples.all { it.temperatureCentiC==null })
    }
    @Test fun statisticsUseTimeAndRejectGaps() {
        val d=Dive(Demo.watch.id,UUID.randomUUID(),DiveMode.APNEA,null,2000,1000,true,listOf(Sample(0,0,null),Sample(1000,1000,null),Sample(2000,0,null)),emptyList())
        assertEquals(1000,d.maximumDepthCm);assertEquals(500.0,d.meanDepthCm)
        assertNull(d.copy(samples=listOf(d.samples.first(),d.samples.last())).meanDepthCm)
        assertNull(d.copy(samples=d.samples.map { it.copy(depthCm=null) }).meanDepthCm)
    }
    @Test fun fullImportMatchesRemoteFile() = runBlocking {
        val client=WatchClient(DemoLink());val info=client.hello();val manifests=client.catalog()
        assertEquals(3,manifests.size)
        val store=BlockStore(temp());val dive=Importer(client,store).download(info,manifests.last())
        assertContentEquals(DiveCodec.encode(Demo.dives().last()),DiveCodec.encode(dive))
    }
    @Test fun interruptedImportResumesFromDurableBlock() = runBlocking {
        val base=DemoLink();var requests=0
        val failing=object: WatchLink { override suspend fun exchange(frame: Frame): Frame { if(frame.opcode==5 && ++requests==2)error("radio coupée");return base.exchange(frame) } }
        val folder=temp();val manifest=DiveCodec.manifest(Demo.dives().last())
        assertFailsWith<IllegalStateException> { Importer(WatchClient(failing),BlockStore(folder)).download(Demo.watch,manifest) }
        assertEquals(1024,BlockStore(folder).nextOffset(manifest))
        val offsets=mutableListOf<Long>();val tracked=object: WatchLink { override suspend fun exchange(frame: Frame): Frame { if(frame.opcode==5)offsets+=Reader(frame.payload).let{it.bytes(32);it.u32()};return base.exchange(frame) } }
        Importer(WatchClient(tracked),BlockStore(folder)).download(Demo.watch,manifest)
        assertEquals(1024L,offsets.first());assertFalse(0L in offsets)
    }
    @Test fun invalidBlockNeverBecomesDurable() = runBlocking {
        val base=DemoLink();val bad=object: WatchLink { override suspend fun exchange(frame: Frame): Frame { val response=base.exchange(frame);return if(frame.opcode==5)response.copy(payload=response.payload.copyOf().apply{this[lastIndex]=(this[lastIndex].toInt() xor 1).toByte()}) else response } }
        val store=BlockStore(temp());val m=DiveCodec.manifest(Demo.dives().last())
        assertFails { Importer(WatchClient(bad),store).download(Demo.watch,m) }
        assertEquals(0,store.nextOffset(m))
    }
    @Test fun corruptedLocalBlockIsRedownloaded() {
        val dir=temp();val d=Demo.dives().last();val m=DiveCodec.manifest(d);val store=BlockStore(dir)
        store.append(m,0,DiveCodec.encode(d).copyOfRange(0,1024));assertEquals(1024,store.nextOffset(m))
        File(dir,m.hash.hex()+"/0.block").writeBytes(byteArrayOf(1,2))
        assertEquals(0,BlockStore(dir).nextOffset(m))
    }
    @Test fun mismatchedFinalHashCannotComplete() {
        val d=Demo.dives().first();val bytes=DiveCodec.encode(d);val m=DiveCodec.manifest(d).copy(hash=ByteArray(32));val store=BlockStore(temp())
        store.append(m,0,bytes)
        assertFails { store.complete(m) }
    }
    @Test fun wrongWatchIdentityCannotPublish() = runBlocking {
        val m=DiveCodec.manifest(Demo.dives().first())
        assertFails { Importer(WatchClient(DemoLink()),BlockStore(temp())).download(Demo.watch.copy(id=UUID.randomUUID()),m) };Unit
    }
    @Test fun safeArchiveRoundTrip() {
        val out=ByteArrayOutputStream();SafeArchive.write(out,mapOf("data/a.bin" to byteArrayOf(1,2,3)))
        val files=SafeArchive.read(ByteArrayInputStream(out.toByteArray()));assertContentEquals(byteArrayOf(1,2,3),files["data/a.bin"])
    }
    @Test fun archiveTraversalIsRejected() {
        val out=ByteArrayOutputStream();ZipOutputStream(out).use { it.putNextEntry(ZipEntry("../evil"));it.write(1);it.closeEntry() }
        assertFails { SafeArchive.read(ByteArrayInputStream(out.toByteArray())) }
    }
}
