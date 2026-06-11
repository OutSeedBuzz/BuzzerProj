#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

const char* ssid = "ESP32_SERVER";
const char* password = "12345678";

WebServer server(80);

// --- VARIABLES LED RGB ET AUDIO ---
const int pinRed = 12;   
const int pinGreen = 18; 
const int pinBlue = 4;  
const int pinBuzzer = 25; 
const int volumeSon = 128; // Volume (0 à 255)

bool anodeCommune = false; 

// --- VARIABLES MULTIJOUEURS ET TRICHE ---
unsigned long tempsJoueur[4] = {0, 0, 0, 0}; 
int vainqueur = 0; 
int joueurBloque = 0; // Joueur bloqué après une mauvaise réponse
int viesTriche[4] = {0, 3, 3, 3}; // 3 chances par joueur pour toute la partie
bool exclusQuestion[4] = {false, false, false, false}; // Bloqué pour la question en cours

enum QuizState { IDLE, DELAY, GO, SHOW_SCORE }; 
QuizState etatQuiz = IDLE;
unsigned long chronoQuiz = 0;
unsigned long tempsDebutReaction = 0;
int numeroQuestion = 1; 

// --- VARIABLES ÉCRAN ET LED ---
String messageActuel = "";
bool doitDefiler = false;
unsigned long chronoDefilement = 0;
int indexDefilement = 0;
const int vitesseDefilement = 350;
unsigned long chronoLed = 0;
const int vitesseLed = 150; 
bool etatLed = LOW;
int dernierNbStations = -1;

unsigned long chronoRGB = 0;
byte hueRGB = 0; 
const int vitesseRGB = 15; 

#define LCD_ADDR 0x27

char lcdBuffer[2][17] = {"                ", "                "};
byte currentRow = 0;
byte currentCol = 0;

// --- FONCTIONS AUDIO ET LED ---
void playVolumeTone(int freq, int duration) {
  ledcAttach(pinBuzzer, freq, 8); 
  ledcWrite(pinBuzzer, volumeSon / 2); 
  delay(duration);
  ledcWrite(pinBuzzer, 0); 
  ledcDetach(pinBuzzer);
}

void setLEDColorPWM(byte r, byte g, byte b) {
  if (anodeCommune) { r = 255 - r; g = 255 - g; b = 255 - b; }
  analogWrite(pinRed, r); analogWrite(pinGreen, g); analogWrite(pinBlue, b);
}

void setLEDColor(bool r, bool g, bool b) {
  setLEDColorPWM(r ? 255 : 0, g ? 255 : 0, b ? 255 : 0);
}

// --- FONCTIONS ÉCRAN LCD ---
void sendCommand(byte cmd) { 
  Wire.beginTransmission(LCD_ADDR); 
  Wire.write((cmd & 0xF0) | 0x08 | 0x04); Wire.write((cmd & 0xF0) | 0x08); 
  Wire.write(((cmd << 4) & 0xF0) | 0x08 | 0x04); Wire.write(((cmd << 4) & 0xF0) | 0x08); 
  Wire.endTransmission(); delayMicroseconds(100); 
}

void sendData(byte data) { 
  Wire.beginTransmission(LCD_ADDR); 
  Wire.write((data & 0xF0) | 0x08 | 0x05); Wire.write((data & 0xF0) | 0x08 | 0x01); 
  Wire.write(((data << 4) & 0xF0) | 0x08 | 0x05); Wire.write(((data << 4) & 0xF0) | 0x08 | 0x01); 
  Wire.endTransmission(); delayMicroseconds(100); 
}

void setCursor(byte col, byte row) { 
  currentCol = col; currentRow = row;
  byte row_offsets[] = {0x00, 0x40, 0x14, 0x54}; 
  sendCommand(0x80 | (col + row_offsets[row])); 
}

void printText(const char* text) { 
  while (*text) { 
    if (currentCol < 16 && currentRow < 2) lcdBuffer[currentRow][currentCol] = *text;
    currentCol++; sendData(*text++); 
  } 
}

void clearLine(byte row) { setCursor(0, row); printText("                "); setCursor(0, row); }

