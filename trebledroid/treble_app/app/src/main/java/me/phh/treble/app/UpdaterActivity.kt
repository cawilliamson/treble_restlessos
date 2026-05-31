package me.phh.treble.app

import android.app.AlertDialog
import android.os.Bundle
import android.os.SystemProperties
import android.preference.PreferenceActivity
import android.text.format.Formatter
import android.util.Log
import android.widget.Button
import android.widget.ProgressBar
import android.widget.TextView
import android.view.Menu
import android.view.MenuItem
import android.view.View
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream
import java.io.IOException
import java.lang.Runnable
import java.net.URL
import java.net.HttpURLConnection
import javax.net.ssl.HttpsURLConnection
import java.time.LocalDate
import java.time.format.DateTimeFormatter
import java.time.format.FormatStyle
import kotlin.concurrent.thread
import okhttp3.*
import org.json.JSONObject
import org.json.JSONTokener
import org.tukaani.xz.XZInputStream

class UpdaterActivity : PreferenceActivity() {

    private val OTA_JSON_URL = SystemProperties.get("ro.system.ota.json_url")
    private var hasUpdate = false
    private var isUpdating = false
    private var otaJson = JSONObject()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_updater)
        actionBar?.setDisplayHomeAsUpEnabled(true)

        updateUiElements(false)
        checkUpdate()

        val btn_update = findViewById(R.id.btn_update) as Button
        btn_update.setOnClickListener {
            if (hasUpdate) {
                isUpdating = true
                downloadUpdate()
            } else {
                isUpdating = false
                checkUpdate()
            }
            return@setOnClickListener
        }
    }

    override fun onBackPressed() {
        if (isUpdating) {
            val builder = AlertDialog.Builder(this)
            builder.setTitle(getString(R.string.title_activity_updater))
            builder.setMessage(getString(R.string.prevent_exit_message))
            builder.setPositiveButton(android.R.string.yes) { _, _ ->
                super.onBackPressed()
                finish()
            }
            builder.setNegativeButton(android.R.string.no) { _, _ -> }
            builder.show()
        } else {
            super.onBackPressed()
            finish()
        }
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.menu_main, menu)
        return super.onCreateOptionsMenu(menu)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        when (item.itemId) {
            R.id.menu_delete_ota -> {
                Log.e("PHH", "Deleting OTA file")
                val builder = AlertDialog.Builder(this)
                builder.setTitle(getString(R.string.warning_dialog_title))
                builder.setMessage(getString(R.string.delete_ota_message))
                builder.setPositiveButton(android.R.string.yes) { _, _ ->
                    Log.e("PHH", "Delete in progress")
                    SystemProperties.set("sys.phh.uninstall-ota", "true");
                }
                builder.setNegativeButton(android.R.string.no) { _, _ -> 
                    Log.e("PHH", "Delete canceled")
                }
                builder.show()
            }
        }
        return super.onOptionsItemSelected(item)
    }

    private fun checkUpdate() {
        val btn_update = findViewById(R.id.btn_update) as Button
        btn_update.setVisibility(View.INVISIBLE)

        val progress_bar = findViewById(R.id.progress_horizontal) as ProgressBar
        val progress_text = findViewById(R.id.progress_value) as TextView
        progress_bar.setVisibility(View.INVISIBLE)
        progress_text.setVisibility(View.INVISIBLE)

        val update_title = findViewById(R.id.txt_update_title) as TextView
        update_title.text = getString(R.string.checking_update_title)

        if (isDynamic()) {
            isMagiskInstalled()
            Log.e("PHH", "Updating OTA info at: " + OTA_JSON_URL)
            val request = Request.Builder().url(OTA_JSON_URL).build()
            OkHttpClient().newCall(request).enqueue(object: Callback {
                override fun onFailure(call: Call, e: IOException) {
                    Log.e("PHH", "Failed downloading OTA info. Error: " + e.toString(), e)
                    runOnUiThread(Runnable {
                        hasUpdate = false
                        updateUiElements(false)
                    })
                }
                override fun onResponse(call: Call, response: Response) {
                    Log.e("PHH", "Got response")
                    if ((response.code == 200 || response.code == 304) && response.body != null) {
                        val body = response.body?.string()
                        Log.e("PHH", "Response body: " + body)
                        otaJson = JSONTokener(body).nextValue() as JSONObject
                        runOnUiThread(Runnable {
                            hasUpdate = existsUpdate()
                            updateUiElements(false)
                        })
                    } else {
                        Log.e("PHH", "Invalid HTTP response or body. Code: " + response.code)
                        runOnUiThread(Runnable {
                            hasUpdate = false
                            updateUiElements(false)
                        })
                    }
                }
            })
        } else {
            hasUpdate = false
            updateUiElements(false)
        }
    }

    private fun updateUiElements(wasUpdated: Boolean) {
        val btn_update = findViewById(R.id.btn_update) as Button

        if (!wasUpdated) {
            btn_update.setVisibility(View.VISIBLE)
        }

        val update_title = findViewById(R.id.txt_update_title) as TextView
        val update_description = findViewById(R.id.txt_update_description) as TextView
        var update_description_text = "Android version: " + getAndroidVersion() + "\n"
        update_description_text += "GSI variant: " + getVariant() + "\n"
        update_description_text += "Security patch: " + getPatchDate() + "\n\n"

        if (hasUpdate) {
            update_description_text += "Update version: " + getUpdateVersion() + "\n"
            update_description_text += "Download size: " + getUpdateSize()
            update_title.text = getString(R.string.update_found_title)
            btn_update.text = getString(R.string.update_found_button)
        } else if (!wasUpdated) {
            update_title.text = getString(R.string.update_not_found_title)
            btn_update.text = getString(R.string.update_not_found_button)
        }
        update_description.text = update_description_text
    }

    private fun getAndroidVersion() : String {
        return SystemProperties.get("ro.system.build.version.release")
    }

    private fun getPatchDate() : String {
        val patchDate = SystemProperties.get("ro.build.version.security_patch")
        Log.e("PHH", "Security patch date: " + patchDate)
        val localDate = LocalDate.parse(patchDate, DateTimeFormatter.ofPattern("yyyy-MM-dd"))
        return localDate.format(DateTimeFormatter.ofLocalizedDate(FormatStyle.LONG))
    }

    private fun getUpdateVersion() : String {
        if (otaJson.length() > 0) {
            return otaJson.getString("version")
        }
        Log.e("PHH", "OTA json is empty")
        return ""
    }

    private fun getUpdateSize() : String {
        if (otaJson.length() > 0) {
            var otaVariants = otaJson.getJSONArray("variants")
            Log.e("PHH", "OTA variants found: " + otaVariants.length())
            for (i in 0 until otaVariants.length()) {
                val otaVariant = otaVariants.get(i) as JSONObject
                val otaVariantName = otaVariant.getString("name")
                Log.e("PHH", "OTA variant found: " + otaVariantName)
                if (otaVariantName == getVariant()) {
                    Log.e("PHH", "OTA variant is the same")
                    val otaSize = otaVariant.getString("size")
                    Log.e("PHH", "OTA variant size: " + otaSize)
                    return Formatter.formatShortFileSize(
                        this.applicationContext,
                        otaSize.toLong()
                    )
                }
            }
        } else {
            Log.e("PHH", "OTA json is empty")
        }
        return ""
    }

    private fun isDynamic() : Boolean {
        val isDynamic = SystemProperties.get("ro.boot.dynamic_partitions")
        if (isDynamic != "true") {
            Log.e("PHH", "Device is not dynamic")
            val builder = AlertDialog.Builder(this)
            builder.setTitle(getString(R.string.error_dialog_title))
            builder.setMessage(getString(R.string.dynamic_device_message))
            builder.setPositiveButton(android.R.string.ok) { _, _ -> }
            builder.show()
            return false
        }
        Log.e("PHH", "Device is dynamic")
        return true
    }

    private fun isMagiskInstalled() {
        val magiskBin = File("/system/bin/magisk")
        if (magiskBin.exists()) {
            Log.e("PHH", "Magisk is installed")
            val builder = AlertDialog.Builder(this)
            builder.setTitle(getString(R.string.warning_dialog_title))
            builder.setMessage(getString(R.string.magisk_exists_message))
            builder.setPositiveButton(android.R.string.ok) { _, _ -> }
            builder.show()
        }
    }

    private fun existsUpdate() : Boolean {
        if (otaJson.length() > 0) {
            var otaDate = otaJson.getString("date")
            Log.e("PHH", "OTA image date: " + otaDate)
            val buildDate = SystemProperties.get("ro.system.build.date.utc")
            Log.e("PHH", "System image date: " + buildDate)
            if (otaDate > buildDate) {
                Log.e("PHH", "System image date is newer than the currently installed")
                return true
            }
            Log.e("PHH", "System image date is older than the currently installed")
        } else {
            Log.e("PHH", "OTA json is empty")
        }
        return false
    }

    private fun getVariant() : String {
        var flavor = SystemProperties.get("ro.build.flavor").replace(Regex("-user(debug)?"), "")
        val secure = File("/system/phh/secure")
        var vndklite = File("/system_ext/apex/com.android.vndk.v29/lib64/vendor.qti.qcril.am@1.0.so")
        if (flavor.contains("_a64_")) {
            vndklite = File("/system_ext/apex/com.android.vndk.v29/lib/libstdc++.so")
        }
        if (secure.exists()) {
            flavor += "-secure"
        } else if (vndklite.exists()) {
            flavor += "-vndklite"
        }
        Log.e("PHH", "Device variant is: " + flavor)
        return flavor
    }

    private fun getUrl() : String {
        if (otaJson.length() > 0) {
            var otaVariants = otaJson.getJSONArray("variants")
            Log.e("PHH", "OTA variants found: " + otaVariants.length())
            for (i in 0 until otaVariants.length()) {
                val otaVariant = otaVariants.get(i) as JSONObject
                val otaVariantName = otaVariant.getString("name")
                Log.e("PHH", "OTA variant found: " + otaVariantName)
                if (otaVariantName == getVariant()) {
                    val url = otaVariant.getString("url")
                    Log.d("PHH", "OTA URL: " + url)
                    return url
                }
            }
        } else {
            Log.e("PHH", "OTA json is empty")
        }
        return ""
    }

    private fun downloadUpdate() {
        val progress_bar = findViewById(R.id.progress_horizontal) as ProgressBar
        val progress_text = findViewById(R.id.progress_value) as TextView

        val btn_update = findViewById(R.id.btn_update) as Button
        btn_update.setVisibility(View.INVISIBLE)

        val url = getUrl()
        if (url.isEmpty()) {
            Log.d("PHH", "Empty URL")
            val builder = AlertDialog.Builder(this)
            builder.setTitle(getString(R.string.error_dialog_title))
            builder.setMessage(getString(R.string.update_error_message))
            builder.setPositiveButton(android.R.string.ok) { _, _ -> }
            builder.show()
            isUpdating = false
            hasUpdate = false
            updateUiElements(false)
        } else {
            Log.d("PHH", "Got URL: " + url)
            try {
                thread {
                    var httpsConnection = URL(url).openConnection() as HttpsURLConnection
                    if (httpsConnection.responseCode == HttpURLConnection.HTTP_MOVED_PERM ||
                        httpsConnection.responseCode == HttpURLConnection.HTTP_MOVED_TEMP ||
                        httpsConnection.responseCode == HttpURLConnection.HTTP_SEE_OTHER) {
                        val newUrl = httpsConnection.getHeaderField("Location")
                        httpsConnection = URL(newUrl).openConnection() as HttpsURLConnection
                    }
                    val completeFileSize = httpsConnection.contentLengthLong
                    Log.d("PHH", "Download size is: " + completeFileSize)
                    httpsConnection.inputStream.use { httpStream ->
                        var hasSuccess = false
                        try {
                            Log.e("PHH", "OTA image install started")
                            prepareOTA()
                            Log.e("PHH", "New slot created")
                            Log.e("PHH", "OTA image extracting")
                            extractUpdate(httpStream, completeFileSize)
                            Log.e("PHH", "OTA image extracted")
                            applyUpdate()
                            Log.e("PHH", "Slot switch made")
                            Log.e("PHH", "OTA image install finished")
                            hasSuccess = true
                        } catch (e: Exception) {
                            Log.e("PHH", "Failed applying OTA image. Error: " + e.toString(), e)
                        }
                        runOnUiThread(Runnable {
                            val builder = AlertDialog.Builder(this)
                            if (hasSuccess) {
                                builder.setTitle(getString(R.string.title_activity_updater))
                                builder.setMessage(getString(R.string.success_install_message))
                            } else {
                                progress_bar.setVisibility(View.GONE)
                                progress_text.setVisibility(View.GONE)
                                builder.setTitle(getString(R.string.error_dialog_title))
                                builder.setMessage(getString(R.string.failed_install_message))
                            }
                            builder.setPositiveButton(android.R.string.ok) { _, _ -> }
                            builder.show()
                            isUpdating = false
                            hasUpdate = false
                            updateUiElements(true)
                        })
                    }
                }
            } catch (e: Exception) {
                Log.e("PHH", "Failed downloading OTA image. Error: " + e.toString(), e)
                progress_bar.setVisibility(View.GONE)
                progress_text.setVisibility(View.GONE)
                val builder = AlertDialog.Builder(this)
                builder.setTitle(getString(R.string.error_dialog_title))
                builder.setMessage(getString(R.string.failed_download_message))
                builder.setPositiveButton(android.R.string.ok) { _, _ -> }
                builder.show()
                isUpdating = false
                hasUpdate = false
                updateUiElements(false)
            }
        }
    }

    private fun prepareOTA() {
        val progress_bar = findViewById(R.id.progress_horizontal) as ProgressBar
        val progress_text = findViewById(R.id.progress_value) as TextView
        val update_title = findViewById(R.id.txt_update_title) as TextView

        runOnUiThread(Runnable {
            update_title.text = getString(R.string.preparing_update_title)
            progress_bar.setIndeterminate(true)
            progress_text.text = "Preparing storage for OTA..."
            progress_bar.setVisibility(View.VISIBLE)
            progress_text.setVisibility(View.VISIBLE)
        })

        SystemProperties.set("ctl.start", "phh-ota-make")
        Thread.sleep(1000)

        while (!SystemProperties.get("init.svc.phh-ota-make", "").equals("stopped")) {
            val state = SystemProperties.get("init.svc.phh-ota-make", "not-defined")
            Log.d("PHH", "Current value of phh-ota-make svc is " + state)
            Thread.sleep(100)
        }
    }

    private fun extractUpdate(stream: InputStream, completeFileSize: Long) {
        val progress_bar = findViewById(R.id.progress_horizontal) as ProgressBar
        val progress_text = findViewById(R.id.progress_value) as TextView
        val update_title = findViewById(R.id.txt_update_title) as TextView

        runOnUiThread(Runnable {
            update_title.text = getString(R.string.downloading_update_title)
            progress_bar.setIndeterminate(false)
            progress_bar.setProgress(0)
            progress_text.text = "Downloading 0%"
            progress_bar.setVisibility(View.VISIBLE)
            progress_text.setVisibility(View.VISIBLE)
        })

        val xzOut = XZInputStream(object: InputStream() {
            var nBytesRead = 0L
            override fun read(): Int {
                nBytesRead++
                return stream.read()
            }

            override fun available(): Int {
                return stream.available()
            }

            override fun close() {
                return stream.close()
            }

            override fun mark(readlimit: Int) {
                return stream.mark(readlimit)
            }

            override fun markSupported(): Boolean {
                return stream.markSupported()
            }

            override fun read(b: ByteArray?): Int {
                val n = stream.read(b)
                nBytesRead += n
                return n
            }

            override fun read(b: ByteArray?, off: Int, len: Int): Int {
                val n = stream.read(b, off, len)
                nBytesRead += n
                var extProgress = (100 * nBytesRead) / completeFileSize
                runOnUiThread(Runnable {
                    if (extProgress < 100) {
                        progress_bar.setProgress(extProgress.toInt())
                        progress_text.text = "Downloading " + extProgress.toInt().toString() + "%"
                    }
                })
                return n
            }

            override fun reset() {
                return stream.reset()
            }

            override fun skip(n: Long): Long {
                return stream.skip(n)
            }
        })
        FileOutputStream("/dev/phh-ota").use { blockDev ->
            val extBuf = ByteArray(1024 * 1024)
            var totalWritten = 0L
            while (true) {
                val extRead = xzOut.read(extBuf)
                if (extRead == -1) break
                blockDev.write(extBuf, 0, extRead)
                totalWritten += extRead
                Log.d("PHH", "Total written to block dev is " + totalWritten)
            }
        }

        runOnUiThread(Runnable {
            progress_bar.setProgress(100)
            progress_text.text = "100%"
        })
    }

    private fun applyUpdate() {
        val progress_bar = findViewById(R.id.progress_horizontal) as ProgressBar
        val progress_text = findViewById(R.id.progress_value) as TextView
        val update_title = findViewById(R.id.txt_update_title) as TextView

        runOnUiThread(Runnable {
            update_title.text = getString(R.string.applying_update_title)
            progress_text.text = "Switching slot..."
            progress_bar.setVisibility(View.VISIBLE)
            progress_text.setVisibility(View.VISIBLE)
        })

        SystemProperties.set("ctl.start", "phh-ota-switch")
        Thread.sleep(1000)

        while (!SystemProperties.get("init.svc.phh-ota-switch", "").equals("stopped")) {
            val state = SystemProperties.get("init.svc.phh-ota-switch", "not-defined")
            Log.d("PHH", "Current value of phh-ota-switch svc is " + state)
            Thread.sleep(100)
        }

        runOnUiThread(Runnable {
            progress_text.text = "Switched slot. OTA finalized."
        })
    }
}
