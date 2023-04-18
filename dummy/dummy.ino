// Include Libraries
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

// Define Ports
#define ledPin 26         // LED (Aktuator) dipasang pada GPIO 26
#define sensorPin 34      // Sensor dipasang pada GPIO 34

// Initialize Variables
// currstate: status aktuator
int currstate = LOW;
// hasil: hasil pembacaan sensor
float hasil;
// statusHasil: status kelembaban
int statusHasil;

// Network Credentials
const char* ssid = "ubnt";
const char* password = "@uber104";

// AsyncWebServer object on port 80
AsyncWebServer server(80);

// Event Source on /events
AsyncEventSource events("/events");

// Timer variables
unsigned long lastTime = 0;  
unsigned long timerDelay = 3000;

// Sensor object

// Pembacaan sensor sebelum dibalik
// 4095 paling kering
// 1095 paling basah (kira-kira 1100 seharusnya)
// range basah 3000

// Dibalik untuk mempermudah pembacaan
// 0 paling kering (4095)
// 3000 paling basah (1095)

void getSensorReadings(){
  hasil = 3000 - (analogRead(sensorPin) - 1095);
  
  // Pembulatan hasil di bawah 0 menjadi 0
  if (hasil < 0)
  {
    hasil = 0;
  }
  // Pembulatan hasil di atas 3000 menjadi 3000
  if (hasil > 3000)
  {
    hasil = 3000;
  }
  
  // Set status kelembaban
  // Tanaman rata-rata harus berada di kelembaban 45% sampai 65%
  // 45% dari 3000: 1350
  // 65% dari 3000: 1950
  if (hasil < 1350){        // terlalu kering
    statusHasil = 1;
  }
  else if (hasil > 1950){   // terlalu lembab
    statusHasil = 3;
  }
  else{                     // kelembaban pas
    statusHasil = 2;
  }
  
  // Ketika LED sedang menyala,  berarti sebelumnya moisture
  // turun di bawah rata-rata
  // Aktuator dalam keadaan menyala
  if (currstate == HIGH)
  {
    // Jika hasil pembacaan diatas 1650 (70%), berarti moisture 
    // sudah berada pada range yang tepat
    if (hasil > 1650)       // pertengahan diantara batas bawah
                            // dan batas atas kelembaban
    {
      // Oleh karena itu, LED (Aktuator) dimatikan
      digitalWrite(ledPin, LOW);
      currstate = LOW;
    }
  }
  // Ketika LED sedang tidak menyala, berarti sebelumnya
  // moisture dalam range normal
  // Aktuator dalam keadaan mati
  else
  {
    // Jika hasil pembacaan dibawah 1350 (45%), berarti 
    // keadaan terlalu kering
    if (hasil < 1350)
    {
      // Oleh karena itu, LED (Aktuator) dinyalakan
      digitalWrite(ledPin, HIGH);
      currstate = HIGH;
    }
  }
}

// Function yang me-return hasil berupa String
String readMoisture(){
  // Menerima hasil pembacaan sensor
  float m = hasil;

  if (isnan(m)){
    Serial.println("Failed to read moisture!");
    return "";
  }
  else {
    Serial.println(m);
    return String(m);
  }
}

// Initialize WiFi
// Melakukan koneksi ke router Wi-Fi menggunakan ssid
// dan password yang telah didefinisi
void initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi ..");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        delay(1000);
    }
    Serial.println(WiFi.localIP());
}

// Initialize Processor Function
// Function processor digunakan untuk menampilkan
// pembacaan sensor ketika dilakukan pembacaan pertama kali
// untuk mencegah menampilkan tampilan yang kosong 
String processor(const String& var){
  getSensorReadings();

  // Untuk tampilan kelembaban
  if(var == "MOISTURE"){
    return String(hasil);
  }

  // Untuk tampilan status kelembaban
  if (var == "STATUS"){
    if (statusHasil == 1){
      return "Terlalu Kering";
    }
    else if (statusHasil == 2){
      return "Lembab";
    }
    else{
      return "Terlalu Lembab";
    }
  }
  return String();
}

// Web Page Building
// Membuat web page untuk menampilkan hasil pembacaan

// CSS
// Digunakan untuk men-design web page

// HTML
// Digunakan untuk menampilkan konten

// JavaScript
// Digunakan untuk menginisialisasi koneksi EventSource
// dengan server dan mengatasi event yang diterima server
  
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p { font-size: 1.2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #50B8B4; color: white; font-size: 1rem; }
    .content { padding: 20px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 800px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); }
    .reading { font-size: 1.4rem; }
  </style>
  
</head>
<body>
  <div class="topnav">
    <h1>PENYIRAMAN TANAMAN OTOMATIS</h1>
  </div>
  <div class="content">
    <div class="cards">
      <div class="card">
        <p><i class="fas fa-tint" style="color:#00add6;"></i> MOISTURE </p><p><span class="reading"><span id="moi">%MOISTURE%</span></span></p>
      </div>
    </div>
  </div>
  <div class="content">
    <div class="cards">
      <div class="card">
        <p></i> STATUS </p><p><span class="reading"><span id="sta">%STATUS%</span></span></p>
      </div>
    </div>
  </div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);
 
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 
 source.addEventListener('moisture', function(e) {
  console.log("moisture", e.data);
  document.getElementById("moi").innerHTML = e.data;
 }, false);

  source.addEventListener('status', function(e) {
  console.log("status", e.data);
  document.getElementById("sta").innerHTML = e.data;
 }, false);
 
}
</script>
</body>
</html>)rawliteral";

// Setup
void setup() {
  // Inisialisasi Serial Monitor 115200
  Serial.begin(115200);
  // Inisialisasi LED (Aktuator) sebagai OUTPUT
  pinMode(ledPin, OUTPUT);
  // Inisialisasi LED (Aktuator) keadaan mati (LOW)
  digitalWrite(ledPin, LOW);
  
  // Inisialisasi SPIFFS (Filesystem)
  // Digunakan untuk meng-upload file ke Flash Memory
  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Inisialisasi Wi-Fi
  // Digunakan untuk menghubungkan ESP32 dengan router
  initWiFi();

  // Handle Web Server
  // Pada root "/" (Main Page) akan ditampilkan text yang telah
  // disimpan pada "index_html" (Web Page yang telah dibuat
  // pada Web Page Building)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  // Pada root "/chart" akan ditampilkan chart yang telah dibuat
  // pada file "index.html" yang diupload menggunakan SPIFFS
  server.on("/chart", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html");
  });
  // Pada root "/moisture" akan ditampilkan pembacaan sensor
  server.on("/moisture", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", readMoisture().c_str());
  });

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);

  // Server Start
  server.begin();
}

// Loop
void loop() {
  if ((millis() - lastTime) > timerDelay) {
    getSensorReadings();

    // Menampilkan hasil pembacaan sensor
    Serial.printf("Moisture = %.2f \n", hasil);
    Serial.println();

    // Mengirimkan events ke browser berdasarkan
    // hasil pembacaan sensor
    events.send("ping",NULL,millis());

    // Mengirimkan hasil pembacaan sensor
    events.send(String(hasil).c_str(),"moisture",millis());

    // Mengirimkan status kelembaban
    if (statusHasil == 1){
      events.send("Terlalu Kering","status",millis());
    }
    else if (statusHasil == 3){
      events.send("Terlalu Lembab","status",millis());
    }
    else{
      events.send("Lembab","status",millis());
    }

    lastTime = millis();
  }
}
