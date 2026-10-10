#include "WebServerManager.h"
#include "RouteStorage.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

static const char *AP_NAME = "Roborock_Web";
static const char *AP_PASSWORD = "roborock123";

static WebServer server(80);
static RouteStorage routeStorage;

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="sv">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Roborock</title>

<style>
* { box-sizing: border-box; }

body {
    margin: 0;
    font-family: Arial, sans-serif;
    background: #202124;
    color: white;
}

header {
    padding: 12px;
    background: #111;
    text-align: center;
    font-size: 22px;
}

#main {
    max-width: 1200px;
    margin: auto;
    padding: 12px;
}

.toolbar {
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
    margin-bottom: 10px;
}

button, .fileButton {
    padding: 10px 16px;
    border: 0;
    border-radius: 6px;
    font-size: 16px;
    cursor: pointer;
    background: #ddd;
    color: #111;
}

.fileButton input { display: none; }

#drawingInfo, #positionInfo, #status {
    margin: 8px 0;
    font-size: 15px;
}

#positionInfo {
    font-family: monospace;
    font-size: 16px;
}

#canvasContainer {
    width: 100%;
    overflow: hidden;
    background: white;
    border: 2px solid #666;
}

canvas {
    display: block;
    width: 100%;
    height: auto;
    background: #ddd;
    touch-action: none;
    cursor: crosshair;
}

canvas.dragging { cursor: grabbing; }

#status { font-weight: bold; }
</style>
</head>

<body>
<header>Roborock Route Editor</header>

<div id="main">

    <div class="toolbar">
        <label class="fileButton">
            LADDA DXF
            <input id="dxfFile" type="file" accept=".dxf">
        </label>

        <button id="fitButton">PASSA RITNING</button>
        <button id="zoomInButton">+</button>
        <button id="zoomOutButton">−</button>
        <button id="clearDrawing">TA BORT RITNING</button>
    </div>

    <div id="drawingInfo">Ingen ritning laddad</div>
    <div id="positionInfo">X: ---- mm &nbsp; Y: ---- mm &nbsp; Zoom: ---</div>

    <div id="canvasContainer">
        <canvas id="canvas" width="1200" height="800"></canvas>
    </div>

    <div id="status">Klar</div>

</div>

<script>
const canvas = document.getElementById("canvas");
const ctx = canvas.getContext("2d");
const drawingInfo = document.getElementById("drawingInfo");
const positionInfo = document.getElementById("positionInfo");
const statusText = document.getElementById("status");

let entities = [];
let bounds = null;

let scale = 1;
let fitScale = 1;
let offsetX = 0;
let offsetY = 0;

let dragging = false;
let dragStartX = 0;
let dragStartY = 0;
let dragOffsetX = 0;
let dragOffsetY = 0;

const MARGIN = 60;
const SMALL_GRID_MM = 250;
const LARGE_GRID_MM = 1000;
const MIN_ZOOM = 0.1;
const MAX_ZOOM = 50;

function setStatus(text) {
    statusText.textContent = text;
}

function pairLines(text) {
    const raw = text.replace(/\r/g, "").split("\n");
    const pairs = [];

    for (let i = 0; i + 1 < raw.length; i += 2) {
        pairs.push({
            code: parseInt(raw[i].trim()),
            value: raw[i + 1].trim()
        });
    }

    return pairs;
}

