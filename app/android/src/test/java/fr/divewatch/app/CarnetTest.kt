package fr.divewatch.app

import androidx.room.Room
import fr.divewatch.core.*
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.File
import kotlinx.coroutines.runBlocking
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[28])
class CarnetTest {
    private lateinit var db: CarnetDatabase
    private lateinit var blocks: File
    @Before fun setUp() {
        db=Room.inMemoryDatabaseBuilder(RuntimeEnvironment.getApplication(),CarnetDatabase::class.java).allowMainThreadQueries().build()
        blocks=File(RuntimeEnvironment.getApplication().cacheDir,"blocks-test").apply { deleteRecursively();mkdirs() }
    }
    @After fun tearDown() {db.close();blocks.deleteRecursively()}
    private suspend fun seed(index: Int=0) {
        val dive=Demo.dives()[index];val blob=DiveCodec.encode(dive)
        db.carnet().saveOuting(OutingRow("outing","Sortie test","Carrière","Note originale"))
        db.carnet().insertDive(DiveRow(dive.watchId.toString(),dive.id.toString(),sha256(blob).hex(),blob,"outing"))
    }
    private suspend fun backup(): ByteArray {val out=ByteArrayOutputStream();Archives(db,blocks).write(out);return out.toByteArray()}
    @Test fun deletingDivesUsesBothIdentifiersAndPreservesOtherDivesAndNotes()=runBlocking {
        seed(0);seed(1)
        val target=db.carnet().allDives().first()
        val otherWatch=target.copy(watchId=java.util.UUID.randomUUID().toString())
        db.carnet().insertDive(otherWatch)
        db.carnet().saveImport(ImportRow(target.watchId,target.diveId,target.hash,100,100,"importé"))
        assertEquals(1,db.carnet().deleteDives(listOf(target)))
        assertNull(db.carnet().find(target.watchId,target.diveId))
        assertEquals(2,db.carnet().allDives().size)
        assertNotNull(db.carnet().find(otherWatch.watchId,otherWatch.diveId))
        assertEquals("Note originale",db.carnet().allOutings().single().notes)
        val cursor=db.openHelper.readableDatabase.query("SELECT count(*) FROM imports")
        cursor.use {assertTrue(it.moveToFirst());assertEquals(0,it.getInt(0))}
        assertEquals(0,db.carnet().deleteDives(listOf(target)))
    }
    @Test fun repeatedRestoreDoesNotDuplicateDivesOrLoseNotes()= runBlocking {
        seed();val bytes=backup()
        repeat(2){Archives(db,blocks).restore(ByteArrayInputStream(bytes))}
        assertEquals(1,db.carnet().allDives().size)
        assertEquals("Note originale",db.carnet().allOutings().single().notes)
    }
    @Test fun changedAnnotationsAreKeptOnce()= runBlocking {
        seed();val original=backup()
        db.carnet().saveOuting(OutingRow("outing","Sortie test","Carrière","Nouvelle note"))
        repeat(2){Archives(db,blocks).restore(ByteArrayInputStream(original))}
        val notes=db.carnet().allOutings().single().notes
        assertTrue(notes.contains("Nouvelle note"));assertTrue(notes.contains("Note originale"))
        assertEquals(1,Regex("Annotation restaurée").findAll(notes).count())
    }
    @Test fun corruptBackupCannotMutateTheCarnet()= runBlocking {
        seed();val files=SafeArchive.read(ByteArrayInputStream(backup())).toMutableMap()
        val path=files.keys.first{it.endsWith(".bin")};files[path]=byteArrayOf(0)
        val out=ByteArrayOutputStream();SafeArchive.write(out,files)
        var failed=false
        try{Archives(db,blocks).restore(ByteArrayInputStream(out.toByteArray()))}catch(_: Exception){failed=true}
        assertTrue(failed);assertEquals(1,db.carnet().allDives().size);assertEquals("Note originale",db.carnet().allOutings().single().notes)
    }
    @Test fun conflictingContentDoesNotOverwriteOriginal()= runBlocking {
        seed();val original=db.carnet().allDives().single()
        val modified=DiveCodec.decode(original.content).copy(events=emptyList())
        val files=SafeArchive.read(ByteArrayInputStream(backup())).toMutableMap()
        val path=files.keys.first{it.endsWith(".bin")};val data=DiveCodec.encode(modified);files[path]=data
        val index=org.json.JSONObject(files.getValue("manifest.json").toString(Charsets.UTF_8))
        index.getJSONObject("files").put(path,sha256(data).hex())
        index.getJSONArray("dives").getJSONObject(0).put("hash",sha256(data).hex())
        files["manifest.json"]=index.toString().toByteArray()
        val out=ByteArrayOutputStream();SafeArchive.write(out,files)
        var failed=false;try{Archives(db,blocks).restore(ByteArrayInputStream(out.toByteArray()))}catch(_: Exception){failed=true}
        assertTrue(failed);assertArrayEquals(original.content,db.carnet().allDives().single().content)
    }
    @Test fun csvPreservesMissingTemperatureAndUnits()= runBlocking {
        seed(1);val out=ByteArrayOutputStream();Archives(db,blocks).csv(out)
        val files=SafeArchive.read(ByteArrayInputStream(out.toByteArray()))
        assertEquals(setOf("dives.csv","samples.csv","events.csv"),files.keys)
        val samples=files.getValue("samples.csv").toString(Charsets.UTF_8)
        assertTrue(samples.contains("temperature_centi_c"))
        assertTrue(samples.lineSequence().drop(1).first { it.isNotBlank() }.endsWith(",\"\""))
    }
    @Test fun incompleteImportAndBlocksRemainIncompleteAfterRestore()= runBlocking {
        val d=Demo.dives().last();val m=DiveCodec.manifest(d);val store=BlockStore(blocks)
        store.append(m,0,DiveCodec.encode(d).copyOfRange(0,1024))
        db.carnet().saveImport(ImportRow(d.watchId.toString(),d.id.toString(),m.hash.hex(),1024,m.size,"incomplet"))
        val bytes=backup();blocks.deleteRecursively();blocks.mkdirs()
        Archives(db,blocks).restore(ByteArrayInputStream(bytes))
        assertTrue(db.carnet().allDives().isEmpty());assertEquals("incomplet",db.carnet().allImports().single().state)
        assertEquals(1024,BlockStore(blocks).nextOffset(m))
    }
}
