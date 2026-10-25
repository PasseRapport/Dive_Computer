package fr.divewatch.app

import androidx.room.*
import kotlinx.coroutines.flow.Flow

@Entity(tableName="dives",primaryKeys=["watchId","diveId"])
data class DiveRow(val watchId: String,val diveId: String,val hash: String,val content: ByteArray,val outingId: String)
@Entity(tableName="outings")
data class OutingRow(@PrimaryKey val id: String,val title: String,val site: String="",val notes: String="")
@Entity(tableName="imports",primaryKeys=["watchId","diveId"])
data class ImportRow(val watchId: String,val diveId: String,val hash: String,val bytes: Int,val total: Int,val state: String)
@Dao
interface CarnetDao {
    @Query("SELECT * FROM dives ORDER BY rowid DESC") fun observeDives(): Flow<List<DiveRow>>
    @Query("SELECT * FROM outings ORDER BY rowid DESC") fun observeOutings(): Flow<List<OutingRow>>
    @Query("SELECT * FROM imports WHERE state != 'importé'") fun observeImports(): Flow<List<ImportRow>>
    @Query("SELECT * FROM dives") suspend fun allDives(): List<DiveRow>
    @Query("SELECT * FROM outings") suspend fun allOutings(): List<OutingRow>
    @Query("SELECT * FROM imports WHERE state != 'importé'") suspend fun allImports(): List<ImportRow>
    @Query("SELECT * FROM dives WHERE watchId=:watch AND diveId=:id") suspend fun find(watch: String,id: String): DiveRow?
    @Insert(onConflict=OnConflictStrategy.ABORT) suspend fun insertDive(row: DiveRow)
    @Delete suspend fun deleteDive(row: DiveRow): Int
    @Query("DELETE FROM imports WHERE watchId=:watch AND diveId=:id") suspend fun deleteImport(watch: String,id: String)
    @Transaction suspend fun deleteDives(rows: List<DiveRow>): Int {
        var count=0
        rows.forEach { row ->
            count+=deleteDive(row)
            deleteImport(row.watchId,row.diveId)
        }
        return count
    }
    @Upsert suspend fun saveOuting(row: OutingRow)
    @Upsert suspend fun saveImport(row: ImportRow)
    @Query("UPDATE dives SET outingId=:outing WHERE watchId=:watch AND diveId=:id") suspend fun moveDive(watch: String,id: String,outing: String)
}
@Database(entities=[DiveRow::class,OutingRow::class,ImportRow::class],version=1,exportSchema=true)
abstract class CarnetDatabase: RoomDatabase() { abstract fun carnet(): CarnetDao }
