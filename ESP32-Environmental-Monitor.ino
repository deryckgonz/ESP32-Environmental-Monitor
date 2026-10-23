
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define RAIN_PIN 34

// Wi-Fi settings
const char* ssid = "Gonzos";
const char* password = "G3057469088";

// Hardware
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_BME280 bme;
BH1750 lightMeter;

// Rain sensor calibration
// Adjust after testing your actual sensor
const int RAIN_DRY = 3500;
const int RAIN_WET = 1000;
const int RAIN_THRESHOLD = 2500;

// Web server
WebServer server(80);

// Store 5 minutes of sensor readings
const int MAX_READINGS = 60;
const unsigned long LOG_INTERVAL = 5000;

float tempHistory[MAX_READINGS];
float humHistory[MAX_READINGS];
float presHistory[MAX_READINGS];
float luxHistory[MAX_READINGS];
float rainHistory[MAX_READINGS];

unsigned long timeHistory[MAX_READINGS];

int readingCount = 0;
int historyIndex = 0;
unsigned long lastLogTime = 0;

// -----------------------------
// RAIN SENSOR
// -----------------------------

int readRainRaw() {
  return analogRead(RAIN_PIN);
}

float rainPercent(int raw) {
  float percent = 100.0 * (RAIN_DRY - raw) /
                  (RAIN_DRY - RAIN_WET);

  return constrain(percent, 0.0, 100.0);
}

bool isRaining(int raw) {
  return raw < RAIN_THRESHOLD;
}

// -----------------------------
// WEB DASHBOARD
// -----------------------------

void handleRoot() {

  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>

<meta name="viewport" content="width=device-width, initial-scale=1">

<title>Environment Monitor</title>

<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

<style>

* {
  box-sizing: border-box;
}

body {
  font-family: Arial, sans-serif;
  background: #0B1220;
  color: #F9FAFB;
  margin: 0;
}

.container {
  max-width: 1000px;
  margin: auto;
  padding: 30px 20px;
}

h1 {
  text-align: center;
  margin-bottom: 5px;
}

.subtitle {
  text-align: center;
  color: #9CA3AF;
  margin-bottom: 12px;
}

.status {
  text-align: center;
  color: #9CA3AF;
  margin-bottom: 30px;
}

.dot {
  height: 9px;
  width: 9px;
  background: #22D3EE;
  border-radius: 50%;
  display: inline-block;
  margin-right: 6px;
}

.cards {
  display: grid;
  grid-template-columns: repeat(5, 1fr);
  gap: 15px;
}

.card {
  background: #111827;
  border: 1px solid #1F2937;
  padding: 18px;
  border-radius: 14px;
  text-align: center;
}

.label {
  color: #9CA3AF;
  font-size: 12px;
  font-weight: bold;
  letter-spacing: 1px;
}

.value {
  color: #22D3EE;
  font-size: 27px;
  font-weight: bold;
  margin-top: 10px;
}

.rainStatus {
  color: #9CA3AF;
  font-size: 13px;
  margin-top: 8px;
}

.chartCard {
  background: #111827;
  border: 1px solid #1F2937;
  padding: 20px;
  margin-top: 20px;
  border-radius: 14px;
}

.chartCard h2 {
  font-size: 18px;
  margin-top: 0;
}

canvas {
  max-height: 300px;
}

.footer {
  text-align: center;
  color: #6B7280;
  font-size: 12px;
  margin-top: 25px;
}

@media(max-width: 900px) {
  .cards {
    grid-template-columns: repeat(2, 1fr);
  }
}

@media(max-width: 450px) {
  .cards {
    grid-template-columns: 1fr;
  }
}

</style>
</head>

<body>
<div class="container">

<h1>Environment Monitor</h1>

<div class="subtitle">
ESP32 Live Sensor Dashboard
</div>

<div class="status">
<span class="dot"></span>
System Online
</div>

<div class="cards">

<div class="card">
  <div class="label">TEMPERATURE</div>
  <div class="value">
    <span id="temp">--</span> &deg;C
  </div>
</div>

<div class="card">
  <div class="label">HUMIDITY</div>
  <div class="value">
    <span id="hum">--</span> %
  </div>
</div>

<div class="card">
  <div class="label">PRESSURE</div>
  <div class="value">
    <span id="pres">--</span> hPa
  </div>
</div>

<div class="card">
  <div class="label">LIGHT</div>
  <div class="value">
    <span id="lux">--</span> lux
  </div>
</div>

<div class="card">
  <div class="label">RAIN SENSOR</div>
  <div class="value">
    <span id="rain">--</span> %
  </div>
  <div class="rainStatus" id="rainStatus">
    Checking...
  </div>
</div>

</div>

<div class="chartCard">
  <h2>Temperature History</h2>
  <canvas id="tempChart"></canvas>
</div>

<div class="chartCard">
  <h2>Humidity History</h2>
  <canvas id="humChart"></canvas>
</div>

<div class="chartCard">
  <h2>Pressure History</h2>
  <canvas id="presChart"></canvas>
</div>

<div class="chartCard">
  <h2>Light History</h2>
  <canvas id="luxChart"></canvas>
</div>

<div class="chartCard">
  <h2>Rain Sensor History</h2>
  <canvas id="rainChart"></canvas>
</div>

<div class="footer">
ESP32 Environmental Monitoring System
</div>

</div>

<script>

Chart.defaults.color = '#9CA3AF';
Chart.defaults.borderColor = '#1F2937';

// Create each graph
function createChart(canvasId, label) {
  return new Chart(
    document.getElementById(canvasId),
    {
      type: 'line',
      data: {
        labels: [],
        datasets: [{
          label: label,
          data: [],
          borderColor: '#22D3EE',
          backgroundColor: 'rgba(34, 211, 238, 0.08)',
          fill: true,
          tension: 0.3
        }]
      }
    }
  );
}

const tempChart = createChart(
  'tempChart', 'Temperature (°C)'
);

const humChart = createChart(
  'humChart', 'Humidity (%)'
);

const presChart = createChart(
  'presChart', 'Pressure (hPa)'
);

const luxChart = createChart(
  'luxChart', 'Light (lux)'
);

const rainChart = createChart(
  'rainChart', 'Estimated Wetness (%)'
);

// -----------------------------
// UPDATE LIVE READINGS
// -----------------------------

function updateCurrentData() {

  fetch('/data')
    .then(response => response.json())
    .then(data => {

      document.getElementById('temp').textContent = data.temp;
      document.getElementById('hum').textContent = data.hum;
      document.getElementById('pres').textContent = data.pres;
      document.getElementById('lux').textContent = data.lux;
      document.getElementById('rain').textContent = data.rain;

      document.getElementById('rainStatus').textContent =
        data.raining ? 'RAIN DETECTED' : 'DRY';

    })
    .catch(error => console.log(error));
}

// -----------------------------
// UPDATE GRAPHS
// -----------------------------

function updateGraphs() {

  fetch('/history')
    .then(response => response.json())
    .then(data => {

      const charts = [
        [tempChart, data.temp],
        [humChart, data.hum],
        [presChart, data.pres],
        [luxChart, data.lux],
        [rainChart, data.rain]
      ];

      charts.forEach(item => {

        item[0].data.labels = data.time;
        item[0].data.datasets[0].data = item[1];
        item[0].update();

      });

    })
    .catch(error => console.log(error));
}

updateCurrentData();
updateGraphs();

setInterval(updateCurrentData, 1000);
setInterval(updateGraphs, 5000);

</script>

</body>
</html>
)rawliteral";

  server.send(200, "text/html", page);
}

