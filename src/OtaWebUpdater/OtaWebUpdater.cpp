#include "OtaWebUpdater.h"

#include "OtaWebUpdaterConfig.h"

#include <ESP.h>      // ESP.restart()
#include <Update.h>   // Update.begin / write / end / abort
#include <cstdlib>    // strtoul
#include <esp_ota_ops.h>
#include <esp_partition.h>

// OTA_ENABLED is defined in the project platform config.
// platform_config.h include PINS files alleen in de main .ino.
// Voor modules (zoals deze OTA updater) definiëren we MODULE_INCLUDE om
// bus/gfx multiple definition fouten te voorkomen.
#define MODULE_INCLUDE
#include "../../platform_config.h"
#undef MODULE_INCLUDE

#ifndef Serial_printf
#define Serial_printf Serial.printf
#endif

OtaWebUpdater::OtaWebUpdater()
    : server(nullptr),
      otaWritten(0),
      otaTotal(0),
      otaStarted(false) {}

void OtaWebUpdater::begin(WebServer* srv) {
    server = srv;
}

bool OtaWebUpdater::parseOtaTotalFromBody(const String& body, size_t& outTotal) {
    // Very small JSON parse for {"total":number}
    int i = body.indexOf(F("\"total\""));
    if (i < 0) return false;
    i = body.indexOf(':', i);
    if (i < 0) return false;

    const char* p = body.c_str() + i + 1;
    while (*p == ' ' || *p == '\t') p++;

    unsigned long val = 0;
    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        p++;
    }

    if (val == 0) return false;
    outTotal = (size_t)val;
    return true;
}

void OtaWebUpdater::registerRoutes(WebServer* srv) {
    server = srv;
#if OTA_ENABLED
    if (server == nullptr) return;

    // Upload page
    server->on("/update", HTTP_GET, [this]() { this->handleUpdateGet(); });

    // Start OTA (JSON body: {"total":N} or query arg: ?total=N)
    server->on("/update/start", HTTP_POST, [this]() { this->handleUpdateStart(); });

    // Chunked upload:
    //  - POST /update/chunk (multipart form-data, includes field name 'c')
    //  - server's upload callback is called for each received chunk
    server->on("/update/chunk", HTTP_POST,
                [this]() { this->handleUpdateChunkPost(); },
                [this]() { this->handleUpdateChunkUpload(); });

    // Finish OTA
    server->on("/update/end", HTTP_POST, [this]() { this->handleUpdateEnd(); });
#else
    (void)server;
#endif
}

