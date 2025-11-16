"use strict";
const statsFields = {};
document.querySelectorAll("[data-field]").forEach((node) => {
    const field = node.getAttribute("data-field");
    if (field) {
        statsFields[field] = node;
    }
});
const refreshButton = document.querySelector("[data-role='refresh']");
const torrentsContainer = document.querySelector("[data-role='torrents']");
const emptyState = document.querySelector("[data-role='empty-state']");
const toast = document.querySelector("[data-role='toast']");
const addForm = document.querySelector("#add-form");
const magnetInput = document.querySelector("#magnet-input");
const nameInput = document.querySelector("#name-input");
const sizeInput = document.querySelector("#size-input");
const appState = {
    snapshot: null,
    ws: null,
    reconnectHandle: null
};
function formatBytes(value) {
    if (!value || value <= 0) {
        return "0 B";
    }
    const units = ["B", "KB", "MB", "GB", "TB"];
    let count = value;
    let unit = 0;
    while (count >= 1024 && unit < units.length - 1) {
        count /= 1024;
        unit++;
    }
    const precision = count >= 10 || unit === 0 ? 0 : 1;
    return `${count.toFixed(precision)} ${units[unit]}`;
}
function formatRate(value) {
    if (!value || value <= 0) {
        return "0 B/s";
    }
    return `${formatBytes(value)}/s`;
}
function formatProgress(value) {
    const pct = Math.max(0, Math.min(1, value || 0));
    return `${(pct * 100).toFixed(1)}%`;
}
function torrentStatus(torrent) {
    if (torrent.complete) {
        return "Complete";
    }
    if (torrent.paused) {
        return "Paused";
    }
    return "Downloading";
}
function showToast(message, kind = "info") {
    if (!toast) {
        return;
    }
    toast.textContent = message;
    toast.setAttribute("data-kind", kind);
    toast.classList.add("toast--visible");
    window.setTimeout(() => toast.classList.remove("toast--visible"), 2500);
}
function updateStats(stats) {
    var _a, _b, _c, _d, _e;
    (_a = statsFields["port"]) === null || _a === void 0 ? void 0 : _a.replaceChildren(document.createTextNode(stats.port.toString()));
    (_b = statsFields["torrent_count"]) === null || _b === void 0 ? void 0 : _b.replaceChildren(document.createTextNode(stats.torrent_count.toString()));
    (_c = statsFields["active"]) === null || _c === void 0 ? void 0 : _c.replaceChildren(document.createTextNode(stats.active.toString()));
    (_d = statsFields["download_rate"]) === null || _d === void 0 ? void 0 : _d.replaceChildren(document.createTextNode(formatRate(stats.download_rate)));
    (_e = statsFields["upload_rate"]) === null || _e === void 0 ? void 0 : _e.replaceChildren(document.createTextNode(formatRate(stats.upload_rate)));
}
function createActionButton(label, action, torrentId, disabled = false) {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "torrent__action";
    button.textContent = label;
    button.disabled = disabled;
    button.addEventListener("click", () => handleTorrentAction(action, torrentId, button));
    return button;
}
function renderTorrent(torrent) {
    const wrapper = document.createElement("article");
    wrapper.className = "torrent";
    if (torrent.complete) {
        wrapper.classList.add("torrent--complete");
    }
    else if (torrent.paused) {
        wrapper.classList.add("torrent--paused");
    }
    const header = document.createElement("div");
    header.className = "torrent__header";
    const titleBlock = document.createElement("div");
    const title = document.createElement("h3");
    title.textContent = torrent.name || `Torrent #${torrent.id}`;
    titleBlock.appendChild(title);
    const meta = document.createElement("p");
    meta.className = "torrent__meta";
    const sizeText = torrent.size > 0 ? formatBytes(torrent.size) : "Unknown size";
    meta.textContent = `ID ${torrent.id} • ${sizeText}`;
    titleBlock.appendChild(meta);
    header.appendChild(titleBlock);
    const actions = document.createElement("div");
    actions.className = "torrent__actions";
    if (!torrent.complete || torrent.paused) {
        const toggleAction = torrent.paused ? "resume" : "pause";
        const toggleLabel = torrent.paused ? "Resume" : "Pause";
        actions.appendChild(createActionButton(toggleLabel, toggleAction, torrent.id));
    }
    else {
        actions.appendChild(createActionButton("Restart", "resume", torrent.id, false));
    }
    actions.appendChild(createActionButton("Remove", "remove", torrent.id));
    header.appendChild(actions);
    const progress = document.createElement("div");
    progress.className = "progress";
    const fill = document.createElement("div");
    fill.className = "progress__fill";
    fill.style.width = formatProgress(torrent.progress);
    progress.appendChild(fill);
    const progressLabel = document.createElement("div");
    progressLabel.className = "progress__label";
    progressLabel.textContent = formatProgress(torrent.progress);
    const details = document.createElement("div");
    details.className = "torrent__details";
    const downloaded = document.createElement("span");
    const downloadedTotal = torrent.size > 0 ? formatBytes(torrent.size) : "unknown";
    downloaded.textContent = `Downloaded ${formatBytes(torrent.downloaded)} / ${downloadedTotal}`;
    const rates = document.createElement("span");
    rates.textContent = `↓ ${formatRate(torrent.download_rate)} ↑ ${formatRate(torrent.upload_rate)}`;
    const status = document.createElement("span");
    status.textContent = torrentStatus(torrent);
    details.append(downloaded, rates, status);
    wrapper.append(header, progress, progressLabel, details);
    return wrapper;
}
function renderTorrents(torrents) {
    if (!torrentsContainer || !emptyState) {
        return;
    }
    torrentsContainer.replaceChildren();
    if (torrents.length === 0) {
        emptyState.hidden = false;
        return;
    }
    emptyState.hidden = true;
    const fragment = document.createDocumentFragment();
    torrents.forEach((torrent) => fragment.appendChild(renderTorrent(torrent)));
    torrentsContainer.appendChild(fragment);
}
async function handleTorrentAction(action, torrentId, button) {
    button.disabled = true;
    try {
        const path = action === "remove"
            ? `/api/torrents/${torrentId}`
            : `/api/torrents/${torrentId}/${action}`;
        const method = action === "remove" ? "DELETE" : "POST";
        const response = await fetch(path, { method });
        if (!response.ok) {
            throw new Error(`Action ${action} failed (${response.status})`);
        }
        await fetchSnapshot();
        showToast(`Torrent ${action}`, "success");
    }
    catch (error) {
        console.error(error);
        showToast("Action failed", "error");
    }
    finally {
        button.disabled = false;
    }
}
async function fetchSnapshot() {
    try {
        const response = await fetch("/api/torrents");
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }
        const snapshot = await response.json();
        appState.snapshot = snapshot;
        applySnapshot(snapshot);
    }
    catch (error) {
        console.error(error);
        showToast("Failed to load torrents", "error");
    }
}
function applySnapshot(snapshot) {
    updateStats(snapshot.stats);
    renderTorrents(snapshot.torrents);
}
function readSizeInBytes() {
    if (!sizeInput) {
        return 0;
    }
    const value = parseFloat(sizeInput.value);
    if (!Number.isFinite(value) || value <= 0) {
        return 0;
    }
    return Math.round(value * 1024 * 1024);
}
async function submitAddForm(event) {
    event.preventDefault();
    if (!magnetInput || !magnetInput.value.trim()) {
        showToast("Provide a magnet URI", "error");
        return;
    }
    const payload = { magnet: magnetInput.value.trim() };
    if (nameInput && nameInput.value.trim()) {
        payload.name = nameInput.value.trim();
    }
    const sizeBytes = readSizeInBytes();
    if (sizeBytes > 0) {
        payload.size = sizeBytes;
    }
    addForm === null || addForm === void 0 ? void 0 : addForm.classList.add("form--busy");
    try {
        const response = await fetch("/api/torrents", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(payload)
        });
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }
        showToast("Torrent added", "success");
        magnetInput.value = "";
        if (nameInput) {
            nameInput.value = "";
        }
        if (sizeInput) {
            sizeInput.value = "";
        }
        await fetchSnapshot();
    }
    catch (error) {
        console.error(error);
        showToast("Add torrent failed", "error");
    }
    finally {
        addForm === null || addForm === void 0 ? void 0 : addForm.classList.remove("form--busy");
    }
}
function connectWebSocket() {
    const protocol = window.location.protocol === "https:" ? "wss" : "ws";
    const url = `${protocol}://${window.location.host}/ws`;
    if (appState.ws) {
        appState.ws.close();
        appState.ws = null;
    }
    const socket = new WebSocket(url);
    appState.ws = socket;
    socket.addEventListener("open", () => showToast("Live updates connected", "info"));
    socket.addEventListener("message", (event) => {
        try {
            const snapshot = JSON.parse(event.data);
            appState.snapshot = snapshot;
            applySnapshot(snapshot);
        }
        catch (error) {
            console.error("Failed to parse WS payload", error);
        }
    });
    socket.addEventListener("close", () => scheduleReconnect());
    socket.addEventListener("error", () => socket.close());
}
function scheduleReconnect() {
    if (appState.reconnectHandle !== null) {
        window.clearTimeout(appState.reconnectHandle);
    }
    appState.reconnectHandle = window.setTimeout(() => connectWebSocket(), 2000);
}
addForm === null || addForm === void 0 ? void 0 : addForm.addEventListener("submit", (event) => {
    void submitAddForm(event);
});
refreshButton === null || refreshButton === void 0 ? void 0 : refreshButton.addEventListener("click", () => {
    void fetchSnapshot();
});
void fetchSnapshot();
connectWebSocket();
window.setInterval(() => void fetchSnapshot(), 8000);
