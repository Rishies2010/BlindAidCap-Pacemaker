const state = {
  settings: { theme: "system", accent: "violet" },
  bpmHistory: [],
  maxHistory: 120,
  lastData: null,
  currentPage: "dashboard",
  oledInterval: null,
};

const accents = {
  emerald: "#10b981",

  blue: "#3b82f6",

  violet: "#7c3aed",

  orchid: "#da70d6",

  orange: "#f97316",

  rose: "#f43f5e",
};

const $ = (selector) => document.querySelector(selector);

const $$ = (selector) => document.querySelectorAll(selector);

const bpmValue = $("#bpmValue");

const statusValue = $("#statusValue");

const signalQuality = $("#signalQuality");

const signalProgress = $("#signalProgress");

const pacingValue = $("#pacingValue");

const targetRate = $("#targetRate");

const effectiveBPM = $("#effectiveBPM");

const normalRange = $("#normalRange");

const targetBPM = $("#targetBPM");

const atriumStatus = $("#atriumStatus");

const ventricleStatus = $("#ventricleStatus");

const connectionDot = $("#connectionDot");

const connectionText = $("#connectionText");

const lastUpdate = $("#lastUpdate");

const monitorBPM = $("#monitorBPM");

const monitorStatus = $("#monitorStatus");

const monitorDescription = $("#monitorDescription");

const rawSensorValue = $("#rawSensorValue");

const monitorSignal = $("#monitorSignal");

const repositionValue = $("#repositionValue");

function formatUptime(ms) {
  const totalSeconds = Math.floor(ms / 1000);

  const hours = Math.floor(totalSeconds / 3600);

  const minutes = Math.floor((totalSeconds % 3600) / 60);

  const seconds = totalSeconds % 60;

  if (hours > 0) {
    return `${hours}h ${minutes}m`;
  }

  return `${minutes}m ${seconds}s`;
}

function getStatusDescription(status) {
  const descriptions = {
    NORMAL: "Reading is within configured range",

    LOW: "Rate below configured threshold",

    HIGH: "Rate above configured threshold",

    REPOSITION: "Sensor signal requires repositioning",
  };

  return descriptions[status] || "Waiting for data";
}

function getStatusColor(status) {
  switch (status) {
    case "NORMAL":
      return "#10b981";

    case "LOW":
      return "#3b82f6";

    case "HIGH":
      return "#f59e0b";

    case "REPOSITION":
      return "#a855f7";

    default:
      return accents[state.settings.accent];
  }
}

async function api(url, options = {}) {
  const response = await fetch(url, options);

  if (!response.ok) {
    throw new Error(`Request failed: ${response.status}`);
  }

  return response.json();
}

async function fetchLiveData() {
  try {
    const data = await api("/api/data");

    state.lastData = data;

    updateDashboard(data);

    setConnection(true);
  } catch (error) {
    console.error(error);

    setConnection(false);
  }
}

function setConnection(online) {
  if (online) {
    connectionDot.classList.add("online");

    connectionText.textContent = "Connected";
  } else {
    connectionDot.classList.remove("online");

    connectionText.textContent = "Connection lost";
  }
}

function updateDashboard(data) {
  const statusColor = getStatusColor(data.status);

  bpmValue.textContent = data.bpm;

  monitorBPM.textContent = data.bpm;

  statusValue.textContent = data.status;

  statusValue.style.color = statusColor;

  monitorStatus.textContent = data.status;

  monitorStatus.style.color = statusColor;

  const description = getStatusDescription(data.status);

  $("#statusDescription").textContent = description;

  monitorDescription.textContent = description;

  const quality = Math.round(data.signalQuality);

  signalQuality.textContent = quality;

  signalProgress.style.width = `${quality}%`;

  monitorSignal.textContent = `${quality}%`;

  pacingValue.textContent = data.pacing ? "Active" : "Inactive";

  pacingValue.style.color = data.pacing ? "#f59e0b" : "";

  targetRate.textContent = `Target: ${data.targetBPM} BPM`;

  normalRange.textContent = `${data.normalLow} — ${data.normalHigh} BPM`;

  targetBPM.textContent = `${data.targetBPM} BPM`;

  effectiveBPM.textContent = `${data.effectiveBPM} BPM`;

  updateHardwareStatus(atriumStatus, data.atriumActive);

  updateHardwareStatus(ventricleStatus, data.ventricleActive);

  rawSensorValue.textContent = data.rawSensor;

  repositionValue.textContent = data.reposition ? "Reposition" : "Stable";

  addBPMHistory(data.bpm);

  lastUpdate.textContent = "Updated just now";
}

function updateHardwareStatus(element, active) {
  if (active) {
    element.textContent = "Active";

    element.classList.add("active");
  } else {
    element.textContent = "Idle";

    element.classList.remove("active");
  }
}

