
.pragma library

function formatTime(secs) {
    if (!secs || secs <= 0) return Qt.formatTime(new Date(), "hh:mm AP");
    var d = new Date(secs * 1000);
    return Qt.formatTime(d, "hh:mm AP");
}

function formatSize(bytes) {
    if (!bytes || bytes === 0) return "0 B";
    if (bytes < 1024) return bytes + " B";
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(0) + " KB";
    return (bytes / (1024 * 1024)).toFixed(1) + " MB";
}

function isPlayableVideo(url, fileName) {
    var path = (fileName || url || "").toLowerCase();
    return path.endsWith(".mp4") || path.endsWith(".webm") || path.endsWith(".mov") || path.endsWith(".m4v") || path.endsWith(".avi");
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

