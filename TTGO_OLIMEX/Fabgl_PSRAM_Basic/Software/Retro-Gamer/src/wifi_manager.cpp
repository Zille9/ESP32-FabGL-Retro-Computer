/*
 * WiFi Manager — Implementation
 * Reads wifi.json from SD, manages WiFi connection, HTTP file server, NTP
 */
#include "wifi_manager.h"

#include "debug.h"  

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include <time.h>
#include <FS.h>

// ============================================================================
//  WiFi Config
// ============================================================================
#define MAX_WIFI_NETWORKS 4
#define MAX_SSID_LEN 33
#define MAX_PASS_LEN 65

static char wifi_ssids[MAX_WIFI_NETWORKS][MAX_SSID_LEN];
static char wifi_passwords[MAX_WIFI_NETWORKS][MAX_PASS_LEN];
static int  wifi_network_count = 0;
static bool wifi_initialized = false;
static bool server_running = false;

static WebServer *httpServer = nullptr;

// ============================================================================
//  JSON Config Parser (minimal, no ArduinoJson dependency)
// ============================================================================
static bool parseWifiJson(const char *json) {
    wifi_network_count = 0;
    for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
        char ssidKey[8], passKey[12];
        snprintf(ssidKey, sizeof(ssidKey), "ssid%d", i);
        snprintf(passKey, sizeof(passKey), "password%d", i);

        // Find "ssidN": "value"
        const char *p = strstr(json, ssidKey);
        if (!p) continue;
        p = strchr(p, ':');
        if (!p) continue;
        p = strchr(p, '"');
        if (!p) continue;
        p++; // skip opening quote
        const char *end = strchr(p, '"');
        if (!end || (end - p) >= MAX_SSID_LEN) continue;
        int len = end - p;
        memcpy(wifi_ssids[i], p, len);
        wifi_ssids[i][len] = '\0';

        // Find "passwordN": "value"
        p = strstr(json, passKey);
        if (p) {
            p = strchr(p, ':');
            if (p) {
                p = strchr(p, '"');
                if (p) {
                    p++;
                    end = strchr(p, '"');
                    if (end && (end - p) < MAX_PASS_LEN) {
                        len = end - p;
                        memcpy(wifi_passwords[i], p, len);
                        wifi_passwords[i][len] = '\0';
                    }
                }
            }
        }

        if (wifi_ssids[i][0] != '\0') {
            wifi_network_count = i + 1;
        }
    }
    return wifi_network_count > 0;
}

// ============================================================================
//  WiFi Init / Connect
// ============================================================================
bool wifi_manager_init(void) {
    memset(wifi_ssids, 0, sizeof(wifi_ssids));
    memset(wifi_passwords, 0, sizeof(wifi_passwords));

    // Create directories if they don't exist
    if (!SD.exists("/retro-go")) {
        SD.mkdir("/retro-go");
    }
    if (!SD.exists("/retro-go/config")) {
        SD.mkdir("/retro-go/config");
    }

    File f = SD.open("/retro-go/config/wifi.json", FILE_READ);
    if (!f) {
        DBG_WARN("WiFi", "No wifi.json found at /retro-go/config/wifi.json");
        return false;
    }

    char buf[512];
    int len = f.readBytes(buf, sizeof(buf) - 1);
    buf[len] = '\0';
    f.close();

    if (!parseWifiJson(buf)) {
        DBG_WARN("WiFi", "No valid networks in wifi.json");
        return false;
    }

    wifi_initialized = true;
    DBG_INFO("WiFi", "Loaded %d network(s)", wifi_network_count);
    return true;
}

bool wifi_manager_connect(int network_index) {
    if (!wifi_initialized || network_index >= wifi_network_count) return false;
    if (wifi_ssids[network_index][0] == '\0') return false;

    DBG_INFO("WiFi", "Connecting to '%s'...", wifi_ssids[network_index]);
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssids[network_index], wifi_passwords[network_index]);

    int timeout = 30; // 15 seconds
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(500);
        timeout--;
        if (timeout % 4 == 0) {
            DBG_VERBOSE("WiFi", "Waiting... status=%d", WiFi.status());
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        DBG_INFO("WiFi", "Connected! IP: %s", WiFi.localIP().toString().c_str());
        return true;
    } else {
        DBG_WARN("WiFi", "Connection failed! Status: %d", WiFi.status());
        WiFi.disconnect();
        return false;
    }
}

void wifi_manager_disconnect(void) {
    if (server_running) wifi_manager_stop_server();
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    DBG_VERBOSE("WiFi", "Disconnected");
}