function addBPMHistory(value) {
  state.bpmHistory.push(value);

  if (state.bpmHistory.length > state.maxHistory) {
    state.bpmHistory.shift();
  }
}

const canvas = $("#ecgCanvas");

const ctx = canvas.getContext("2d");

function resizeCanvas() {
  const rect = canvas.getBoundingClientRect();

  const dpr = window.devicePixelRatio || 1;

  canvas.width = rect.width * dpr;

  canvas.height = rect.height * dpr;

  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
}

function drawChart() {
  const width = canvas.clientWidth;

  const height = canvas.clientHeight;

  ctx.clearRect(0, 0, width, height);

  ctx.lineWidth = 1;

  ctx.strokeStyle = getComputedStyle(document.documentElement).getPropertyValue(
    "--border",
  );

  for (let x = 0; x < width; x += width / 6) {
    ctx.beginPath();

    ctx.moveTo(x, 0);

    ctx.lineTo(x, height);

    ctx.stroke();
  }

  for (let y = 0; y < height; y += height / 4) {
    ctx.beginPath();

    ctx.moveTo(0, y);

    ctx.lineTo(width, y);

    ctx.stroke();
  }

  if (state.bpmHistory.length < 2) {
    requestAnimationFrame(drawChart);

    return;
  }

  const values = state.bpmHistory;

  const min = Math.min(...values) - 5;

  const max = Math.max(...values) + 5;

  const range = Math.max(max - min, 1);

  ctx.beginPath();

  values.forEach((value, index) => {
    const x = (index / (values.length - 1)) * width;

    const normalized = (value - min) / range;

    const y = height - normalized * height;

    if (index === 0) {
      ctx.moveTo(x, y);
    } else {
      ctx.lineTo(x, y);
    }
  });

  ctx.strokeStyle = accents[state.settings.accent];

  ctx.lineWidth = 2;

  ctx.stroke();

  requestAnimationFrame(drawChart);
}

const oledCanvas = $("#oledCanvas");
const oledCtx = oledCanvas.getContext("2d");

const OLED_W = 128;
const OLED_H = 64;

const OLED_PIXEL_COLOR = "rgb(125, 210, 255)";

function renderOLED(buffer) {

  const needed = OLED_W * (OLED_H / 8);

  if (buffer.length < needed) {
    return;
  }

  oledCtx.fillStyle = "#000";
  oledCtx.fillRect(0, 0, OLED_W, OLED_H);

  oledCtx.fillStyle = OLED_PIXEL_COLOR;

  for (let x = 0; x < OLED_W; x++) {
    for (let p = 0; p < OLED_H / 8; p++) {

      const byte = buffer[x + p * OLED_W];

      if (!byte) continue;

      for (let b = 0; b < 8; b++) {
        if (byte & (1 << b)) {
          oledCtx.fillRect(x, p * 8 + b, 1, 1);
        }
      }

    }
  }

}

async function fetchOLED() {

  try {

    const response =
      await fetch("/api/oled", { cache: "no-store" });

    if (!response.ok) {
      throw new Error(`OLED fetch failed: ${response.status}`);
    }

    const buffer =
      new Uint8Array(await response.arrayBuffer());

    renderOLED(buffer);

  }

  catch (error) {
    console.error(error);
  }

}

async function fetchSystem() {
  try {
    const data = await api("/api/system");

    const systemGrid = $("#systemGrid");

    const items = [
      ["Chip Model", data.chipModel],

      ["CPU Cores", data.cores],

      ["CPU Frequency", `${data.cpuMHz} MHz`],

      ["Chip Revision", data.chipRevision],

      ["Free Heap", formatBytes(data.freeHeap)],

      ["Minimum Heap", formatBytes(data.minFreeHeap)],

      ["Flash Size", formatBytes(data.flashSize)],

      ["Flash Speed", `${Math.round(data.flashSpeed / 1000000)} MHz`],

      [
        "Filesystem",
        `${formatBytes(data.filesystemUsed)} / ${formatBytes(
          data.filesystemTotal,
        )}`,
      ],

      ["SDK", data.sdkVersion],

      ["WiFi Mode", data.wifiMode],

      ["Access Point", data.ssid],

      ["IP Address", data.ip],

      ["Connected Clients", data.connectedClients],

      ["MAC Address", data.mac],

      ["System Uptime", formatUptime(data.uptime)],
    ];

    systemGrid.innerHTML = items
      .map(
        ([label, value]) =>
          `
          <div class="system-card">

            <span>
              ${label}
            </span>

            <strong>
              ${value}
            </strong>

          </div>
          `,
      )
      .join("");
  } catch (error) {
    console.error(error);
  }
}

