#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

// --- CONFIGURATION WI-FI ---
const char* ssid = "ESP32_SERVER2";
const char* password = "12345678";

WebServer server(80);

// --- VARIABLES POUR LE DÉFILEMENT ---
String messageActuel = "";
bool doitDefiler = false;
unsigned long chronoDefilement = 0;
int indexDefilement = 0;
const int vitesseDefilement = 350;

// --- VARIABLES POUR LE JEU DE RÉACTION ---
enum QuizState { IDLE, DELAY, GO, SHOW_SCORE }; // Ajout de l'état SHOW_SCORE
QuizState etatQuiz = IDLE;
unsigned long chronoQuiz = 0;
unsigned long tempsDebutReaction = 0;
int numeroQuestion = 1; // Compteur de questions

// --- FONCTIONS DE L'ÉCRAN LCD (SANS LIBRAIRIE) ---
#define LCD_ADDR 0x27

void sendCommand(byte cmd) {
  Wire.beginTransmission(LCD_ADDR);
  Wire.write((cmd & 0xF0) | 0x08 | 0x04);
  Wire.write((cmd & 0xF0) | 0x08);
  Wire.write(((cmd << 4) & 0xF0) | 0x08 | 0x04);
  Wire.write(((cmd << 4) & 0xF0) | 0x08);
  Wire.endTransmission();
  delay(2);
}

void sendData(byte data) {
  Wire.beginTransmission(LCD_ADDR);
  Wire.write((data & 0xF0) | 0x08 | 0x05);
  Wire.write((data & 0xF0) | 0x08 | 0x01);
  Wire.write(((data << 4) & 0xF0) | 0x08 | 0x05);
  Wire.write(((data << 4) & 0xF0) | 0x08 | 0x01);
  Wire.endTransmission();
  delay(2);
}

void printText(const char* text) {
  while (*text) {
    sendData(*text++);
  }
}

void setCursor(byte col, byte row) {
  byte row_offsets[] = {0x00, 0x40, 0x14, 0x54};
  sendCommand(0x80 | (col + row_offsets[row]));
}

void clearLine(byte row) {
  setCursor(0, row);
  printText("                "); 
  setCursor(0, row); 
}