bool wifi_manager_is_connected(void) {
    return WiFi.status() == WL_CONNECTED;
}

const char* wifi_manager_get_ip(void) {
    static char ipBuf[16];
    if (WiFi.status() == WL_CONNECTED) {
        strncpy(ipBuf, WiFi.localIP().toString().c_str(), sizeof(ipBuf)-1);
        ipBuf[sizeof(ipBuf)-1] = '\0';
    } else {
        strcpy(ipBuf, "0.0.0.0");
    }
    return ipBuf;
}

const char* wifi_manager_get_ssid(int index) {
    if (index < 0 || index >= MAX_WIFI_NETWORKS) return "";
    return wifi_ssids[index];
}

int wifi_manager_get_network_count(void) {
    return wifi_network_count;
}

// ============================================================================
//  HTTP File Server - With recursive navigation
// ============================================================================
static String getContentType(const String &path) {
    if (path.endsWith(".htm") || path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css")) return "text/css";
    if (path.endsWith(".js"))  return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".png")) return "image/png";
    if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
    if (path.endsWith(".gif")) return "image/gif";
    if (path.endsWith(".ico")) return "image/x-icon";
    if (path.endsWith(".txt")) return "text/plain";
    if (path.endsWith(".wad")) return "application/octet-stream";
    if (path.endsWith(".nes")) return "application/octet-stream";
    if (path.endsWith(".gb")) return "application/octet-stream";
    if (path.endsWith(".gbc")) return "application/octet-stream";
    if (path.endsWith(".sms")) return "application/octet-stream";
    if (path.endsWith(".gg")) return "application/octet-stream";
    if (path.endsWith(".pce")) return "application/octet-stream";
    if (path.endsWith(".sfc")) return "application/octet-stream";
    if (path.endsWith(".smc")) return "application/octet-stream";
    if (path.endsWith(".lnx")) return "application/octet-stream";
    if (path.endsWith(".col")) return "application/octet-stream";
    if (path.endsWith(".md")) return "application/octet-stream";
    if (path.endsWith(".bin")) return "application/octet-stream";
    if (path.endsWith(".zip")) return "application/zip";
    return "application/octet-stream";
}

/**
 * Build full path from URL, sanitizing dangerous characters
 */
static String buildPath(const String &path) {
    String fullPath = path;
    // Remove dangerous characters
    fullPath.replace("..", "");
    fullPath.replace("//", "/");
    if (!fullPath.startsWith("/")) fullPath = "/" + fullPath;
    return fullPath;
}

