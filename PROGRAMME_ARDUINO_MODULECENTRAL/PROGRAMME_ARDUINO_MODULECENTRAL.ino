#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

const char* ssid = "ESP32_SERVER";
const char* password = "12345678";

WebServer server(80);

// --- VARIABLES LED RGB ---
// Modifie ces numéros selon les broches (GPIO) de ton ESP32
const int pinRed = 12;   
const int pinGreen = 18; 
const int pinBlue = 4;  

// --- VARIABLES MULTIJOUEURS ---
unsigned long tempsJoueur[4] = {0, 0, 0, 0}; 
int vainqueur = 0; 
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

#define LCD_ADDR 0x27

// --- FONCTION DE CONTRÔLE LED RGB ---
// Si ta LED est à Anode Commune, inverse les HIGH et LOW ici
void setLEDColor(bool r, bool g, bool b) {
  digitalWrite(pinRed, r ? HIGH : LOW);
  digitalWrite(pinGreen, g ? HIGH : LOW);
  digitalWrite(pinBlue, b ? HIGH : LOW);
}

// --- FONCTIONS DE L'ÉCRAN LCD ---
void sendCommand(byte cmd) { Wire.beginTransmission(LCD_ADDR); Wire.write((cmd & 0xF0) | 0x08 | 0x04); Wire.write((cmd & 0xF0) | 0x08); Wire.write(((cmd << 4) & 0xF0) | 0x08 | 0x04); Wire.write(((cmd << 4) & 0xF0) | 0x08); Wire.endTransmission(); delay(2); }
void sendData(byte data) { Wire.beginTransmission(LCD_ADDR); Wire.write((data & 0xF0) | 0x08 | 0x05); Wire.write((data & 0xF0) | 0x08 | 0x01); Wire.write(((data << 4) & 0xF0) | 0x08 | 0x05); Wire.write(((data << 4) & 0xF0) | 0x08 | 0x01); Wire.endTransmission(); delay(2); }
void printText(const char* text) { while (*text) { sendData(*text++); } }
void setCursor(byte col, byte row) { byte row_offsets[] = {0x00, 0x40, 0x14, 0x54}; sendCommand(0x80 | (col + row_offsets[row])); }
void clearLine(byte row) { setCursor(0, row); printText("                "); setCursor(0, row); }

