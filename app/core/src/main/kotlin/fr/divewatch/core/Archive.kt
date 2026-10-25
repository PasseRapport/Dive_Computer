package fr.divewatch.core

import java.io.InputStream
import java.io.OutputStream
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream
import java.util.zip.ZipOutputStream

/** Validated in memory before the caller mutates its database. No archive path is extracted. */
object SafeArchive {
    private const val MAX_FILE=20*1024*1024
    private const val MAX_TOTAL=128*1024*1024
    fun write(output: OutputStream, files: Map<String,ByteArray>) {
        require(files.size<=2000 && files.values.sumOf { it.size.toLong() }<=MAX_TOTAL)
        ZipOutputStream(output).use { zip -> files.toSortedMap().forEach { (name,bytes) ->
            require(validName(name) && bytes.size<=MAX_FILE)
            zip.putNextEntry(ZipEntry(name).apply { time=0 });zip.write(bytes);zip.closeEntry()
        } }
    }
    fun read(input: InputStream): Map<String,ByteArray> {
        val files=linkedMapOf<String,ByteArray>();var total=0
        ZipInputStream(input).use { zip ->
            while(true) {
                val entry=zip.nextEntry?:break
                require(!entry.isDirectory && validName(entry.name) && entry.name !in files && files.size<2000) { "Archive invalide" }
                val buffer=java.io.ByteArrayOutputStream()
                val chunk=ByteArray(8192)
                while(buffer.size()<=MAX_FILE) { val n=zip.read(chunk,0,minOf(chunk.size,MAX_FILE+1-buffer.size()));if(n<0)break;buffer.write(chunk,0,n) }
                val data=buffer.toByteArray();total+=data.size
                require(data.size<=MAX_FILE && total<=MAX_TOTAL) { "Archive trop volumineuse" }
                files[entry.name]=data;zip.closeEntry()
            }
        }
        require(files.isNotEmpty());return files
    }
    private fun validName(name: String)=name.matches(Regex("[a-zA-Z0-9_./-]+")) && !name.startsWith('/') && name.split('/').none { it.isEmpty()||it==".."||it=="." }
}
