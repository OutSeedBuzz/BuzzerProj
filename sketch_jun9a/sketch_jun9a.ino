#include <WiFi.h>
#include <WebServer.h>

// 📡 WiFi de l'ESP32
const char* ssid = "ESP32_SERVER";
const char* password = "12345678";

WebServer server(80);

void handleRoot() {
  server.send(200, "text/html",
R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>ESP32 Server</title>

  <style>
    body {
      font-family: Arial;
      text-align: center;
      background: #111;
      color: white;
      margin-top: 60px;
    }

    .card {
      background: #222;
      padding: 20px;
      border-radius: 15px;
      display: inline-block;
      box-shadow: 0 0 10px #00ffcc;
    }

    h1 {
      color: #00ffcc;
    }

    button {
      padding: 10px 20px;
      margin: 10px;
      border: none;
      border-radius: 10px;
      cursor: pointer;
      font-size: 16px;
    }

    .btn1 { background: #00ffcc; }
    .btn2 { background: #ff4444; color: white; }
  </style>
</head>

<body>
  <div class="card">
    <h1>ESP32 Server 🚀</h1>

    <button class="btn1" onclick="alert('Bouton 1 cliqué')">Bouton 1</button>
    <button class="btn2" onclick="alert('Bouton 2 cliqué')">Bouton 2</button>
  </div>
</body>
</html>
)rawliteral"
  );
}

void setup() {
  Serial.begin(115200);

  WiFi.softAP(ssid, password);

  Serial.println("Access Point créé !");
  Serial.print("IP ESP32 : ");
  Serial.println(WiFi.softAPIP().toString());

  server.on("/", handleRoot);
  server.begin();

  Serial.println("Serveur HTTP démarré");
}

void loop() {
  server.handleClient();
}