// -----------------------------
// CURRENT SENSOR DATA
// -----------------------------

void handleData() {

  float temp = bme.readTemperature();
  float hum = bme.readHumidity();
  float pres = bme.readPressure() / 100.0F;
  float lux = lightMeter.readLightLevel();

  int rainRaw = readRainRaw();
  float rain = rainPercent(rainRaw);
  bool raining = isRaining(rainRaw);

  String json = "{";

  json += "\"temp\":" + String(temp, 1) + ",";
  json += "\"hum\":" + String(hum, 1) + ",";
  json += "\"pres\":" + String(pres, 0) + ",";
  json += "\"lux\":" + String(lux, 0) + ",";
  json += "\"rain\":" + String(rain, 0) + ",";
  json += "\"raining\":";
  json += raining ? "true" : "false";

  json += "}";

  server.send(200, "application/json", json);
}

// -----------------------------
// HISTORY DATA
// -----------------------------

void handleHistory() {

  String json = "{";

  json += "\"time\":[";

  for (int i = 0; i < readingCount; i++) {

    int index =
      (historyIndex - readingCount + i + MAX_READINGS)
      % MAX_READINGS;

    unsigned long seconds =
      timeHistory[index] / 1000;

    json += "\"" + String(seconds) + "s\"";

    if (i < readingCount - 1)
      json += ",";
  }

  json += "],";

  json += "\"temp\":[";

  for (int i = 0; i < readingCount; i++) {

    int index =
      (historyIndex - readingCount + i + MAX_READINGS)
      % MAX_READINGS;

    json += String(tempHistory[index], 1);

    if (i < readingCount - 1)
      json += ",";
  }

  json += "],";

  json += "\"hum\":[";

  for (int i = 0; i < readingCount; i++) {

    int index =
      (historyIndex - readingCount + i + MAX_READINGS)
      % MAX_READINGS;

    json += String(humHistory[index], 1);

    if (i < readingCount - 1)
      json += ",";
  }

  json += "],";

  json += "\"pres\":[";

  for (int i = 0; i < readingCount; i++) {

    int index =
      (historyIndex - readingCount + i + MAX_READINGS)
      % MAX_READINGS;

    json += String(presHistory[index], 0);

    if (i < readingCount - 1)
      json += ",";
  }

  json += "],";

  json += "\"lux\":[";

  for (int i = 0; i < readingCount; i++) {

    int index =
      (historyIndex - readingCount + i + MAX_READINGS)
      % MAX_READINGS;

    json += String(luxHistory[index], 0);

    if (i < readingCount - 1)
      json += ",";
  }

  json += "],";

  json += "\"rain\":[";

  for (int i = 0; i < readingCount; i++) {

    int index =
      (historyIndex - readingCount + i + MAX_READINGS)
      % MAX_READINGS;

    json += String(rainHistory[index], 0);

    if (i < readingCount - 1)
      json += ",";
  }

  json += "]";

  json += "}";

  server.send(200, "application/json", json);
}