function parseDXF(text) {
    const pairs = pairLines(text);
    const result = [];

    let i = 0;
    let section = "";

    while (i < pairs.length) {
        const p = pairs[i];

        if (p.code === 0 && p.value === "SECTION") {
            if (i + 1 < pairs.length && pairs[i + 1].code === 2)
                section = pairs[i + 1].value;

            i += 2;
            continue;
        }

        if (p.code === 0 && p.value === "ENDSEC") {
            section = "";
            i++;
            continue;
        }

        if (section !== "ENTITIES" || p.code !== 0) {
            i++;
            continue;
        }

        const type = p.value;

        if (type === "LINE") {
            const e = { type: "LINE", x1: 0, y1: 0, x2: 0, y2: 0 };
            i++;

            while (i < pairs.length && pairs[i].code !== 0) {
                const q = pairs[i];

                if (q.code === 10) e.x1 = Number(q.value);
                if (q.code === 20) e.y1 = Number(q.value);
                if (q.code === 11) e.x2 = Number(q.value);
                if (q.code === 21) e.y2 = Number(q.value);

                i++;
            }

            result.push(e);
            continue;
        }

        if (type === "CIRCLE") {
            const e = { type: "CIRCLE", x: 0, y: 0, r: 0 };
            i++;

            while (i < pairs.length && pairs[i].code !== 0) {
                const q = pairs[i];

                if (q.code === 10) e.x = Number(q.value);
                if (q.code === 20) e.y = Number(q.value);
                if (q.code === 40) e.r = Number(q.value);

                i++;
            }

            result.push(e);
            continue;
        }

        if (type === "ARC") {
            const e = { type: "ARC", x: 0, y: 0, r: 0, start: 0, end: 0 };
            i++;

            while (i < pairs.length && pairs[i].code !== 0) {
                const q = pairs[i];

                if (q.code === 10) e.x = Number(q.value);
                if (q.code === 20) e.y = Number(q.value);
                if (q.code === 40) e.r = Number(q.value);
                if (q.code === 50) e.start = Number(q.value);
                if (q.code === 51) e.end = Number(q.value);

                i++;
            }

            result.push(e);
            continue;
        }

        if (type === "LWPOLYLINE") {
            const e = { type: "POLYLINE", points: [], closed: false };
            let current = null;
            i++;

            while (i < pairs.length && pairs[i].code !== 0) {
                const q = pairs[i];

                if (q.code === 70)
                    e.closed = (Number(q.value) & 1) !== 0;

                if (q.code === 10) {
                    current = { x: Number(q.value), y: 0 };
                    e.points.push(current);
                }

                if (q.code === 20 && current)
                    current.y = Number(q.value);

                i++;
            }

            if (e.points.length > 1)
                result.push(e);

            continue;
        }

        if (type === "POLYLINE") {
            const e = { type: "POLYLINE", points: [], closed: false };
            i++;

            while (i < pairs.length) {
                if (pairs[i].code === 70)
                    e.closed = (Number(pairs[i].value) & 1) !== 0;

                if (pairs[i].code === 0 && pairs[i].value === "VERTEX") {
                    const point = { x: 0, y: 0 };
                    i++;

                    while (i < pairs.length && pairs[i].code !== 0) {
                        if (pairs[i].code === 10) point.x = Number(pairs[i].value);
                        if (pairs[i].code === 20) point.y = Number(pairs[i].value);

                        i++;
                    }

                    e.points.push(point);
                    continue;
                }

                if (pairs[i].code === 0 && pairs[i].value === "SEQEND") {
                    i++;
                    break;
                }

                i++;
            }

            if (e.points.length > 1)
                result.push(e);

            continue;
        }

        i++;
    }

    return result;
}

function addBoundsPoint(b, x, y) {
    if (!Number.isFinite(x) || !Number.isFinite(y))
        return;

    b.minX = Math.min(b.minX, x);
    b.maxX = Math.max(b.maxX, x);
    b.minY = Math.min(b.minY, y);
    b.maxY = Math.max(b.maxY, y);
}

function calculateBounds() {
    const b = {
        minX: Infinity,
        minY: Infinity,
        maxX: -Infinity,
        maxY: -Infinity
    };

    for (const e of entities) {
        if (e.type === "LINE") {
            addBoundsPoint(b, e.x1, e.y1);
            addBoundsPoint(b, e.x2, e.y2);
        }

        else if (e.type === "POLYLINE") {
            for (const p of e.points)
                addBoundsPoint(b, p.x, p.y);
        }

        else if (e.type === "CIRCLE" || e.type === "ARC") {
            addBoundsPoint(b, e.x - e.r, e.y - e.r);
            addBoundsPoint(b, e.x + e.r, e.y + e.r);
        }
    }

    return Number.isFinite(b.minX) ? b : null;
}

function fitDrawing() {
    if (!bounds)
        return;

    const width = Math.max(bounds.maxX - bounds.minX, 1);
    const height = Math.max(bounds.maxY - bounds.minY, 1);

    fitScale = Math.min(
        (canvas.width - MARGIN * 2) / width,
        (canvas.height - MARGIN * 2) / height
    );

    scale = fitScale;

    const centerX = (bounds.minX + bounds.maxX) / 2;
    const centerY = (bounds.minY + bounds.maxY) / 2;

    offsetX = canvas.width / 2 - centerX * scale;
    offsetY = canvas.height / 2 + centerY * scale;

    draw();
    updatePositionInfo();
}

function worldToScreen(x, y) {
    return {
        x: x * scale + offsetX,
        y: offsetY - y * scale
    };
}

function screenToWorld(x, y) {
    return {
        x: (x - offsetX) / scale,
        y: (offsetY - y) / scale
    };
}

