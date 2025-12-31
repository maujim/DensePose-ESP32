/**
 * @file html_data.h
 * @brief Embedded HTML/JS for the CSI visualization web page
 *
 * This file contains the complete HTML page as a C string literal.
 * It's stored in flash and served by the HTTP server.
 */

#ifndef HTML_DATA_H
#define HTML_DATA_H

/**
 * @brief The HTML page source code
 *
 * Features:
 * - Real-time scrolling CSI amplitude graph using Canvas
 * - Signal strength (RSSI) display
 * - Packet rate counter
 * - System stats display
 * - No external dependencies (vanilla JS)
 */
static const char html_data[] =
R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>DensePose ESP32 - CSI Monitor</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #0a0e1a;
            color: #e0e6ed;
            min-height: 100vh;
            padding: 20px;
        }
        .container { max-width: 900px; margin: 0 auto; }
        h1 {
            font-size: 1.5rem;
            margin-bottom: 5px;
            color: #00ff9d;
        }
        .subtitle { color: #5c6b7f; font-size: 0.85rem; margin-bottom: 20px; }
        .status {
            display: inline-block;
            padding: 4px 12px;
            border-radius: 12px;
            font-size: 0.75rem;
            font-weight: 600;
        }
        .status.connected { background: #00ff9d22; color: #00ff9d; }
        .status.disconnected { background: #ff444422; color: #ff6b6b; }
        .card {
            background: #111625;
            border-radius: 12px;
            padding: 20px;
            margin-bottom: 16px;
            border: 1px solid #1e2738;
        }
        .card h2 {
            font-size: 0.9rem;
            color: #7aa2f7;
            margin-bottom: 12px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        .stats-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
            gap: 12px;
        }
        .stat {
            background: #0a0e1a;
            padding: 12px;
            border-radius: 8px;
            text-align: center;
        }
        .stat-value { font-size: 1.5rem; font-weight: 600; }
        .stat-label { font-size: 0.7rem; color: #5c6b7f; margin-top: 4px; }
        .rssi-strong { color: #00ff9d; }
        .rssi-medium { color: #ffb86c; }
        .rssi-weak { color: #ff6b6b; }
        canvas {
            width: 100%;
            height: 200px;
            background: #0a0e1a;
            border-radius: 8px;
        }
        .footer {
            text-align: center;
            color: #3a4a5a;
            font-size: 0.75rem;
            margin-top: 20px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>DensePose ESP32</h1>
        <p class="subtitle">WiFi CSI Real-Time Monitor</p>
        <span id="status" class="status disconnected">Connecting...</span>

        <div class="card">
            <h2>Signal Strength</h2>
            <div class="stats-grid">
                <div class="stat">
                    <div id="rssi" class="stat-value">--</div>
                    <div class="stat-label">RSSI (dBm)</div>
                </div>
                <div class="stat">
                    <div id="rate" class="stat-value">0</div>
                    <div class="stat-label">Packets/sec</div>
                </div>
                <div class="stat">
                    <div id="total" class="stat-value">0</div>
                    <div class="stat-label">Total Packets</div>
                </div>
            </div>
        </div>

        <div class="card">
            <h2>CSI Amplitude Graph</h2>
            <canvas id="csi-graph"></canvas>
        </div>

        <div class="card">
            <h2>System Stats</h2>
            <div class="stats-grid">
                <div class="stat">
                    <div id="heap" class="stat-value">--</div>
                    <div class="stat-label">Free Heap (KB)</div>
                </div>
                <div class="stat">
                    <div id="uptime" class="stat-value">--</div>
                    <div class="stat-label">Uptime (sec)</div>
                </div>
            </div>
        </div>

        <p class="footer">DensePose-ESP32 &bull; WiFi-based Human Pose Estimation</p>
    </div>

    <script>
        const canvas = document.getElementById('csi-graph');
        const ctx = canvas.getContext('2d');
        const statusEl = document.getElementById('status');

        // Set canvas resolution
        function resizeCanvas() {
            const rect = canvas.getBoundingClientRect();
            canvas.width = rect.width * window.devicePixelRatio;
            canvas.height = rect.height * window.devicePixelRatio;
            ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
        }
        resizeCanvas();
        window.addEventListener('resize', resizeCanvas);

        // CSI data history
        const maxPoints = 200;
        let csiHistory = [];
        let packetCount = 0;
        let lastSecondPackets = 0;
        let packetsPerSec = 0;
        let currentRssi = -100;

        // Track packets per second
        setInterval(() => {
            packetsPerSec = lastSecondPackets;
            lastSecondPackets = 0;
            document.getElementById('rate').textContent = packetsPerSec;
        }, 1000);

        // Fetch system stats periodically
        setInterval(async () => {
            try {
                const resp = await fetch('/stats');
                const stats = await resp.json();
                document.getElementById('heap').textContent = (stats.free_heap / 1024).toFixed(1);
                document.getElementById('uptime').textContent = stats.uptime;
                document.getElementById('total').textContent = stats.packets_received;
            } catch(e) {}
        }, 2000);

        // Connect to SSE stream
        const eventSource = new EventSource('/csi');

        eventSource.onopen = () => {
            statusEl.textContent = 'Live';
            statusEl.className = 'status connected';
        };

        eventSource.onerror = () => {
            statusEl.textContent = 'Disconnected';
            statusEl.className = 'status disconnected';
        };

        eventSource.addEventListener('connected', (e) => {
            console.log('SSE connected');
        });

        eventSource.onmessage = (e) => {
            const data = JSON.parse(e.data);

            // Update RSSI display
            currentRssi = data.rssi;
            const rssiEl = document.getElementById('rssi');
            rssiEl.textContent = data.rssi;
            rssiEl.className = 'stat-value ' + (data.rssi > -50 ? 'rssi-strong' : data.rssi > -70 ? 'rssi-medium' : 'rssi-weak');

            // Count packet
            lastSecondPackets++;
            packetCount++;

            // Add to history (store average amplitude)
            const avgAmp = data.amp && data.amp.length > 0
                ? data.amp.reduce((a, b) => a + b, 0) / data.amp.length
                : 0;
            csiHistory.push(avgAmp);
            if (csiHistory.length > maxPoints) {
                csiHistory.shift();
            }

            draw();
        };

        function draw() {
            const width = canvas.getBoundingClientRect().width;
            const height = canvas.getBoundingClientRect().height;

            // Clear
            ctx.fillStyle = '#0a0e1a';
            ctx.fillRect(0, 0, width, height);

            // Draw grid lines
            ctx.strokeStyle = '#1e2738';
            ctx.lineWidth = 1;
            for (let i = 0; i < 5; i++) {
                const y = (height / 5) * i;
                ctx.beginPath();
                ctx.moveTo(0, y);
                ctx.lineTo(width, y);
                ctx.stroke();
            }

            if (csiHistory.length < 2) return;

            // Draw CSI amplitude line
            ctx.beginPath();
            ctx.strokeStyle = '#00ff9d';
            ctx.lineWidth = 2;

            const step = width / maxPoints;
            const maxAmp = 80;  // Expected max amplitude

            for (let i = 0; i < csiHistory.length; i++) {
                const x = i * step;
                const y = height - (csiHistory[i] / maxAmp) * height * 0.8 - height * 0.1;
                if (i === 0) {
                    ctx.moveTo(x, y);
                } else {
                    ctx.lineTo(x, y);
                }
            }
            ctx.stroke();

            // Draw gradient fill
            const gradient = ctx.createLinearGradient(0, 0, 0, height);
            gradient.addColorStop(0, 'rgba(0, 255, 157, 0.2)');
            gradient.addColorStop(1, 'rgba(0, 255, 157, 0)');
            ctx.lineTo((csiHistory.length - 1) * step, height);
            ctx.lineTo(0, height);
            ctx.closePath();
            ctx.fillStyle = gradient;
            ctx.fill();
        }

        // Initial draw
        draw();
    </script>
</body>
</html>
)";

#endif // HTML_DATA_H