function formatBytes(bytes) {
  if (!bytes) {
    return "0 B";
  }

  const units = ["B", "KB", "MB", "GB"];

  const index = Math.floor(Math.log(bytes) / Math.log(1024));

  const value = bytes / Math.pow(1024, index);

  return `${value.toFixed(1)} ${units[index]}`;
}

async function loadSettings() {
  try {
    const data = await api("/api/settings");

    state.settings.theme = data.theme;

    state.settings.accent = data.accent;

    $("#themeSelect").value = data.theme;

    $("#volumeSlider").value = data.volume;

    $("#volumeValue").textContent = `${data.volume}%`;

    $("#targetInput").value = data.targetBPM;

    $("#normalLowInput").value = data.normalLow;

    $("#normalHighInput").value = data.normalHigh;

    applyTheme(data.theme);

    applyAccent(data.accent);
  } catch (error) {
    console.error(error);
  }
}

async function saveSettings() {
  const settings = {
    theme: $("#themeSelect").value,

    accent: state.settings.accent,

    volume: Number($("#volumeSlider").value),

    targetBPM: Number($("#targetInput").value),

    normalLow: Number($("#normalLowInput").value),

    normalHigh: Number($("#normalHighInput").value),
  };

  try {
    await api(
      "/api/settings",

      {
        method: "POST",

        headers: {
          "Content-Type": "application/json",
        },

        body: JSON.stringify(settings),
      },
    );

    closeSettings();

    await fetchLiveData();
  } catch (error) {
    console.error(error);

    alert("Unable to save settings");
  }
}

function applyTheme(theme) {
  state.settings.theme = theme;

  const root = document.documentElement;

  if (theme === "dark") {
    root.classList.add("dark");
  } else if (theme === "light") {
    root.classList.remove("dark");
  } else {
    const dark = window.matchMedia("(prefers-color-scheme: dark)").matches;

    root.classList.toggle("dark", dark);
  }
}

function applyAccent(accent) {
  state.settings.accent = accent;

  const color = accents[accent] || accents.violet;

  document.documentElement.style.setProperty("--accent", color);

  $$(".accent").forEach((button) => {
    button.classList.toggle(
      "selected",

      button.dataset.accent === accent,
    );
  });
}

$$(".accent").forEach((button) => {
  button.addEventListener("click", () => {
    applyAccent(button.dataset.accent);
  });
});

$("#themeSelect").addEventListener(
  "change",

  (event) => {
    applyTheme(event.target.value);
  },
);

$("#themeButton").addEventListener(
  "click",

  () => {
    const root = document.documentElement;

    const isDark = root.classList.contains("dark");

    $("#themeSelect").value = isDark ? "light" : "dark";

    applyTheme($("#themeSelect").value);
  },
);

$("#volumeSlider").addEventListener(
  "input",

  (event) => {
    $("#volumeValue").textContent = `${event.target.value}%`;
  },
);

const settingsPanel = $("#settingsPanel");

const settingsOverlay = $("#settingsOverlay");

function openSettings() {
  settingsPanel.classList.add("open");

  settingsOverlay.classList.add("visible");
}

function closeSettings() {
  settingsPanel.classList.remove("open");

  settingsOverlay.classList.remove("visible");
}

$("#settingsButton").addEventListener("click", openSettings);

$("#openSettings").addEventListener("click", openSettings);

$("#closeSettings").addEventListener("click", closeSettings);

settingsOverlay.addEventListener("click", closeSettings);

$("#saveSettings").addEventListener("click", saveSettings);

$("#restartButton").addEventListener(
  "click",

  async () => {
    const confirmed = confirm("Restart the ESP32?");

    if (!confirmed) {
      return;
    }

    try {
      await api(
        "/api/restart",

        {
          method: "POST",
        },
      );
    } catch (_) {}

    setConnection(false);
  },
);

const pageMetadata = {
  dashboard: {
    title: "Dashboard",

    subtitle: "Live device overview",
  },

  monitor: {
    title: "Live Monitor",

    subtitle: "Real-time sensor monitoring",
  },

  oled: {
    title: "OLED Display",
    subtitle: "Live mirror of the onboard screen",
  },

  hardware: {
    title: "Hardware",

    subtitle: "Connected components",
  },

  system: {
    title: "System Information",

    subtitle: "ESP32 runtime information",
  },

  logs: {
    title: "Event Logs",

    subtitle: "Recent device activity",
  },
};

