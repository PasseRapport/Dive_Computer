package fr.divewatch.app

import androidx.room.withTransaction
import fr.divewatch.core.*
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.InputStream
import java.io.OutputStream

class Archives(private val db: CarnetDatabase, private val blocks: File) {
    private val dao=db.carnet()
    suspend fun write(output: OutputStream) {
        val snapshot=db.withTransaction { Triple(dao.allDives(),dao.allOutings(),dao.allImports()) }
        val dives=snapshot.first;val outings=snapshot.second
        val files=linkedMapOf<String,ByteArray>()
        val rows=JSONArray();dives.forEach { d ->
            val path="dives/${d.watchId}/${d.diveId}.bin";files[path]=d.content
            rows.put(JSONObject().put("watch",d.watchId).put("id",d.diveId).put("hash",d.hash).put("outing",d.outingId).put("path",path))
        }
        val groups=JSONArray();outings.forEach { groups.put(JSONObject().put("id",it.id).put("title",it.title).put("site",it.site).put("notes",it.notes)) }
        // Include durable partial blocks; they remain resumable, never complete dives.
        blocks.walkTopDown().filter { it.isFile && it.name.endsWith(".block") }.forEach { file ->
            files["partial/"+file.relativeTo(blocks).invariantSeparatorsPath]=file.readBytes()
        }
        val partialImports=JSONArray();snapshot.third.forEach { row -> partialImports.put(JSONObject().put("watch",row.watchId).put("id",row.diveId).put("hash",row.hash).put("bytes",row.bytes).put("total",row.total)) }
        val entries=JSONObject();files.forEach { (name,bytes) -> entries.put(name,sha256(bytes).hex()) }
        files["manifest.json"]=JSONObject().put("version",1).put("dives",rows).put("outings",groups).put("imports",partialImports).put("files",entries).toString().toByteArray()
        SafeArchive.write(output,files)
    }
    suspend fun restore(input: InputStream) {
        val files=SafeArchive.read(input)
        val index=JSONObject(files["manifest.json"]?.toString(Charsets.UTF_8)?:error("Manifeste absent"))
        require(index.getInt("version")==1)
        val hashes=index.getJSONObject("files")
        require(hashes.length()==files.size-1)
        hashes.keys().forEach { name -> require(sha256(files[name]?:error("Fichier absent")).hex()==hashes.getString(name)) { "Empreinte incorrecte" } }
        val groups=index.getJSONArray("outings");val restoredGroups=List(groups.length()) { i -> groups.getJSONObject(i).let { OutingRow(it.getString("id"),it.getString("title"),it.getString("site"),it.getString("notes")) } }
        require(restoredGroups.map { it.id }.distinct().size==restoredGroups.size)
        val rows=index.getJSONArray("dives");val restoredDives=List(rows.length()) { i ->
            val row=rows.getJSONObject(i);val content=files[row.getString("path")]?:error("Plongée absente")
            val dive=DiveCodec.decode(content)
            require(dive.watchId.toString()==row.getString("watch")&&dive.id.toString()==row.getString("id")&&sha256(content).hex()==row.getString("hash"))
            require(restoredGroups.any { it.id==row.getString("outing") })
            DiveRow(dive.watchId.toString(),dive.id.toString(),row.getString("hash"),content,row.getString("outing"))
        }
        val partial=files.filterKeys { it.startsWith("partial/") }.map { (name,bytes) ->
            val path=name.removePrefix("partial/")
            require(path.matches(Regex("[a-f0-9]{64}/[0-9]+\\.block")))
            val r=Reader(bytes);val crc=r.u32();require(crc32(r.bytes(r.remaining))==crc)
            path to bytes
        }
        val importList=index.optJSONArray("imports")?:JSONArray()
        val restoredImports=List(importList.length()) { i ->
            val r=importList.getJSONObject(i)
            val row=ImportRow(r.getString("watch"),r.getString("id"),r.getString("hash"),r.getInt("bytes"),r.getInt("total"),"incomplet")
            java.util.UUID.fromString(row.watchId);java.util.UUID.fromString(row.diveId)
            require(row.hash.matches(Regex("[a-f0-9]{64}")) && row.total in 1..16*1024*1024 && row.bytes in 0..row.total)
            row
        }
        db.withTransaction {
            val existingGroups=dao.allOutings().associateBy { it.id }
            // Keep the current annotation and append an explicitly marked alternative once.
            restoredGroups.forEach { row ->
                val current=existingGroups[row.id]
                if(current==null)dao.saveOuting(row)
                else if(current!=row) {
                    val marker="\n\n[Annotation restaurée]\n${row.title}\n${row.site}\n${row.notes}"
                    if(!current.notes.contains(marker))dao.saveOuting(current.copy(notes=current.notes+marker))
                }
            }
            restoredImports.forEach { row -> if(dao.find(row.watchId,row.diveId)==null)dao.saveImport(row) }
            restoredDives.forEach { row ->
                val existing=dao.find(row.watchId,row.diveId)
                if(existing==null)dao.insertDive(row)
                else require(existing.hash==row.hash) { "Conflit de plongée : restauration annulée, original conservé" }
            }
        }
        partial.forEach { (path,bytes) ->
            val file=File(blocks,path)
            if(!file.exists())atomicWrite(file,bytes)
        }
    }
    suspend fun csv(output: OutputStream) {
        val dives=dao.allDives()
        fun field(value: Any?)="\""+(value?.toString()?.replace("\"","\"\"")?:"")+"\""
        fun row(vararg values: Any?)=values.joinToString(",") { field(it) }+"\r\n"
        val summary=StringBuilder(row("watch_id","dive_id","mode","simulated","start_utc_ms","duration_ms","max_depth_cm","mean_depth_cm"))
        val samples=StringBuilder(row("watch_id","dive_id","elapsed_ms","depth_cm","temperature_centi_c"))
        val events=StringBuilder(row("watch_id","dive_id","event_id","elapsed_ms","code","severity","value"))
        dives.forEach { saved ->
            val d=DiveCodec.decode(saved.content)
            summary.append(row(d.watchId,d.id,d.mode.name,d.simulated,d.startedAtMs,d.durationMs,d.maximumDepthCm,d.meanDepthCm))
            d.samples.forEach { samples.append(row(d.watchId,d.id,it.elapsedMs,it.depthCm,it.temperatureCentiC)) }
            d.events.forEach { events.append(row(d.watchId,d.id,it.id,it.elapsedMs,it.code,it.severity,it.value)) }
        }
        SafeArchive.write(output,mapOf("dives.csv" to summary.toString().toByteArray(),"samples.csv" to samples.toString().toByteArray(),"events.csv" to events.toString().toByteArray()))
    }
}