// --- GESTION DE LA PAGE WEB (HTML/JS) ---
void handleRoot() {
  server.send(200, "text/html",
R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Contrôle ESP32</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background: #111; color: white; margin-top: 30px; }
    .container { background: #222; padding: 20px; border-radius: 15px; display: inline-block; box-shadow: 0 0 15px #00ffcc; margin-bottom: 20px; width: 85%; max-width: 300px; }
    h2 { color: #00ffcc; margin-top: 0; }
    input[type="text"] { padding: 10px; font-size: 16px; border-radius: 8px; border: none; width: 80%; margin-bottom: 15px; text-align: center; }
    button { padding: 12px 24px; font-size: 16px; background: #00ffcc; color: #111; font-weight: bold; border: none; border-radius: 8px; cursor: pointer; transition: 0.2s; }
    button:active { transform: scale(0.95); }
    .btn-quiz { background: #ff9900; color: white; width: 100%; font-size: 18px; padding: 15px; }
    .quiz-container { box-shadow: 0 0 15px #ff9900; }
  </style>
</head>
<body>
  <div class="container">
    <h2>Contrôle Écran</h2>
    <input type="text" id="champTexte" placeholder="Tape un message...">
    <br>
    <button onclick="envoyerMessage()">Afficher</button>
  </div>

  <div class="container quiz-container">
    <h2>Test de Réaction</h2>
    <button class="btn-quiz" onclick="lancerQuiz()">LANCER LE QUIZ</button>
  </div>

  <script>
    function envoyerMessage() {
      var texte = document.getElementById('champTexte').value;
      texte = texte.replace(/ /g, '_');
      fetch('/message?texte=' + texte);
      document.getElementById('champTexte').value = '';
    }
    function lancerQuiz() {
      fetch('/start_quiz');
    }
  </script>
</body>
</html>
)rawliteral"
  );
}

// --- ROUTE WEB : DECLENCHEMENT DU JEU ---
void handleStartQuiz() {
  // Interdit de relancer un quiz si on est déjà en cours ou en affichage de score
  if (etatQuiz != IDLE) {
    server.send(200, "text/plain", "Deja en cours");
    return;
  }

  doitDefiler = false; 
  clearLine(1); 
  printText("Pret...");
  
  etatQuiz = DELAY;
  chronoQuiz = millis() + random(2000, 5000);
  
  server.send(200, "text/plain", "Quiz lance");
}

// --- GESTION DES REQUÊTES REÇUES (WEB ET INTERRUPTEUR) ---
void handleCustomMessage() {
  // Cas 1 : Le joueur a basculé l'interrupteur au bon moment (pendant le GO !)
  if (etatQuiz == GO) {
    unsigned long tempsReaction = millis() - tempsDebutReaction;
    
    // On passe à l'état d'affichage du score et on règle le chrono sur +5 secondes
    etatQuiz = SHOW_SCORE; 
    chronoQuiz = millis() + 5000; 
    
    clearLine(1); 
    doitDefiler = false; 
    String score = "Temps: " + String(tempsReaction) + " ms";
    printText(score.c_str());
    
    server.send(200, "text/plain", "Score enregistre");
    return;
  }
  
  // Cas 2 : Le joueur a bougé l'interrupteur trop tôt (Triche)
  if (etatQuiz == DELAY) {
    etatQuiz = IDLE; // On retourne au repos immédiatement (pas de changement de question)
    
    messageActuel = "Triche ! Trop tot                "; 
    doitDefiler = true;
    indexDefilement = 0;
    
    server.send(200, "text/plain", "Triche");
    return;
  }

  // Ignorer les changements d'interrupteur classiques si on attend la question suivante
  if (etatQuiz == SHOW_SCORE) {
    server.send(200, "text/plain", "Attente");
    return;
  }

  // Cas 3 : Mode normal (affichage de texte envoyé par le site)
  if (server.hasArg("texte")) {
    String messageRecu = server.arg("texte");
    messageRecu.replace("_", " "); 
    
    messageActuel = messageRecu;
    
    if (messageActuel.length() > 16) {
      doitDefiler = true;
      indexDefilement = 0;
      messageActuel += "                "; 
    } else {
      doitDefiler = false;
      clearLine(1); 
      printText(messageActuel.c_str()); 
    }
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Erreur");
  }
}

// --- DÉMARRAGE ---
void setup() {
  Serial.begin(115200);

  Wire.begin(21, 22); 
  delay(50);
  sendCommand(0x03); delay(5);
  sendCommand(0x03); delay(5);
  sendCommand(0x03); delay(1);
  sendCommand(0x02); 
  sendCommand(0x28); 
  sendCommand(0x0C); 
  sendCommand(0x06); 
  sendCommand(0x01); delay(5);

  // Initialisation de la ligne du haut avec la Question 1
  setCursor(0, 0);
  String q = "Question " + String(numeroQuestion);
  printText(q.c_str());

  WiFi.softAP(ssid, password);

  server.on("/", handleRoot);               
  server.on("/message", handleCustomMessage); 
  server.on("/start_quiz", handleStartQuiz); 
  
  server.begin();
}

// --- BOUCLE PRINCIPALE ---
void loop() {
  server.handleClient();
  
  // 1. Gestion de l'attente aléatoire avant le "GO !"
  if (etatQuiz == DELAY) {
    if (millis() >= chronoQuiz) {
      etatQuiz = GO;
      clearLine(1);
      printText("GO !!!!!!!");
      tempsDebutReaction = millis(); 
    }
  }
  
  // 2. Gestion de l'affichage du score pendant 5 secondes avant la question suivante
  if (etatQuiz == SHOW_SCORE) {
    if (millis() >= chronoQuiz) {
      numeroQuestion++; // On incrémente le numéro de la question
      
      // Mise à jour propre de la ligne du haut
      clearLine(0);
      String q = "Question " + String(numeroQuestion);
      printText(q.c_str());
      
      // Nettoyage de la ligne du bas
      clearLine(1);
      
      etatQuiz = IDLE; // Le jeu est prêt pour la nouvelle question
    }
  }
  
  // 3. Gestion du défilement des longs messages (uniquement en mode IDLE)
  if (doitDefiler == true && etatQuiz == IDLE) {
    if (millis() - chronoDefilement > vitesseDefilement) {
      chronoDefilement = millis();
      String affichage = messageActuel.substring(indexDefilement, indexDefilement + 16);
      setCursor(0, 1);
      printText(affichage.c_str());
      indexDefilement++;
      if (indexDefilement > messageActuel.length() - 16) {
        indexDefilement = 0;
      }
    }
  }
}