function getVisibleWorld() {
    const a = screenToWorld(0, canvas.height);
    const b = screenToWorld(canvas.width, 0);

    return {
        minX: Math.min(a.x, b.x),
        maxX: Math.max(a.x, b.x),
        minY: Math.min(a.y, b.y),
        maxY: Math.max(a.y, b.y)
    };
}

function drawGrid() {
    if (!bounds)
        return;

    const visible = getVisibleWorld();

    let grid = SMALL_GRID_MM;

    if (grid * scale < 12)
        grid = 1000;

    if (grid * scale < 12)
        grid = 5000;

    const startX = Math.floor(visible.minX / grid) * grid;
    const endX = Math.ceil(visible.maxX / grid) * grid;
    const startY = Math.floor(visible.minY / grid) * grid;
    const endY = Math.ceil(visible.maxY / grid) * grid;

    ctx.lineWidth = 1;

    for (let x = startX; x <= endX; x += grid) {
        const p = worldToScreen(x, 0);

        ctx.strokeStyle =
            x % LARGE_GRID_MM === 0 ? "#c7c7c7" : "#e8e8e8";

        ctx.beginPath();
        ctx.moveTo(p.x, 0);
        ctx.lineTo(p.x, canvas.height);
        ctx.stroke();
    }

    for (let y = startY; y <= endY; y += grid) {
        const p = worldToScreen(0, y);

        ctx.strokeStyle =
            y % LARGE_GRID_MM === 0 ? "#c7c7c7" : "#e8e8e8";

        ctx.beginPath();
        ctx.moveTo(0, p.y);
        ctx.lineTo(canvas.width, p.y);
        ctx.stroke();
    }
}

function drawEntities() {
    ctx.strokeStyle = "#222";
    ctx.lineWidth = 2;

    for (const e of entities) {
        if (e.type === "LINE") {
            const a = worldToScreen(e.x1, e.y1);
            const b = worldToScreen(e.x2, e.y2);

            ctx.beginPath();
            ctx.moveTo(a.x, a.y);
            ctx.lineTo(b.x, b.y);
            ctx.stroke();
        }

        else if (e.type === "POLYLINE") {
            if (e.points.length < 2)
                continue;

            let p = worldToScreen(e.points[0].x, e.points[0].y);

            ctx.beginPath();
            ctx.moveTo(p.x, p.y);

            for (let i = 1; i < e.points.length; i++) {
                p = worldToScreen(e.points[i].x, e.points[i].y);
                ctx.lineTo(p.x, p.y);
            }

            if (e.closed)
                ctx.closePath();

            ctx.stroke();
        }

        else if (e.type === "CIRCLE") {
            const c = worldToScreen(e.x, e.y);

            ctx.beginPath();
            ctx.arc(c.x, c.y, e.r * scale, 0, Math.PI * 2);
            ctx.stroke();
        }

        else if (e.type === "ARC") {
            const c = worldToScreen(e.x, e.y);

            const start = -e.end * Math.PI / 180;
            const end = -e.start * Math.PI / 180;

            ctx.beginPath();
            ctx.arc(c.x, c.y, e.r * scale, start, end);
            ctx.stroke();
        }
    }
}

function draw() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    ctx.fillStyle = "#ddd";
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    if (!bounds)
        return;

    drawGrid();
    drawEntities();
}

function zoomAt(screenX, screenY, factor) {
    if (!bounds)
        return;

    const world = screenToWorld(screenX, screenY);
    const newScale = Math.min(
        Math.max(scale * factor, fitScale * MIN_ZOOM),
        fitScale * MAX_ZOOM
    );

    scale = newScale;

    offsetX = screenX - world.x * scale;
    offsetY = screenY + world.y * scale;

    draw();
    updatePositionInfo(screenX, screenY);
}

function getCanvasPosition(event) {
    const rect = canvas.getBoundingClientRect();

    return {
        x: (event.clientX - rect.left) * canvas.width / rect.width,
        y: (event.clientY - rect.top) * canvas.height / rect.height
    };
}

function updatePositionInfo(x = null, y = null) {
    if (!bounds) {
        positionInfo.innerHTML =
            "X: ---- mm &nbsp; Y: ---- mm &nbsp; Zoom: ---";
        return;
    }

    const zoomPercent = Math.round(scale / fitScale * 100);

    if (x === null || y === null) {
        positionInfo.innerHTML =
            `X: ---- mm &nbsp; Y: ---- mm &nbsp; Zoom: ${zoomPercent}%`;
        return;
    }

    const world = screenToWorld(x, y);

    positionInfo.innerHTML =
        `X: ${world.x.toFixed(0)} mm &nbsp; ` +
        `Y: ${world.y.toFixed(0)} mm &nbsp; ` +
        `Zoom: ${zoomPercent}%`;
}