// -----------------------------
// SAVE SENSOR READINGS
// -----------------------------

void logReading() {

  tempHistory[historyIndex] =
    bme.readTemperature();

  humHistory[historyIndex] =
    bme.readHumidity();

  presHistory[historyIndex] =
    bme.readPressure() / 100.0F;

  luxHistory[historyIndex] =
    lightMeter.readLightLevel();

  rainHistory[historyIndex] =
    rainPercent(readRainRaw());

  timeHistory[historyIndex] = millis();

  historyIndex++;

  if (historyIndex >= MAX_READINGS)
    historyIndex = 0;

  if (readingCount < MAX_READINGS)
    readingCount++;
}

// -----------------------------
// SETUP
// -----------------------------

void setup() {

  Serial.begin(115200);
  delay(1000);

  Wire.begin(21, 22);

  analogReadResolution(12);
  pinMode(RAIN_PIN, INPUT);

  // OLED initialization
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found");
    while (true);
  }

  // BME280 initialization
  if (!bme.begin(0x76)) {
    Serial.println("BME280 not found");
    while (true);
  }

  // BH1750 initialization
  if (!lightMeter.begin()) {
    Serial.println("BH1750 not found");
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi CONNECTED");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Start web dashboard
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/history", handleHistory);

  server.begin();

  Serial.println("Web server started");

  logReading();
  lastLogTime = millis();
}

// -----------------------------
// MAIN LOOP
// -----------------------------

void loop() {

  server.handleClient();

  // Save readings every 5 seconds
  if (millis() - lastLogTime >= LOG_INTERVAL) {

    logReading();
    lastLogTime = millis();

  }

  // Read sensors
  float temp = bme.readTemperature();
  float hum = bme.readHumidity();
  float pres = bme.readPressure() / 100.0F;
  float lux = lightMeter.readLightLevel();

  int rainRaw = readRainRaw();
  float rain = rainPercent(rainRaw);
  bool raining = isRaining(rainRaw);

  // -----------------------------
  // OLED DISPLAY
  // -----------------------------

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextWrap(false);

  // Title
  display.setCursor(7, 0);
  display.println("ENVIRONMENT MONITOR");

  display.drawLine(
    0, 11, 127, 11, SSD1306_WHITE
  );

  // Temperature
  display.setCursor(3, 15);
  display.print("TEMP");

  display.setCursor(48, 15);
  display.print(temp, 1);
  display.print(" C");

  // Humidity
  display.setCursor(3, 24);
  display.print("HUM");

  display.setCursor(48, 24);
  display.print(hum, 1);
  display.print(" %");

  // Pressure
  display.setCursor(3, 33);
  display.print("PRES");

  display.setCursor(48, 33);
  display.print(pres, 0);
  display.print(" hPa");

  // Light
  display.setCursor(3, 42);
  display.print("LIGHT");

  display.setCursor(48, 42);
  display.print(lux, 0);
  display.print(" lx");

  // Rain
  display.setCursor(3, 53);
  display.print("RAIN");

  display.setCursor(48, 53);

  if (raining) {
    display.print("WET ");
  } else {
    display.print("DRY ");
  }

  display.print(rain, 0);
  display.print("%");

  display.display();

  delay(50);
}