void OtaWebUpdater::handleUpdateGet() {
#if OTA_ENABLED
    if (server == nullptr) return;
    if (!isClientConnected(server)) return;

    const char* html =
        "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Firmware update</title><style>"
        "body{font-family:sans-serif;background:#1a1a1a;color:#eee;padding:20px;max-width:500px;margin:0 auto;}"
        "h1{font-size:1.2em;} input[type=file]{margin:10px 0;}"
        "button{background:#2196F3;color:#fff;border:none;padding:12px 24px;border-radius:4px;cursor:pointer;font-size:16px;}"
        "button:disabled{opacity:0.5;cursor:not-allowed;}"
        "#msg{margin-top:15px;padding:10px;border-radius:4px;}"
        ".ok{background:#2e7d32;}.err{background:#c62828;}"
        ".progress-wrap{margin-top:12px;display:none;}"
        ".progress-bar{height:24px;background:#333;border-radius:4px;overflow:hidden;}"
        ".progress-fill{height:100%;background:#2196F3;width:0%;transition:width 0.15s;}"
        "#progressPct{margin-top:6px;font-size:14px;color:#00BCD4;}"
        "</style></head><body>"
        "<h1>Firmware update (OTA)</h1>"
        "<p>Kies een .bin bestand.</p>"
        "<p style='font-size:0.9em;color:#aaa;'>In Arduino IDE compileer je via Menu &rarr; Sketch &rarr; Export compiled Binary. "
        "Het bestand &hellip;.ino.bin staat in de build-map.</p>"
        "<p><input type='file' id='file' accept='.bin'><button type='button' id='btn' onclick='doOtaUpload()'>Upload</button></p>"
        "<div class='progress-wrap' id='progressWrap'><div class='progress-bar'>"
        "<div class='progress-fill' id='progressFill'></div></div><div id='progressPct'>0%</div></div>"
        "<div id='msg'></div>"
        "<script>"
        "function doOtaUpload(){"
        "var f=document.getElementById('file').files[0],btn=document.getElementById('btn');"
        "var wrap=document.getElementById('progressWrap'),fill=document.getElementById('progressFill');"
        "var pct=document.getElementById('progressPct'),msg=document.getElementById('msg');"
        "if(!f){msg.innerHTML='<span class=err>Kies een bestand.</span>';return;}"
        "btn.disabled=true;msg.textContent='Uploaden...';wrap.style.display='block';fill.style.width='0%';pct.textContent='0%';"
        "function setPct(w,t){var v=(t>0)?Math.min(100,Math.round(100*w/t)):0;fill.style.width=v+'%';pct.textContent=v+'%';}"
        "function fail(e){msg.innerHTML='<span class=err>'+e.message+'</span>';btn.disabled=false;wrap.style.display='none';}"
        "var CHUNK=32768;"
        "fetch('/update/start?total='+f.size,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({total:f.size})})"
        ".then(function(r){if(!r.ok)throw new Error(r.status);return r.json();}).then(function(j){if(!j.ok)throw new Error(j.error||'Start');"
        "var off=0;"
        "function next(){"
        "if(off>=f.size){return fetch('/update/end',{method:'POST'}).then(function(r){if(!r.ok)throw new Error(r.status);return r.json();})"
        ".then(function(){fill.style.width='100%';pct.textContent='100%';msg.innerHTML='<span class=ok>OK. Herstart...</span>';setTimeout(function(){location.href='/';},3000);})"
        ".catch(function(e){msg.innerHTML='<span class=err>'+e.message+'</span>';btn.disabled=false;});}"
        "var end=Math.min(off+CHUNK,f.size),fd=new FormData();fd.append('c',f.slice(off,end));"
        "return fetch('/update/chunk',{method:'POST',body:fd}).then(function(r){if(!r.ok)throw new Error(r.status);return r.json();})"
        ".then(function(j){off=end;setPct(j.written,j.total);return next();});"
        "}"
        "return next();"
        "}).catch(fail);"
        "}"
        "</script>"
        "</body></html>";

    server->send(200, "text/html", html);
#endif
}

#if OTA_ENABLED
// TEMP OTA DEBUG BEGIN
static void otaTempDebugLogRuntimePartitions() {
    Serial.printf("[OTA] free_sketch_space=%u bytes\n", (unsigned)ESP.getFreeSketchSpace());
    const esp_partition_t* run = esp_ota_get_running_partition();
    const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
    if (run) {
        Serial.printf("[OTA] running: label=%s addr=0x%08lx size=%lu\n",
                      run->label ? run->label : "?",
                      (unsigned long)run->address,
                      (unsigned long)run->size);
    } else {
        Serial.println(F("[OTA] running: (null)"));
    }
    if (next) {
        Serial.printf("[OTA] next_update: label=%s addr=0x%08lx size=%lu\n",
                      next->label ? next->label : "?",
                      (unsigned long)next->address,
                      (unsigned long)next->size);
    } else {
        Serial.println(F("[OTA] next_update: (null)"));
    }
}
// TEMP OTA DEBUG END
#endif

