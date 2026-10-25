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
}