static void handleRoot() {
    String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Retro-Gamer File Manager</title>"
        "<style>"
        "body{font-family:monospace;background:#1a1a2e;color:#e0e0e0;margin:0;padding:20px}"
        "h1{color:#00d4ff;text-align:center}"
        "a{color:#00d4ff;text-decoration:none}"
        "a:hover{text-decoration:underline}"
        ".file{padding:4px 8px;margin:2px 0;display:flex;justify-content:space-between}"
        ".file:hover{background:#16213e}"
        ".dir{color:#ffc107;font-weight:bold}"
        ".upload{background:#16213e;padding:15px;border-radius:8px;margin:10px 0}"
        "input[type=file]{color:#e0e0e0}"
        "button{background:#00d4ff;color:#000;border:none;padding:8px 16px;cursor:pointer;border-radius:4px}"
        "button:hover{background:#00b8d4}"
        ".bar{background:#16213e;padding:10px;border-radius:8px;margin-bottom:10px}"
        ".mkdir{display:inline-block;margin-left:10px}"
        ".mkdir input{background:#0d1b2a;color:#e0e0e0;border:1px solid #00d4ff;padding:4px 8px;border-radius:4px}"
        ".size{color:#666;font-size:0.9em}"
        "</style></head><body>";

    html += "<h1>🎮 Retro-Gamer File Manager</h1>";

    String path = httpServer->hasArg("dir") ? httpServer->arg("dir") : "/";
    path = buildPath(path);

    html += "<div class='bar'>📁 " + path + "</div>";

    // Upload form
    html += "<div class='upload'><form method='POST' action='/upload?dir=" + path +
            "' enctype='multipart/form-data'>"
            "<input type='file' name='file' multiple>"
            "<button type='submit'>📤 Upload</button></form>";

    // Mkdir form
    html += "<form class='mkdir' method='GET' action='/mkdir' style='display:inline-block;margin-left:10px'>"
            "<input type='hidden' name='dir' value='" + path + "'>"
            "<input type='text' name='name' placeholder='new folder' size='15'>"
            "<button type='submit'>📁 New Folder</button></form></div>";

    // Parent directory link (if not in root)
    if (path != "/") {
        String parent = path;
        if (parent.endsWith("/")) parent = parent.substring(0, parent.length() - 1);
        int lastSlash = parent.lastIndexOf('/');
        if (lastSlash > 0) {
            parent = parent.substring(0, lastSlash);
            if (parent.length() == 0) parent = "/";
        } else {
            parent = "/";
        }
        html += "<div class='file'><a class='dir' href='/?dir=" + parent + "'>📁 ..</a></div>";
    }

    // List directory
    File dir = SD.open(path.c_str());
    if (dir && dir.isDirectory()) {
        // Collect files and directories for sorting
        struct Entry {
            String name;
            String path;
            bool isDir;
            size_t size;
        };
        std::vector<Entry> entries;
        
        File entry = dir.openNextFile();
        while (entry) {
            String name = entry.name();
            String fullPath = path;
            if (!fullPath.endsWith("/")) fullPath += "/";
            fullPath += name;
            
            Entry e;
            e.name = name;
            e.path = fullPath;
            e.isDir = entry.isDirectory();
            e.size = entry.size();
            entries.push_back(e);
            entry = dir.openNextFile();
        }
        dir.close();
        
        // Sort: directories first, then files alphabetically
        std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
            if (a.isDir != b.isDir) return a.isDir > b.isDir;
            return a.name < b.name;
        });
        
        for (auto &e : entries) {
            String displayName = e.name;
            if (e.isDir) {
                html += "<div class='file'><a class='dir' href='/?dir=" + e.path + "'>📁 " + displayName + "/</a></div>";
            } else {
                String sizeStr;
                if (e.size >= 1048576) sizeStr = String(e.size / 1048576) + " MB";
                else if (e.size >= 1024) sizeStr = String(e.size / 1024) + " KB";
                else sizeStr = String(e.size) + " B";

                html += "<div class='file'><a href='/download?file=" + e.path + "'>📄 " + displayName + "</a>"
                        "<span><span class='size'>" + sizeStr + "</span> "
                        "<a href='/delete?file=" + e.path + "' "
                        "onclick='return confirm(\"Delete " + displayName + "?\")'>🗑️</a></span></div>";
            }
        }
    } else {
        html += "<div class='file' style='color:#ff6b6b'>❌ Directory not found</div>";
    }

    html += "<hr><p style='text-align:center;color:#666'>Retro-Gamer v1.0 | Free: " +
            String(SD.totalBytes() / 1048576) + "MB / " +
            String(SD.usedBytes() / 1048576) + "MB used</p>";
    html += "</body></html>";

    httpServer->send(200, "text/html", html);
}

static void handleDownload() {
    if (!httpServer->hasArg("file")) {
        httpServer->send(400, "text/plain", "Missing file parameter");
        return;
    }
    String path = httpServer->arg("file");
    path = buildPath(path);
    
    if (!SD.exists(path.c_str())) {
        httpServer->send(404, "text/plain", "File not found: " + path);
        return;
    }
    
    File f = SD.open(path.c_str(), FILE_READ);
    if (!f) {
        httpServer->send(404, "text/plain", "Cannot open file: " + path);
        return;
    }
    
    // Get just the filename for Content-Disposition
    String filename = path;
    int lastSlash = filename.lastIndexOf('/');
    if (lastSlash >= 0) filename = filename.substring(lastSlash + 1);
    
    httpServer->sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    httpServer->streamFile(f, getContentType(path));
    f.close();
}

static File uploadFile;
static String uploadDir;