void OtaWebUpdater::handleUpdateStart() {
#if OTA_ENABLED
    if (server == nullptr) return;

    // TEMP OTA DEBUG BEGIN
    Serial.println(F("[OTA] /update/start: OTA upload gestart"));
    // TEMP OTA DEBUG END

    size_t total = 0;

    // Total uit URL-parameter of JSON-body (plain)
    if (server->hasArg("total")) {
        const char* ts = server->arg("total").c_str();
        char* endPtr = nullptr;
        unsigned long t = strtoul(ts, &endPtr, 10);
        if (endPtr != ts && t > 0UL) {
            total = (size_t)t;
        }
    }
    if (total == 0) {
        parseOtaTotalFromBody(server->arg("plain"), total);
    }

    if (total == 0) {
        server->send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid total\"}");
        return;
    }

    // Echte limiet = kleinste van (compile-time plafond) en (OTA-slot in huidige partitietabel).
    // ESP.getFreeSketchSpace() = grootte van het volgende app-partitie-slot (zie Esp.cpp).
    const size_t slotBytes = ESP.getFreeSketchSpace();
    size_t maxBin = (size_t)OTA_WEBUPDATER_MAX_SIZE;
    if (slotBytes > 0 && slotBytes < maxBin) {
        maxBin = slotBytes;
    }
    if (slotBytes == 0) {
        server->send(400, "application/json",
                     "{\"ok\":false,\"error\":\"no_ota_partition\",\"hint\":\"Wrong partition scheme in flash (need two app slots)\"}");
        return;
    }
    if (total > maxBin) {
        char rej[192];
        snprintf(rej, sizeof(rej),
                 "{\"ok\":false,\"error\":\"file_larger_than_ota_slot\",\"slot_max\":%u,\"requested\":%u}",
                 (unsigned)maxBin, (unsigned)total);
        server->send(400, "application/json", rej);
        return;
    }

    // TEMP OTA DEBUG BEGIN
    Serial.printf("[OTA] ontvangen firmware_size=%u (maxBin=%u slot=%u config_cap=%u)\n",
                  (unsigned)total, (unsigned)maxBin, (unsigned)slotBytes,
                  (unsigned)OTA_WEBUPDATER_MAX_SIZE);
    // TEMP OTA DEBUG END

    if (otaStarted) {
        Update.abort();
        otaStarted = false;
    }

    // TEMP OTA DEBUG BEGIN
    Serial.println(F("[OTA] vlak voor Update.begin(total, U_FLASH)"));
    otaTempDebugLogRuntimePartitions();
    // TEMP OTA DEBUG END

    const bool beginOk = Update.begin(total, U_FLASH);

    // TEMP OTA DEBUG BEGIN
    Serial.printf("[OTA] Update.begin -> %s\n", beginOk ? "OK" : "FAIL");
    // TEMP OTA DEBUG END

    if (!beginOk) {
        // TEMP OTA DEBUG BEGIN
        Serial.println(F("[OTA] Update.begin failed; Update.printError:"));
        Update.printError(Serial);
        Serial.printf("[OTA] Update.getError()=%u errorString=%s\n",
                      (unsigned)Update.getError(),
                      Update.errorString() ? Update.errorString() : "(null)");
        char errJson[160];
        snprintf(errJson, sizeof(errJson),
                 "{\"ok\":false,\"error\":\"OTA could not begin\",\"code\":%u}",
                 (unsigned)Update.getError());
        server->send(400, "application/json", errJson);
        // TEMP OTA DEBUG END
        return;
    }

    otaWritten = 0;
    otaTotal = total;
    otaStarted = true;
    Serial_printf(F("[OTA] Start: %u bytes\n"), (unsigned)total);

    char buf[80];
    snprintf(buf, sizeof(buf), "{\"ok\":true,\"total\":%u}", (unsigned)total);
    server->send(200, "application/json", buf);
#endif
}

void OtaWebUpdater::handleUpdateChunkUpload() {
#if OTA_ENABLED
    if (server == nullptr) return;

    HTTPUpload& upload = server->upload();
    static unsigned long s_lastLogKb = 0;

    if (upload.status == UPLOAD_FILE_WRITE) {
        if (otaTotal > 0 && otaWritten + upload.currentSize > otaTotal) {
            Update.abort();
            otaStarted = false;
            return;
        }

        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
            return;
        }

        otaWritten += upload.currentSize;
        unsigned long kb = (unsigned long)(otaWritten / 1024u);
        if (kb >= s_lastLogKb + 100u || (s_lastLogKb == 0 && kb >= 100u)) {
            Serial_printf(F("[OTA] %lu KB\n"), kb);
            s_lastLogKb = kb;
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        s_lastLogKb = 0;
    }
#endif
}

void OtaWebUpdater::handleUpdateChunkPost() {
#if OTA_ENABLED
    if (server == nullptr) return;

    if (Update.hasError()) {
        Update.printError(Serial);
        server->send(500, "application/json", "{\"written\":0,\"total\":0,\"error\":\"write failed\"}");
        return;
    }

    char buf[80];
    snprintf(buf, sizeof(buf), "{\"written\":%u,\"total\":%u}", (unsigned)otaWritten, (unsigned)otaTotal);
    server->send(200, "application/json", buf);
#endif
}

void OtaWebUpdater::handleUpdateEnd() {
#if OTA_ENABLED
    if (server == nullptr) return;

    otaStarted = false;

    if (!Update.end(true)) {
        Update.printError(Serial);
        server->send(500, "application/json", "{\"done\":false,\"error\":\"Update.end failed\"}");
        return;
    }

    Serial_printf(F("[OTA] Klaar: %u bytes, herstart\n"), (unsigned)otaWritten);
    server->send(200, "application/json", "{\"done\":true}");
    delay(1500);
    Serial.flush();
    ESP.restart();
#endif
}