// --- PAGE WEB ---
void handleRoot() {
  server.send(200, "text/html",
R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Buzzer Master</title>
  <style>
    body { background: #111; color: white; font-family: Arial, sans-serif; display: flex; justify-content: center; padding: 10px; margin: 0; }
    .maquette { display: flex; flex-direction: column; gap: 15px; width: 100%; max-width: 500px; margin-top: 10px; }
    .box { border: 4px solid #fff; border-radius: 8px; background: #222; text-align: center; }
    .row { display: flex; gap: 15px; width: 100%; }
    .col { flex: 1; display: flex; flex-direction: column; align-items: center; justify-content: center; border: 4px solid #fff; border-radius: 8px; padding: 15px; background: #222; cursor: pointer; transition: 0.1s; }
    .col:active { transform: scale(0.95); }
    button { background: transparent; color: white; font-size: 16px; font-weight: bold; cursor: pointer; border: none; width: 100%; height: 100%; padding: 15px; transition: 0.1s;}
    button:active { transform: scale(0.95); }
    .btn-lancer { background: #ff9900; color: #111; font-size: 22px; padding: 25px; }
    .tps-box { border: 3px solid #00ffcc; padding: 10px; margin-top: 10px; width: 80%; font-family: monospace; font-size: 16px; background: #111; color: #00ffcc; }
    .main-box { font-size: 20px; color: #ff9900; font-weight: bold; padding: 25px; }
    .nav-btn { background: #444; display: flex; flex-direction: column; align-items: center; gap: 5px; }
    .triangle { font-size: 35px; color: #00ffcc; } 
    .lcd-container { background: #7a9a3b; border: 4px solid #333; border-radius: 8px; padding: 10px; display: flex; justify-content: center; box-shadow: inset 0 0 10px rgba(0,0,0,0.7); }
    .lcd-screen { font-family: 'Courier New', Courier, monospace; font-size: 22px; font-weight: bold; color: #111; white-space: pre; line-height: 1.3; }
    .vies { font-size: 14px; margin-top: 5px; letter-spacing: 2px; }
  </style>
</head>
<body>
  <div class="maquette">
    <div class="box" style="padding:0; border-color: #ff9900;"><button class="btn-lancer" onclick="fetch('/start_quiz')">LANCER LE QUIZZ</button></div>
    <div class="lcd-container"><div class="lcd-screen" id="lcd-div"> <br> </div></div>
    
    <div class="row">
      <div class="col" onclick="fetch('/buzz?joueur=1')">
        <div style="font-size:18px; font-weight:bold;" id="nom-j1">Joueur 1</div>
        <div class="vies" id="vies-j1"></div>
        <div class="tps-box" id="t1">- ms</div>
      </div>
      <div class="col" onclick="fetch('/buzz?joueur=2')">
        <div style="font-size:18px; font-weight:bold;" id="nom-j2">Joueur 2</div>
        <div class="vies" id="vies-j2"></div>
        <div class="tps-box" id="t2">- ms</div>
      </div>
      <div class="col" onclick="fetch('/buzz?joueur=3')">
        <div style="font-size:18px; font-weight:bold;" id="nom-j3">Joueur 3</div>
        <div class="vies" id="vies-j3"></div>
        <div class="tps-box" id="t3">- ms</div>
      </div>
    </div>
    
    <div class="box main-box" id="vainqueur">En attente d'une question...</div>
    
    <div class="box" style="padding:0; border-color: #ff3333; margin-top: 5px;">
      <button style="background: #ff3333; color: white; font-size: 18px; padding: 15px;" onclick="fetch('/relancer')">
        ❌ MAUVAISE RÉPONSE (Relancer)
      </button>
    </div>

    <div class="row" style="margin-top: 5px;">
      <div class="box" style="flex:1; padding:0;"><button class="nav-btn" onclick="fetch('/prev')"><span class="triangle">◀</span><span>Précédente</span></button></div>
      <div class="box" style="flex:1; padding:0;"><button class="nav-btn" onclick="fetch('/next')"><span class="triangle">▶</span><span>Suivante</span></button></div>
    </div>
  </div>
  
  <script>
    setInterval(() => {
      fetch('/etat').then(r => r.json()).then(d => {
        document.getElementById('t1').innerText = d.t1 > 0 ? d.t1 + " ms" : "- ms";
        document.getElementById('t2').innerText = d.t2 > 0 ? d.t2 + " ms" : "- ms";
        document.getElementById('t3').innerText = d.t3 > 0 ? d.t3 + " ms" : "- ms";
        
        // Fonction pour gérer l'affichage des vies et des blocages
        function updatePlayerUI(num, lives, isBlockedForAns, isExcluded) {
          let blocked = (isBlockedForAns == num || isExcluded == 1);
          document.getElementById('nom-j'+num).innerText = blocked ? "Joueur " + num + " 🚫" : "Joueur " + num + (num==1?" (Toi)":"");
          document.getElementById('vies-j'+num).innerText = blocked ? "" : "🔴".repeat(lives);
        }

        updatePlayerUI(1, d.v1, d.b, d.e1);
        updatePlayerUI(2, d.v2, d.b, d.e2);
        updatePlayerUI(3, d.v3, d.b, d.e3);

        let mainText = "En attente...";
        if (d.v > 0) mainText = "👑 Le Joueur " + d.v + " a la main !";
        else if (d.etat == 1) mainText = "Préparez-vous...";
        else if (d.etat == 2) mainText = "GO ! BUZZEZ !";
        
        document.getElementById('vainqueur').innerText = mainText;
        document.getElementById('lcd-div').innerText = d.lcd0 + '\n' + d.lcd1;
      });
    }, 150); 
  </script>
</body>
</html>
)rawliteral"
  );
}

// --- API ÉTAT ---
void handleEtat() {
  String json = "{";
  json += "\"t1\":" + String(tempsJoueur[1]) + ",\"t2\":" + String(tempsJoueur[2]) + ",\"t3\":" + String(tempsJoueur[3]) + ",";
  json += "\"v\":" + String(vainqueur) + ",\"etat\":" + String(etatQuiz) + ",";
  json += "\"lcd0\":\"" + String(lcdBuffer[0]) + "\",\"lcd1\":\"" + String(lcdBuffer[1]) + "\",";
  json += "\"b\":" + String(joueurBloque) + ",";
  json += "\"v1\":" + String(viesTriche[1]) + ",\"v2\":" + String(viesTriche[2]) + ",\"v3\":" + String(viesTriche[3]) + ",";
  json += "\"e1\":" + String(exclusQuestion[1]) + ",\"e2\":" + String(exclusQuestion[2]) + ",\"e3\":" + String(exclusQuestion[3]);
  json += "}";
  server.send(200, "application/json", json);
}

// --- LOGIQUE DU JEU ---
void handleRelancer() {
  if (etatQuiz == SHOW_SCORE) {
    playVolumeTone(150, 400); 
    joueurBloque = vainqueur; // On bloque le joueur qui a donné la mauvaise réponse
    vainqueur = 0; tempsJoueur[1] = 0; tempsJoueur[2] = 0; tempsJoueur[3] = 0;
    setLEDColor(false, false, false);
    
    doitDefiler = false; clearLine(1); printText("Relance...");
    etatQuiz = DELAY; chronoQuiz = millis() + random(1500, 3000); 
    server.send(200, "text/plain", "OK");
  } else {
    server.send(200, "text/plain", "Attente");
  }
}

void handleStartQuiz() {
  if (etatQuiz != IDLE) return;
  tempsJoueur[1] = 0; tempsJoueur[2] = 0; tempsJoueur[3] = 0; 
  vainqueur = 0; joueurBloque = 0; 
  
  doitDefiler = false; clearLine(1); printText("Pret...");
  etatQuiz = DELAY; chronoQuiz = millis() + random(2000, 5000);
  server.send(200, "text/plain", "OK");
}

void handleNext() {
  numeroQuestion++; etatQuiz = IDLE; vainqueur = 0; joueurBloque = 0;
  // On pardonne les exclusions de triche pour la nouvelle question
  exclusQuestion[1] = false; exclusQuestion[2] = false; exclusQuestion[3] = false;
  clearLine(0); String q = "Question " + String(numeroQuestion); printText(q.c_str());
  clearLine(1); server.send(200, "text/plain", "OK");
}

void handlePrev() {
  if (numeroQuestion > 1) numeroQuestion--; etatQuiz = IDLE; vainqueur = 0; joueurBloque = 0;
  // On pardonne les exclusions de triche pour la nouvelle question
  exclusQuestion[1] = false; exclusQuestion[2] = false; exclusQuestion[3] = false;
  clearLine(0); String q = "Question " + String(numeroQuestion); printText(q.c_str());
  clearLine(1); server.send(200, "text/plain", "OK");
}

void handleBuzz() {
  int j = 2; if (server.hasArg("joueur")) j = server.arg("joueur").toInt();
  
  // Si le joueur est bloqué (soit par mauvaise réponse, soit car 0 vie de triche)
  if (j == joueurBloque || exclusQuestion[j]) {
    server.send(200, "text/plain", "Bloque");
    return;
  }

  if (etatQuiz == GO && vainqueur == 0) {
    tempsJoueur[j] = millis() - tempsDebutReaction; vainqueur = j; etatQuiz = SHOW_SCORE;
    playVolumeTone(800, 600);
    clearLine(1); String score = "J" + String(j) + " GAGNE: " + String(tempsJoueur[j]) + "ms"; printText(score.c_str());
  } 
  else if (etatQuiz == DELAY) {
    etatQuiz = IDLE; setLEDColor(false, false, false); playVolumeTone(150, 400);
    
    // GESTION DES CHANCES DE TRICHE
    if (viesTriche[j] > 0) {
      viesTriche[j]--; // Perd une vie
      messageActuel = "J" + String(j) + " TRICHE! Reste " + String(viesTriche[j]) + " vie(s)    "; 
    } else {
      exclusQuestion[j] = true; // Exclu pour cette question
      messageActuel = "J" + String(j) + " EXCLU POUR LA Q!       "; 
    }
    
    doitDefiler = true; indexDefilement = 0;
  }
  server.send(200, "text/plain", "OK");
}

// --- SETUP ET LOOP ---
void setup() {
  Serial.begin(115200);
  pinMode(pinRed, OUTPUT); pinMode(pinGreen, OUTPUT); pinMode(pinBlue, OUTPUT);
  setLEDColor(false, false, false); 
  
  Wire.begin(21, 22); delay(50);
  sendCommand(0x03); sendCommand(0x02); sendCommand(0x28); sendCommand(0x0C); sendCommand(0x06); sendCommand(0x01);
  setCursor(0, 0); String q = "Question " + String(numeroQuestion); printText(q.c_str());
  
  WiFi.softAP(ssid, password, 1, 0, 4);
  server.on("/", handleRoot); server.on("/etat", handleEtat); 
  server.on("/relancer", handleRelancer);
  server.on("/start_quiz", handleStartQuiz); server.on("/buzz", handleBuzz);
  server.on("/next", handleNext); server.on("/prev", handlePrev); 
  server.begin();
}

void loop() {
  server.handleClient();
  if (etatQuiz == IDLE) {
    if (millis() - chronoRGB >= vitesseRGB) {
      chronoRGB = millis(); hueRGB++; int r, g, b;
      if (hueRGB < 85) { r = 255 - hueRGB * 3; g = 0; b = hueRGB * 3; }
      else if (hueRGB < 170) { int t = hueRGB - 85; r = 0; g = t * 3; b = 255 - t * 3; }
      else { int t = hueRGB - 170; r = t * 3; g = 255 - t * 3; b = 0; }
      setLEDColorPWM(r, g, b); 
    }
  } else if (etatQuiz == DELAY) {
    if (millis() - chronoLed >= vitesseLed) { chronoLed = millis(); etatLed = !etatLed; setLEDColor(etatLed, etatLed, etatLed); }
  } else if (etatQuiz == GO) { setLEDColor(false, false, false); }
  
  if (etatQuiz == DELAY && millis() >= chronoQuiz) { etatQuiz = GO; clearLine(1); printText("GO !!!!!!!"); tempsDebutReaction = millis(); }
  
  if (doitDefiler && etatQuiz == IDLE) {
    if (millis() - chronoDefilement > vitesseDefilement) {
      chronoDefilement = millis(); String affichage = messageActuel.substring(indexDefilement, indexDefilement + 16);
      setCursor(0, 1); printText(affichage.c_str()); indexDefilement = (indexDefilement + 1) % (messageActuel.length() - 15);
    }
  }
}