function showDrawingInfo(fileName) {
    if (!bounds) {
        drawingInfo.textContent = "Ingen geometri hittades";
        return;
    }

    const width = bounds.maxX - bounds.minX;
    const height = bounds.maxY - bounds.minY;

    drawingInfo.textContent =
        `${fileName} | ${entities.length} objekt | ` +
        `${(width / 1000).toFixed(2)} × ${(height / 1000).toFixed(2)} m`;
}

canvas.addEventListener("wheel", event => {
    if (!bounds)
        return;

    event.preventDefault();

    const p = getCanvasPosition(event);
    const factor = event.deltaY < 0 ? 1.15 : 1 / 1.15;

    zoomAt(p.x, p.y, factor);
}, { passive: false });

canvas.addEventListener("pointerdown", event => {
    if (!bounds)
        return;

    const p = getCanvasPosition(event);

    dragging = true;
    dragStartX = p.x;
    dragStartY = p.y;
    dragOffsetX = offsetX;
    dragOffsetY = offsetY;

    canvas.classList.add("dragging");
    canvas.setPointerCapture(event.pointerId);
});

canvas.addEventListener("pointermove", event => {
    if (!bounds)
        return;

    const p = getCanvasPosition(event);

    if (dragging) {
        offsetX = dragOffsetX + (p.x - dragStartX);
        offsetY = dragOffsetY + (p.y - dragStartY);
        draw();
    }

    updatePositionInfo(p.x, p.y);
});

function stopDragging(event) {
    dragging = false;
    canvas.classList.remove("dragging");

    if (event && canvas.hasPointerCapture(event.pointerId))
        canvas.releasePointerCapture(event.pointerId);
}

canvas.addEventListener("pointerup", stopDragging);
canvas.addEventListener("pointercancel", stopDragging);

canvas.addEventListener("pointerleave", () => {
    if (!dragging)
        updatePositionInfo();
});

document.getElementById("fitButton").addEventListener("click", fitDrawing);

document.getElementById("zoomInButton").addEventListener("click", () => {
    zoomAt(canvas.width / 2, canvas.height / 2, 1.25);
});

document.getElementById("zoomOutButton").addEventListener("click", () => {
    zoomAt(canvas.width / 2, canvas.height / 2, 1 / 1.25);
});

document.getElementById("dxfFile").addEventListener("change", async event => {
    const file = event.target.files[0];

    if (!file)
        return;

    setStatus("Läser DXF...");

    try {
        const text = await file.text();

        entities = parseDXF(text);
        bounds = calculateBounds();

        if (!bounds || !entities.length) {
            entities = [];
            bounds = null;
            draw();
            setStatus("Ingen stödd geometri hittades");
            return;
        }

        showDrawingInfo(file.name);
        fitDrawing();
        setStatus("DXF laddad");
    }
    catch (error) {
        console.error(error);
        setStatus("Fel vid DXF-import");
    }
});

document.getElementById("clearDrawing").addEventListener("click", () => {
    entities = [];
    bounds = null;

    scale = 1;
    fitScale = 1;
    offsetX = 0;
    offsetY = 0;

    document.getElementById("dxfFile").value = "";

    drawingInfo.textContent = "Ingen ritning laddad";
    updatePositionInfo();

    setStatus("Ritningen borttagen");
    draw();
});

draw();
</script>
</body>
</html>
)rawliteral";

static void handleRoot()
{
    server.send_P(200, "text/html", INDEX_HTML);
}

bool WebServerManager::begin()
{
    if (!routeStorage.begin())
    {
        Serial.println("RouteStorage FAILED");
        return false;
    }

    WiFi.mode(WIFI_AP);
    delay(100);

    if (!WiFi.softAP(AP_NAME, AP_PASSWORD))
    {
        Serial.println("WiFi AP FAILED");
        return false;
    }

    server.on("/", HTTP_GET, handleRoot);
    server.begin();

    Serial.println();
    Serial.println("WEB SERVER READY");
    Serial.printf("WiFi: %s\n", AP_NAME);
    Serial.printf("Password: %s\n", AP_PASSWORD);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());

    return true;
}

void WebServerManager::update()
{
    server.handleClient();
}