static void handleUpload() {
    HTTPUpload &upload = httpServer->upload();
    if (upload.status == UPLOAD_FILE_START) {
        uploadDir = httpServer->hasArg("dir") ? httpServer->arg("dir") : "/";
        uploadDir = buildPath(uploadDir);
        if (!uploadDir.endsWith("/")) uploadDir += "/";
        String filepath = uploadDir + upload.filename;
        DBG_VERBOSE("HTTP", "Upload: %s", filepath.c_str());
        
        // Create directories if they don't exist
        String dirPath = uploadDir;
        if (dirPath.endsWith("/")) dirPath = dirPath.substring(0, dirPath.length() - 1);
        if (!SD.exists(dirPath.c_str())) {
            // Create recursively
            String current = "";
            int start = 0;
            while (start < dirPath.length()) {
                int next = dirPath.indexOf('/', start + 1);
                if (next == -1) next = dirPath.length();
                current = dirPath.substring(0, next);
                if (current.length() > 0 && !SD.exists(current.c_str())) {
                    SD.mkdir(current.c_str());
                }
                start = next;
            }
        }
        
        uploadFile = SD.open(filepath.c_str(), FILE_WRITE);
        if (!uploadFile) {
            DBG_ERROR("HTTP", "Cannot create file: %s", filepath.c_str());
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            uploadFile.write(upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            DBG_VERBOSE("HTTP", "Upload done: %u bytes", upload.totalSize);
            uploadFile.close();
        }
    }
}

static void handleUploadDone() {
    String dir = httpServer->hasArg("dir") ? httpServer->arg("dir") : "/";
    httpServer->sendHeader("Location", "/?dir=" + dir);
    httpServer->send(303);
}

static void handleDelete() {
    if (!httpServer->hasArg("file")) {
        httpServer->send(400, "text/plain", "Missing file parameter");
        return;
    }
    
    String path = httpServer->arg("file");
    path = buildPath(path);
    
    DBG_VERBOSE("HTTP", "Delete request for: %s", path.c_str());
    
    // Verify file exists
    if (!SD.exists(path.c_str())) {
        DBG_WARN("HTTP", "Delete failed: File not found: %s", path.c_str());
        httpServer->send(404, "text/plain", "File not found: " + path);
        return;
    }
    
    // Attempt to delete
    bool success = SD.remove(path.c_str());
    if (success) {
        DBG_VERBOSE("HTTP", "Deleted: %s", path.c_str());
    } else {
        DBG_WARN("HTTP", "Delete failed: %s", path.c_str());
    }
    
    // Get directory for redirect
    String dir = path;
    int lastSlash = dir.lastIndexOf('/');
    if (lastSlash > 0) {
        dir = dir.substring(0, lastSlash);
    } else {
        dir = "/";
    }
    
    httpServer->sendHeader("Location", "/?dir=" + dir);
    httpServer->send(303);
}

static void handleMkdir() {
    if (!httpServer->hasArg("name")) {
        httpServer->send(400, "text/plain", "Missing name parameter");
        return;
    }
    String dir = httpServer->hasArg("dir") ? httpServer->arg("dir") : "/";
    dir = buildPath(dir);
    if (!dir.endsWith("/")) dir += "/";
    String path = dir + httpServer->arg("name");
    path = buildPath(path);
    
    DBG_VERBOSE("HTTP", "Mkdir: %s", path.c_str());
    
    if (SD.mkdir(path.c_str())) {
        DBG_VERBOSE("HTTP", "Directory created: %s", path.c_str());
    } else {
        DBG_WARN("HTTP", "Mkdir failed: %s", path.c_str());
    }
    
    httpServer->sendHeader("Location", "/?dir=" + dir);
    httpServer->send(303);
}

bool wifi_manager_start_server(void) {
    if (!wifi_manager_is_connected()) return false;
    if (httpServer) { delete httpServer; httpServer = nullptr; }

    httpServer = new WebServer(80);
    httpServer->on("/", HTTP_GET, handleRoot);
    httpServer->on("/download", HTTP_GET, handleDownload);
    httpServer->on("/upload", HTTP_POST, handleUploadDone, handleUpload);
    httpServer->on("/delete", HTTP_GET, handleDelete);
    httpServer->on("/mkdir", HTTP_GET, handleMkdir);
    
    // Handle 404 errors
    httpServer->onNotFound([]() {
        httpServer->send(404, "text/plain", "Not found");
    });
    
    httpServer->begin();

    server_running = true;
    DBG_INFO("HTTP", "Server started on port 80");
    DBG_INFO("HTTP", "Visit http://%s/", wifi_manager_get_ip());
    return true;
}

void wifi_manager_stop_server(void) {
    if (httpServer) {
        httpServer->stop();
        delete httpServer;
        httpServer = nullptr;
    }
    server_running = false;
    DBG_VERBOSE("HTTP", "Server stopped");
}

void wifi_manager_process(void) {
    if (server_running && httpServer) {
        httpServer->handleClient();
    }
}

// ============================================================================
//  NTP Time Sync
// ============================================================================
bool wifi_manager_ntp_sync(const char *timezone) {
    if (!wifi_manager_is_connected()) return false;

    const char *tz = timezone ? timezone : "UTC0";
    configTzTime(tz, "pool.ntp.org", "time.nist.gov");

    // Wait up to 5 seconds for time sync
    struct tm timeinfo;
    for (int i = 0; i < 10; i++) {
        if (getLocalTime(&timeinfo, 500)) {
            DBG_INFO("NTP", "Time: %04d-%02d-%02d %02d:%02d:%02d",
                   timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                   timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            return true;
        }
    }
    DBG_WARN("NTP", "Sync failed");
    return false;
}