function openPage(pageName) {
  state.currentPage = pageName;

  $$(".page").forEach((page) => {
    page.classList.remove("active");
  });

  const page = document.getElementById(pageName);

  if (page) {
    page.classList.add("active");
  }

  $$(".nav-item").forEach((item) => {
    item.classList.toggle(
      "active",

      item.dataset.page === pageName,
    );
  });

  const metadata = pageMetadata[pageName];

  if (metadata) {
    $("#pageTitle").textContent = metadata.title;

    $("#pageSubtitle").textContent = metadata.subtitle;
  }

  if (pageName === "system") {
    fetchSystem();
  }

  if (pageName === "logs") {
    fetchLogs();
  }

  if (pageName === "oled") {
    fetchOLED();
    if (!state.oledInterval) {
      state.oledInterval = setInterval(fetchOLED, 250);
    }
  } else {
    if (state.oledInterval) {
      clearInterval(state.oledInterval);
      state.oledInterval = null;
    }
  }

  $("#sidebar").classList.remove("mobile-open");

  window.scrollTo(0, 0);
}

$$(".nav-item[data-page]").forEach((item) => {
  item.addEventListener(
    "click",

    () => {
      openPage(item.dataset.page);
    },
  );
});

$("#mobileMenu").addEventListener(
  "click",

  () => {
    $("#sidebar").classList.toggle("mobile-open");
  },
);

async function fetchLogs() {
  try {
    const data = await api("/api/logs");

    const logs = data.logs || [];

    const container = $("#logsList");

    if (logs.length === 0) {
      container.innerHTML = `
        <p>
          No events recorded yet.
        </p>
        `;

      return;
    }

    container.innerHTML = logs
      .slice()
      .reverse()
      .map((log) => {
        return `

            <div class="log-entry">

              <strong>
                ${log.event}
              </strong>

              <time>
                ${formatUptime(log.time)}
              </time>

            </div>

            `;
      })
      .join("");
  } catch (error) {
    console.error(error);
  }
}

const searchItems = [
  {
    title: "Dashboard",

    description: "Live device overview",

    page: "dashboard",
  },

  {
    title: "OLED Display",

    description: "Pixel-accurate mirror of the onboard SSD1306",
    
    page: "oled",
  },

  {
    title: "Live Monitor",

    description: "Sensor and pacing information",

    page: "monitor",
  },

  {
    title: "Hardware",

    description: "ESP32 components and GPIO assignments",

    page: "hardware",
  },

  {
    title: "System Information",

    description: "ESP32 hardware and runtime details",

    page: "system",
  },

  {
    title: "Event Logs",

    description: "Recent system events",

    page: "logs",
  },

  {
    title: "Appearance Settings",

    description: "Theme and accent color",

    action: "settings",
  },

  {
    title: "Audio Settings",

    description: "Speaker volume configuration",

    action: "settings",
  },

  {
    title: "Monitoring Settings",

    description: "Target BPM and normal range",

    action: "settings",
  },
];

const searchInput = $("#searchInput");

const searchResults = $("#searchResults");

searchInput.addEventListener(
  "input",

  (event) => {
    const query = event.target.value.toLowerCase().trim();

    if (!query) {
      searchResults.classList.remove("visible");

      return;
    }

    const matches = searchItems.filter(
      (item) =>
        item.title.toLowerCase().includes(query) ||
        item.description.toLowerCase().includes(query),
    );

    searchResults.innerHTML = matches
      .map(
        (item) =>
          `

            <button
              class="search-result"
              data-page="${item.page || ""}"
              data-action="${item.action || ""}"
            >

              <strong>
                ${item.title}
              </strong>

              <small>
                ${item.description}
              </small>

            </button>

            `,
      )
      .join("");

    searchResults.classList.add("visible");
  },
);

searchResults.addEventListener(
  "click",

  (event) => {
    const button = event.target.closest(".search-result");

    if (!button) {
      return;
    }

    const page = button.dataset.page;

    const action = button.dataset.action;

    if (page) {
      openPage(page);
    }

    if (action === "settings") {
      openSettings();
    }

    searchInput.value = "";

    searchResults.classList.remove("visible");
  },
);

document.addEventListener(
  "keydown",

  (event) => {
    if (event.key === "/" && document.activeElement !== searchInput) {
      event.preventDefault();

      searchInput.focus();
    }

    if (event.key === "Escape") {
      searchResults.classList.remove("visible");

      closeSettings();
    }
  },
);

document.addEventListener(
  "click",

  (event) => {
    if (
      !searchInput.contains(event.target) &&
      !searchResults.contains(event.target)
    ) {
      searchResults.classList.remove("visible");
    }
  },
);

window.addEventListener(
  "resize",

  resizeCanvas,
);

async function initialize() {
  resizeCanvas();

  drawChart();

  await loadSettings();

  await fetchLiveData();

  await fetchSystem();

  setInterval(fetchLiveData, 500);

  setInterval(fetchSystem, 10000);

  setInterval(
    () => {
      if (state.currentPage === "logs") {
        fetchLogs();
      }
    },

    5000,
  );
}

initialize();
