package fr.divewatch.core

import java.io.File
import java.nio.file.Files
import kotlinx.coroutines.runBlocking
import kotlin.test.*

class PythonInteropTest {
    private fun server(scenario: String="normal"): Process {
        val path=File(System.getProperty("fixtures")).parentFile.parentFile.resolve("simulator/stdio_server.py")
        return ProcessBuilder("python3",path.absolutePath,"--scenario",scenario).redirectError(ProcessBuilder.Redirect.INHERIT).start()
    }
    private fun link(process: Process): WatchLink {
        val input=process.inputStream.bufferedReader();val output=process.outputStream.bufferedWriter()
        return object: WatchLink { override suspend fun exchange(frame: Frame): Frame {
            output.write(frame.encode().hex());output.newLine();output.flush()
            val line=input.readLine()?:error("Python server terminated")
            val bytes=line.chunked(2).map{it.toInt(16).toByte()}.toByteArray()
            return FrameDecoder().feed(bytes).single()
        } }
    }
    @Test fun kotlinImportsAllThreeDivesFromIndependentPythonServer()=runBlocking {
        val process=server()
        val dir=Files.createTempDirectory("divewatch-interop").toFile()
        try {
            val client=WatchClient(link(process));val watch=client.hello();assertTrue(watch.simulated)
            assertEquals(3,client.status().completedDives)
            val catalog=client.catalog();assertEquals(3,catalog.size)
            for(entry in catalog) {
                val manifest=client.manifest(entry.id);val dive=Importer(client,BlockStore(dir)).download(watch,manifest)
                assertEquals(entry.id,dive.id);assertContentEquals(entry.hash,sha256(DiveCodec.encode(dive)))
            }
            assertTrue(client.supportsErase())
            assertEquals(3,client.erase(erasePlan(watch.id,catalog)))
            assertTrue(client.catalog().isEmpty())
            assertEquals(0,client.status().completedDives)
        } finally {process.destroy();dir.deleteRecursively()}
    }
    @Test fun independentCorruptionIsRejectedThenCanResume()=runBlocking {
        val process=server("corrupt-block");val dir=Files.createTempDirectory("divewatch-corrupt").toFile()
        try {
            val client=WatchClient(link(process));val watch=client.hello();val m=client.catalog().last();val store=BlockStore(dir)
            assertFails { Importer(client,store).download(watch,m) }
            assertEquals(0,store.nextOffset(m))
            assertEquals(m.id,Importer(client,store).download(watch,m).id)
        } finally {process.destroy();dir.deleteRecursively()}
    }
    @Test fun independentServerRejectsDeletionDuringADiveAndForAnotherWatch()=runBlocking {
        val process=server("active-dive")
        try {
            val client=WatchClient(link(process));val info=client.hello();val plan=erasePlan(info.id,client.catalog())
            assertEquals(3,assertFailsWith<WatchCommandException> {client.erase(plan)}.code)
            assertEquals(3,client.catalog().size)
            assertEquals(2,assertFailsWith<WatchCommandException> {client.erase(plan.copy(watchId=java.util.UUID.randomUUID()))}.code)
        } finally {process.destroy()}
    }
    @Test fun olderPeripheralWithoutEraseSupportIsNeverSentAnEraseCommand()=runBlocking {
        val ops=mutableListOf<Int>()
        val client=WatchClient(object: WatchLink {override suspend fun exchange(frame: Frame): Frame {
            ops+=frame.opcode
            return Frame(frame.opcode,frame.requestId,3,Writer().u16(1).result())
        }})
        assertFalse(client.supportsErase());assertEquals(listOf(7),ops)
    }
}