// --- GESTION DE LA PAGE WEB ---
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
    .col { flex: 1; display: flex; flex-direction: column; align-items: center; justify-content: center; border: 4px solid #fff; border-radius: 8px; padding: 15px; background: #222; }
    
    button { background: transparent; color: white; font-size: 16px; font-weight: bold; cursor: pointer; border: none; width: 100%; height: 100%; padding: 15px; transition: 0.1s;}
    button:active { transform: scale(0.95); }

    .btn-lancer { background: #ff9900; color: #111; font-size: 22px; padding: 25px; }
    .tps-box { border: 3px solid #00ffcc; padding: 10px; margin-top: 10px; width: 80%; font-family: monospace; font-size: 16px; background: #111; color: #00ffcc; }
    .main-box { font-size: 20px; color: #ff9900; font-weight: bold; padding: 25px; }
    
    .nav-btn { background: #444; display: flex; flex-direction: column; align-items: center; gap: 5px; }
    .triangle { font-size: 35px; color: #00ffcc; } 
    
    .j1-box { cursor: pointer; transition: 0.1s; }
    .j1-box:active { border-color: #ff3333; background: #331111; }
  </style>
</head>
<body>
  <div class="maquette">
    
    <div class="box" style="padding:0; border-color: #ff9900;">
      <button class="btn-lancer" onclick="fetch('/start_quiz')">LANCER LE QUIZZ</button>
    </div>
    
    <div class="row">
      <div class="col j1-box" onclick="fetch('/buzz?joueur=1')">
        <div style="font-size:18px; font-weight:bold;">Joueur 1 (Toi)</div>
        <div class="tps-box" id="t1">- ms</div>
      </div>
      
      <div class="col">
        <div style="font-size:18px; font-weight:bold;">Joueur 2</div>
        <div class="tps-box" id="t2">- ms</div>
      </div>
      
      <div class="col">
        <div style="font-size:18px; font-weight:bold;">Joueur 3</div>
        <div class="tps-box" id="t3">- ms</div>
      </div>
    </div>
    
    <div class="box main-box" id="vainqueur">En attente d'une question...</div>
    
    <div class="row">
      <div class="box" style="flex:1; padding:0;">
        <button class="nav-btn" onclick="fetch('/prev')">
          <span class="triangle">◀</span>
          <span>Précédente</span>
        </button>
      </div>
      
      <div class="box" style="flex:1; padding:0;">
        <button class="nav-btn" onclick="fetch('/next')">
          <span class="triangle">▶</span>
          <span>Suivante</span>
        </button>
      </div>
    </div>

  </div>

  <script>
    setInterval(() => {
      fetch('/etat').then(r => r.json()).then(d => {
        document.getElementById('t1').innerText = d.t1 > 0 ? d.t1 + " ms" : "- ms";
        document.getElementById('t2').innerText = d.t2 > 0 ? d.t2 + " ms" : "- ms";
        document.getElementById('t3').innerText = d.t3 > 0 ? d.t3 + " ms" : "- ms";
        
        let mainText = "En attente...";
        if (d.v > 0) {
          mainText = "👑 Le Joueur " + d.v + " a la main !";
        } else if (d.etat == 1) {
          mainText = "Préparez-vous...";
        } else if (d.etat == 2) {
          mainText = "GO ! BUZZEZ !";
        }
        document.getElementById('vainqueur').innerText = mainText;
      });
    }, 1000);
  </script>
</body>
</html>
)rawliteral"
  );
}

// --- API : ENVOI DE L'ÉTAT AU SITE WEB ---
void handleEtat() {
  String json = "{";
  json += "\"t1\":" + String(tempsJoueur[1]) + ",";
  json += "\"t2\":" + String(tempsJoueur[2]) + ",";
  json += "\"t3\":" + String(tempsJoueur[3]) + ",";
  json += "\"v\":" + String(vainqueur) + ",";
  json += "\"etat\":" + String(etatQuiz);
  json += "}";
  server.send(200, "application/json", json);
}

// --- ROUTE : DÉMARRAGE DU QUIZ ---
void handleStartQuiz() {
  if (etatQuiz != IDLE) { server.send(200, "text/plain", "Deja en cours"); return; }

  tempsJoueur[1] = 0; tempsJoueur[2] = 0; tempsJoueur[3] = 0;
  vainqueur = 0; 
  
  doitDefiler = false; 
  clearLine(1); printText("Pret...");
  etatQuiz = DELAY;
  chronoQuiz = millis() + random(2000, 5000);
  
  server.send(200, "text/plain", "Quiz lance");
}

// --- ROUTE : NAVIGATION MANUELLE ---
void handleNext() {
  numeroQuestion++;
  etatQuiz = IDLE; vainqueur = 0;
  tempsJoueur[1] = 0; tempsJoueur[2] = 0; tempsJoueur[3] = 0;
  clearLine(0); String q = "Question " + String(numeroQuestion); printText(q.c_str());
  clearLine(1); 
  setLEDColor(false, false, false); // Éteindre la LED
  server.send(200, "text/plain", "Next");
}

void handlePrev() {
  if (numeroQuestion > 1) numeroQuestion--;
  etatQuiz = IDLE; vainqueur = 0;
  tempsJoueur[1] = 0; tempsJoueur[2] = 0; tempsJoueur[3] = 0;
  clearLine(0); String q = "Question " + String(numeroQuestion); printText(q.c_str());
  clearLine(1); 
  setLEDColor(false, false, false); // Éteindre la LED
  server.send(200, "text/plain", "Prev");
}

// --- ROUTE : GESTION DES BUZZERS ---
void handleBuzz() {
  int j = 2; 
  if (server.hasArg("joueur")) { j = server.arg("joueur").toInt(); }

  if (etatQuiz == GO) {
    if (vainqueur == 0) { 
      tempsJoueur[j] = millis() - tempsDebutReaction;
      vainqueur = j; 
      
      etatQuiz = SHOW_SCORE; 
      
      // Allumer la couleur de la LED correspondante au joueur
      if (j == 1) setLEDColor(false, false, true); // Bleu (J1)
      else if (j == 2) setLEDColor(true, false, false); // Rouge (J2)
      else if (j == 3) setLEDColor(false, true, false); // Vert (J3)
      
      clearLine(1); doitDefiler = false; 
      String score = "J" + String(j) + " GAGNE: " + String(tempsJoueur[j]) + "ms";
      printText(score.c_str());
    }
    server.send(200, "text/plain", "Buzzed");
  } 
  else if (etatQuiz == DELAY) {
    etatQuiz = IDLE; 
    setLEDColor(false, false, false); // Éteindre la LED si triche
    messageActuel = "J" + String(j) + " a triche !                "; 
    doitDefiler = true; indexDefilement = 0;
    server.send(200, "text/plain", "Triche");
  } else {
    server.send(200, "text/plain", "Attente");
  }
}

// --- DÉMARRAGE ---
void setup() {
  Serial.begin(115200);
  
  // Initialisation des broches de la LED RGB
  pinMode(pinRed, OUTPUT);
  pinMode(pinGreen, OUTPUT);
  pinMode(pinBlue, OUTPUT);
  setLEDColor(false, false, false); // On s'assure qu'elle est éteinte

  Wire.begin(21, 22); delay(50);
  sendCommand(0x03); delay(5); sendCommand(0x03); delay(5); sendCommand(0x03); delay(1);
  sendCommand(0x02); sendCommand(0x28); sendCommand(0x0C); sendCommand(0x06); sendCommand(0x01); delay(5);

  setCursor(0, 0); String q = "Question " + String(numeroQuestion); printText(q.c_str());

  WiFi.softAP(ssid, password, 1, 0, 4);

  server.on("/", handleRoot);               
  server.on("/etat", handleEtat);         
  server.on("/start_quiz", handleStartQuiz); 
  server.on("/buzz", handleBuzz);
  server.on("/next", handleNext); 
  server.on("/prev", handlePrev); 
  
  server.begin();
}

// --- BOUCLE PRINCIPALE ---
void loop() {
  server.handleClient();
  
  int nbStations = WiFi.softAPgetStationNum();
  if (nbStations != dernierNbStations) {
    dernierNbStations = nbStations;
    String q = "Question " + String(numeroQuestion);
    String c = "C:" + String(nbStations);
    clearLine(0); printText(q.c_str());
    setCursor(16 - c.length(), 0); printText(c.c_str());
  }

  // Clignotement de la LED en Blanc pendant l'attente
  if (etatQuiz == DELAY || etatQuiz == GO) {
    if (millis() - chronoLed >= vitesseLed) {
      chronoLed = millis(); 
      etatLed = !etatLed; 
      setLEDColor(etatLed, etatLed, etatLed); // Blanc
    }
  }

  if (etatQuiz == DELAY && millis() >= chronoQuiz) {
    etatQuiz = GO; clearLine(1); printText("GO !!!!!!!"); tempsDebutReaction = millis(); 
  }
  
  if (doitDefiler == true && etatQuiz == IDLE) {
    if (millis() - chronoDefilement > vitesseDefilement) {
      chronoDefilement = millis();
      String affichage = messageActuel.substring(indexDefilement, indexDefilement + 16);
      setCursor(0, 1); printText(affichage.c_str());
      indexDefilement++;
      if (indexDefilement > messageActuel.length() - 16) indexDefilement = 0;
    }
  }
}