const API = "http://127.0.0.1:8080/api";

let accessPoints = [];
let tick = 0;
let totalTicks = 50;
let timer = null;
let simulationFinished = false;

const $ = (id) => document.getElementById(id);

async function apiRequest(endpoint, method = "GET") {
    const response = await fetch(`${API}/${endpoint}`, { method });
    const data = await response.json();

    if (!response.ok || data.error) {
        throw new Error(data.error || "Backend request failed");
    }

    return data;
}

function renderDashboard() {
    $("tick").textContent = `${tick} / ${totalTicks}`;
    $("apCount").textContent = accessPoints.length;

    $("userCount").textContent = accessPoints.reduce(
        (sum, ap) => sum + ap.users, 0
    );

    const averageLoad = accessPoints.length
        ? accessPoints.reduce((sum, ap) => sum + ap.load, 0)
            / accessPoints.length
        : 0;

    $("avgLoad").textContent = `${averageLoad.toFixed(1)}%`;

    $("health").textContent = accessPoints.some(
        ap => ap.load >= 80 || ap.users > 30
    ) ? "Congested" : "Healthy";

    $("apGrid").innerHTML = accessPoints.map(ap => `
        <article class="ap-card">
            <div class="ap-title">
                <div>
                    <h3>Access Point ${ap.id}</h3>
                    <p>Wireless network node</p>
                </div>
                <span class="health-pill">${ap.status}</span>
            </div>

            <div class="load-row">
                <span>Network load</span>
                <strong>${ap.load.toFixed(1)}%</strong>
            </div>

            <div class="progress">
                <div class="progress-fill"
                     style="width:${Math.max(0, Math.min(ap.load, 100))}%">
                </div>
            </div>

            <div class="ap-details">
                <div>
                    <span>Connected users</span>
                    <strong>${ap.users}</strong>
                </div>
                <div>
                    <span>Capacity</span>
                    <strong>${ap.capacity} Mbps</strong>
                </div>
                <div>
                    <span>Channel utilization</span>
                    <strong>${ap.channel.toFixed(1)}%</strong>
                </div>
                <div>
                    <span>Latency</span>
                    <strong>${ap.latency.toFixed(1)} ms</strong>
                </div>
            </div>
        </article>
    `).join("");
}

function updateFromBackend(data) {
    tick = data.tick;
    totalTicks = data.totalTicks;
    simulationFinished = data.finished;

    
accessPoints = (data.accessPoints || []).map(ap => ({
    id: ap.id,
    users: ap.userCount,
    load: ap.loadPercent,
    channel: ap.channelUtilization,
    latency: ap.latencyMs,
    capacity: ap.capacityMbps,
    status: ap.status
}));

    renderDashboard();
}

async function refreshStatus() {
    const data = await apiRequest("status");
    updateFromBackend(data);
}

async function simulationStep() {
    try {
        const data = await apiRequest("step", "POST");
        updateFromBackend(data);

        $("statusMessage").textContent =
            `C++ simulation running: tick ${tick} of ${totalTicks}.`;
        $("statusMessage").style.color = "#34d399";

        if (simulationFinished || tick >= totalTicks) {
            stopSimulation();
            $("statusMessage").textContent =
                `C++ simulation completed at tick ${tick}.`;
        }
    } catch (error) {
        stopSimulation();
        showError(error);
    }
}


async function startSimulation() {
    if (timer !== null) return;

    try {
        if (simulationFinished || tick >= totalTicks) {
            $("statusMessage").textContent =
                "Simulation finished. Press Reset before starting again.";
            return;
        }

        if (tick === 0) {
            const data = await apiRequest("start", "POST");
            updateFromBackend(data);
        }

        $("statusMessage").textContent =
            "C++ simulation running...";
        $("statusMessage").style.color = "#34d399";

        timer = setInterval(simulationStep, 700);
    } catch (error) {
        showError(error);
    }
}


function stopSimulation() {
    if (timer !== null) {
        clearInterval(timer);
        timer = null;
    }
}


async function resetSimulation() {
    stopSimulation();

    try {
        const data = await apiRequest("configure", "POST", {
            apCount: Number(document.getElementById("configAPs").value),
            userCount: Number(document.getElementById("configUsers").value),
            capacityMbps: Number(document.getElementById("configCapacity").value)
        });

        updateFromBackend(data);

        $("statusMessage").textContent =
            "Simulation reset. Configuration restored successfully.";
        $("statusMessage").style.color = "#34d399";

    } catch (error) {
        showError(error);
    }
}


function showError(error) {
    $("statusMessage").textContent =
        `Backend error: ${error.message}. Check that WiBalanceServer.exe is running.`;
    $("statusMessage").style.color = "#f87171";
    console.error(error);
}

$("startBtn").addEventListener("click", startSimulation);
$("pauseBtn").addEventListener("click", stopSimulation);
$("resetBtn").addEventListener("click", resetSimulation);

async function initializeDashboard() {
    try {
        await refreshStatus();

        $("statusMessage").textContent =
            "Connected to the C++ WiBalance simulation.";
        $("statusMessage").style.color = "#34d399";
    } catch (error) {
        showError(error);
    }
}

initializeDashboard();

const configForm = document.getElementById("configForm");

configForm.addEventListener("submit", async function (event) {
    event.preventDefault();

    const apCount = Number(
        document.getElementById("configAPs").value
    );

    const userCount = Number(
        document.getElementById("configUsers").value
    );

    const capacityMbps = Number(
        document.getElementById("configCapacity").value
    );

    const message = document.getElementById("configMessage");

    try {
        message.textContent = "Applying configuration...";

        const response = await fetch(
            "http://127.0.0.1:8080/api/configure",
            {
                method: "POST",
                headers: {
                    "Content-Type": "application/json"
                },
                body: JSON.stringify({
                    apCount,
                    userCount,
                    capacityMbps
                })
            }
        );

        const data = await response.json();

        if (!response.ok || data.error) {
            throw new Error(data.error || "Configuration failed");
        }

        updateFromBackend(data);

        stopSimulation();

        message.textContent =
            `Configuration applied: ${data.accessPointCount} APs, ` +
            `${data.userCount} users.`;

        document.getElementById("statusMessage").textContent =
            "Configuration updated. Ready to simulate.";

    } catch (error) {
        message.textContent =
            "Configuration failed: " + error.message;

        console.error(error);
    }
});
