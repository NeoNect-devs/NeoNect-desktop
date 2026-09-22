
.pragma library

function formatTime(val) {
    if (!val || val <= 0) return Qt.formatTime(new Date(), "hh:mm AP");
    var ms = (val > 100000000000) ? val : (val * 1000);
    var d = new Date(ms);
    return Qt.formatTime(d, "hh:mm AP");
}

function formatSize(bytes) {
    if (!bytes || bytes <= 0) return "0 B";
    if (bytes < 1024) return Math.round(bytes) + " B";
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB";
    if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + " MB";
    return (bytes / (1024 * 1024 * 1024)).toFixed(2) + " GB";
}

function isPlayableVideo(url, fileName) {
    var path = (fileName || url || "").toLowerCase();
    return path.endsWith(".mp4") || path.endsWith(".webm") || path.endsWith(".mov") || path.endsWith(".m4v") || path.endsWith(".avi") || path.endsWith(".mkv") || path.endsWith(".wmv");
}

function formatMediaSource(rawUrl) {
    if (!rawUrl || typeof rawUrl !== "string") return "";
    var s = rawUrl.trim();
    if (s.length === 0) return "";
    if (s.startsWith("http://") || s.startsWith("https://") || s.startsWith("file://") || s.startsWith("qrc:/") || s.startsWith("data:")) {
        return s;
    }
    // Convert backslashes to forward slashes
    s = s.replace(/\\/g, "/");
    if (s.startsWith("/")) {
        return "file://" + s;
    }
    // Windows drive letters like C:/
    if (/^[A-Za-z]:\//.test(s)) {
        return "file:///" + s;
    }
    return "file:///" + s;
}

function getFileExtension(fileName, url) {
    var name = fileName || url || "FILE";
    var idx = name.lastIndexOf('.');
    if (idx !== -1 && idx < name.length - 1) {
        return name.substring(idx + 1).toUpperCase();
    }
    return "FILE";
}

function detectMediaType(url, fileName) {
    var path = (fileName || url || "").toLowerCase();
    if (path.endsWith(".png") || path.endsWith(".jpg") || path.endsWith(".jpeg") ||
        path.endsWith(".webp") || path.endsWith(".gif") || path.endsWith(".bmp") ||
        path.endsWith(".svg") || path.endsWith(".ico") || path.endsWith(".tiff")) {
        return "image";
    }
    if (path.endsWith(".mp4") || path.endsWith(".webm") || path.endsWith(".mov") ||
        path.endsWith(".mkv") || path.endsWith(".avi") || path.endsWith(".m4v") ||
        path.endsWith(".flv") || path.endsWith(".wmv") || path.endsWith(".3gp")) {
        return "video";
    }
    if (path.endsWith(".mp3") || path.endsWith(".wav") || path.endsWith(".ogg") ||
        path.endsWith(".flac") || path.endsWith(".m4a") || path.endsWith(".aac") ||
        path.endsWith(".opus") || path.endsWith(".wma")) {
        return "audio";
    }
    return "file";
}

function getAvatarColor(sender) {
    if (!sender || sender === "") return "#4F545C";
    if (sender === "System") return "#14B8A6";
    var hash = 0;
    for (var i = 0; i < sender.length; i++) {
        hash = sender.charCodeAt(i) + ((hash << 5) - hash);
    }
    var c = (hash & 0x00FFFFFF).toString(16).toUpperCase();
    return "#" + "00000".substring(0, 6 - c.length) + c;
}

function isRTL(text) {
    if (!text) return false;
    return /[\u0600-\u06FF\u0750-\u077F\u0590-\u05FF\uFB50-\uFDFF\uFE70-\uFEFF]/.test(text);
}

