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

// Wi-Fi
const char* ssid = "wifiname";
const char* password = "wifipassword";

// Hardware
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_BME280 bme;
BH1750 lightMeter;

// Web server
WebServer server(80);

// 60 readings x 5 seconds = 5 minutes
const int MAX_READINGS = 60;

float tempHistory[MAX_READINGS];
float humHistory[MAX_READINGS];
float presHistory[MAX_READINGS];
float luxHistory[MAX_READINGS];

unsigned long timeHistory[MAX_READINGS];

int readingCount = 0;
int historyIndex = 0;

unsigned long lastLogTime = 0;
const unsigned long LOG_INTERVAL = 5000;


// -----------------------------
// WEBPAGE
// -----------------------------

void handleRoot() {

  String page = R"rawliteral(
<!DOCTYPE html>
<html>

<head>

<meta name="viewport" content="width=device-width, initial-scale=1">

<title>Environmental Monitor</title>

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
  grid-template-columns: repeat(4, 1fr);
  gap: 15px;
}

.card {
  background: #111827;
  border: 1px solid #1F2937;
  padding: 22px;
  border-radius: 14px;
  text-align: center;
}

.label {
  color: #9CA3AF;
  font-size: 13px;
  font-weight: bold;
  letter-spacing: 1px;
}

.value {
  color: #22D3EE;
  font-size: 30px;
  font-weight: bold;
  margin-top: 10px;
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

@media(max-width: 700px) {
  .cards {
    grid-template-columns: repeat(2, 1fr);
  }
}

</style>

</head>

<body>

<div class="container">

<h1>Environmental Monitor</h1>

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

<div class="footer">
ESP32 Environmental Monitoring System
</div>

</div>


<script>

Chart.defaults.color = '#9CA3AF';
Chart.defaults.borderColor = '#1F2937';

const tempChart = new Chart(
  document.getElementById('tempChart'),
  {
    type: 'line',
    data: {
      labels: [],
      datasets: [{
        label: 'Temperature (°C)',
        data: [],
        borderColor: '#22D3EE',
        backgroundColor: 'rgba(34, 211, 238, 0.08)',
        fill: true,
        tension: 0.3
      }]
    }
  }
);


const humChart = new Chart(
  document.getElementById('humChart'),
  {
    type: 'line',
    data: {
      labels: [],
      datasets: [{
        label: 'Humidity (%)',
        data: [],
        borderColor: '#22D3EE',
        backgroundColor: 'rgba(34, 211, 238, 0.08)',
        fill: true,
        tension: 0.3
      }]
    }
  }
);


const presChart = new Chart(
  document.getElementById('presChart'),
  {
    type: 'line',
    data: {
      labels: [],
      datasets: [{
        label: 'Pressure (hPa)',
        data: [],
        borderColor: '#22D3EE',
        backgroundColor: 'rgba(34, 211, 238, 0.08)',
        fill: true,
        tension: 0.3
      }]
    }
  }
);


const luxChart = new Chart(
  document.getElementById('luxChart'),
  {
    type: 'line',
    data: {
      labels: [],
      datasets: [{
        label: 'Light (lux)',
        data: [],
        borderColor: '#22D3EE',
        backgroundColor: 'rgba(34, 211, 238, 0.08)',
        fill: true,
        tension: 0.3
      }]
    }
  }
);


function updateCurrentData() {

  fetch('/data')
    .then(response => response.json())
    .then(data => {

      document.getElementById('temp').textContent = data.temp;
      document.getElementById('hum').textContent = data.hum;
      document.getElementById('pres').textContent = data.pres;
      document.getElementById('lux').textContent = data.lux;

    });

}


function updateGraphs() {

  fetch('/history')
    .then(response => response.json())
    .then(data => {

      tempChart.data.labels = data.time;
      tempChart.data.datasets[0].data = data.temp;

      humChart.data.labels = data.time;
      humChart.data.datasets[0].data = data.hum;

      presChart.data.labels = data.time;
      presChart.data.datasets[0].data = data.pres;

      luxChart.data.labels = data.time;
      luxChart.data.datasets[0].data = data.lux;

      tempChart.update();
      humChart.update();
      presChart.update();
      luxChart.update();

    });

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

  String json = "{";

  json += "\"temp\":" + String(temp, 1) + ",";
  json += "\"hum\":" + String(hum, 1) + ",";
  json += "\"pres\":" + String(pres, 0) + ",";
  json += "\"lux\":" + String(lux, 0);

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

  json += "]";

  json += "}";

  server.send(200, "application/json", json);
}


// -----------------------------
// SAVE READING
// -----------------------------

void logReading() {

  tempHistory[historyIndex] = bme.readTemperature();
  humHistory[historyIndex] = bme.readHumidity();
  presHistory[historyIndex] = bme.readPressure() / 100.0F;
  luxHistory[historyIndex] = lightMeter.readLightLevel();

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


  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found");
    while (true);
  }


  if (!bme.begin(0x76)) {
    Serial.println("BME280 not found");
    while (true);
  }


  if (!lightMeter.begin()) {
    Serial.println("BH1750 not found");
    while (true);
  }


  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);


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


  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/history", handleHistory);

  server.begin();

  Serial.println("Web server started");


  logReading();

  lastLogTime = millis();
}


// -----------------------------
// LOOP
// -----------------------------

void loop() {

  server.handleClient();


  if (millis() - lastLogTime >= LOG_INTERVAL) {

    logReading();
    lastLogTime = millis();
  }


  float temp = bme.readTemperature();
  float hum = bme.readHumidity();
  float pres = bme.readPressure() / 100.0F;
  float lux = lightMeter.readLightLevel();


  display.clearDisplay();
  display.setTextSize(1);


  display.setCursor(25, 0);
  display.println("ENV MONITOR");

  display.drawLine(0, 11, 127, 11, SSD1306_WHITE);


  display.setCursor(3, 17);
  display.print("TEMP");

  display.setCursor(48, 17);
  display.print(temp, 1);
  display.print(" C");


  display.setCursor(3, 29);
  display.print("HUM");

  display.setCursor(48, 29);
  display.print(hum, 1);
  display.print(" %");


  display.setCursor(3, 41);
  display.print("PRES");

  display.setCursor(48, 41);
  display.print(pres, 0);
  display.print(" hPa");


  display.setCursor(3, 53);
  display.print("LIGHT");

  display.setCursor(48, 53);
  display.print(lux, 0);
  display.print(" lx");


  display.display();

  delay(50);
}