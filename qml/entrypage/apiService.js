// qml/entrypage/apiService.js
.pragma library

function getBaseUrl(serverAddress) {
    let address = serverAddress.trim();
    return address.indexOf("://") === -1 ? "http://" + address : address;
}

function verifyAvilaServer(address, callback) {
    let xhr = new XMLHttpRequest();
    let targetUrl = getBaseUrl(address);

    xhr.open("GET", targetUrl + "/api/v1/status", true);
    xhr.onreadystatechange = function() {
        if (xhr.readyState === XMLHttpRequest.DONE) {
            if (xhr.status === 200) {
                try {
                    let response = JSON.parse(xhr.responseText);
                    if (response.server_type === "avila_node") {
                        callback(true, "Connected to authentic Avila Server");
                    } else {
                        callback(false, "Security mismatch: Target is not an Avila server.");
                    }
                } catch(e) {
                    callback(false, "Invalid server footprint payload.");
                }
            } else {
                callback(false, "Server unreachable (HTTP " + xhr.status + "). Check host/port.");
            }
        }
    }
    xhr.onerror = function() {
        callback(false, "Network handshake dropped. Verify your routing/firewall.");
    }
    xhr.send();
}

function checkUsernameUniqueness(serverAddress, username, callback) {
    let xhr = new XMLHttpRequest();
    let targetUrl = getBaseUrl(serverAddress) + "/api/v1/users/check?username=" + encodeURIComponent(username);

    xhr.open("GET", targetUrl, true);
    xhr.onreadystatechange = function() {
        if (xhr.readyState === XMLHttpRequest.DONE) {
            if (xhr.status === 200) {
                let response = JSON.parse(xhr.responseText);
                callback(response.available, response.available ? "Username is available!" : "Username is already taken");
            } else {
                callback(false, "Lookup failed. System offline.");
            }
        }
    }
    xhr.send();
}

function registerUser(serverAddress, username, password, callback) {
    let xhr = new XMLHttpRequest();
    xhr.open("POST", getBaseUrl(serverAddress) + "/api/v1/users/register", true);
    xhr.setRequestHeader("Content-Type", "application/json");

    xhr.onreadystatechange = function() {
        if (xhr.readyState === XMLHttpRequest.DONE) {
            callback(xhr.status === 201);
        }
    }
    xhr.send(JSON.stringify({ "username": username, "password": password }));
}

function loginUser(serverAddress, username, password, callback) {
    let xhr = new XMLHttpRequest();
    xhr.open("POST", getBaseUrl(serverAddress) + "/api/v1/auth/login", true);
    xhr.setRequestHeader("Content-Type", "application/json");

    xhr.onreadystatechange = function() {
        if (xhr.readyState === XMLHttpRequest.DONE) {
            if (xhr.status === 200) {
                try {
                    let response = JSON.parse(xhr.responseText);
                    callback(true, response.token);
                } catch(e) {
                    callback(false, "Session allocation parsing error.");
                }
            } else {
                callback(false, "Invalid username or password credentials.");
            }
        }
    }
    xhr.send(JSON.stringify({ "username": username, "password": password }));
}
