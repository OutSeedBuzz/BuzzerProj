// ==================================================================================
// INCLUSION DES BIBLIOTHÈQUES (Les outils nécessaires au fonctionnement de l'ESP32)
// ==================================================================================
#include <WiFi.h>       // Gère la création du réseau Wi-Fi de l'ESP32
#include <WebServer.h>  // Permet de créer la page web interactive
#include <Wire.h>       // Gère la communication I2C avec l'écran LCD

// ==================================================================================
// CONFIGURATION DU RÉSEAU WI-FI (Modifiable pour changer de nom ou de mot de passe)
// ==================================================================================
const char* ssid = "ESP32_SERVER";  // Nom du réseau Wi-Fi généré par l'ESP32
const char* password = "12345678";  // Mot de passe du Wi-Fi (8 caractères minimum)

// Création du serveur web sur le port standard 80 (HTTP)
WebServer server(80);

// ==================================================================================
// CONFIGURATION DES COMPOSANTS MATÉRIELS (Broches / Pins)
// ==================================================================================
const int pinRed = 12;     // Broche reliée à la couleur Rouge de la LED RGB
const int pinGreen = 18;   // Broche reliée à la couleur Verte de la LED RGB
const int pinBlue = 4;     // Broche reliée à la couleur Bleue de la LED RGB
const int pinBuzzer = 25;  // Broche reliée au Buzzer (Audio)

// Réglages audio et matériel
const int volumeSon = 128;   // Volume du buzzer : de 0 (muet) à 255 (maximum)
bool anodeCommune = false;   // Modifier en 'true' si la LED RGB s'allume à l'envers 

// ==================================================================================
// VARIABLES DU JEU (Gestion des scores, des triches et des joueurs)
// ==================================================================================
unsigned long tempsJoueur[4] = {0, 0, 0, 0};      // Stocke le temps de réaction de chaque joueur en millisecondes
int vainqueur = 0;                                // Contient le numéro du joueur qui a buzzé en premier (0 = personne)
int joueurBloque = 0;                             // Bloque le joueur qui a donné une mauvaise réponse au tour précédent
int viesTriche[4] = {0, 3, 3, 3};                 // Nombre de "vies" (essais) autorisées pour les buzz anticipés (Joueurs 1, 2, 3)
bool exclusQuestion[4] = {false, false, false, false}; // Passe à 'true' si un joueur n'a plus de vies et est banni pour la question

// Définition des différents états possibles du jeu (Machine à états)
enum QuizState { 
  IDLE,        // En attente : Le jeu attend que l'animateur lance la question
  DELAY,       // Préparez-vous : Compte à rebours aléatoire avant de pouvoir buzzer
  GO,          // BUZZEZ ! : Le signal est donné, les joueurs peuvent buzzer
  SHOW_SCORE   // Score : Un joueur a buzzé, on affiche son temps et on attend la validation
}; 

QuizState etatQuiz = IDLE;       // Au démarrage, le jeu est en attente (IDLE)
unsigned long chronoQuiz = 0;    // Chronomètre pour gérer la fin du compte à rebours aléatoire
unsigned long tempsDebutReaction = 0; // Moment exact (en millisecondes) où le "GO !" est apparu
int numeroQuestion = 1;          // Numéro de la question actuelle

// ==================================================================================
// GESTION DE L'AFFICHAGE ET DES ANIMATIONS LUMINEUSES
// ==================================================================================
String messageActuel = "";       // Stocke le texte long qui doit défiler sur l'écran
bool doitDefiler = false;        // Active ou désactive le défilement du texte
unsigned long chronoDefilement = 0; 
int indexDefilement = 0;
const int vitesseDefilement = 350; // Temps (en ms) entre chaque déplacement de lettre (plus petit = plus rapide)

unsigned long chronoLed = 0;
const int vitesseLed = 150;      // Vitesse de clignotement de la LED pendant le "Préparez-vous" (en ms)
bool etatLed = LOW;              // État actuel de la LED de statut (allumée/éteinte)
int dernierNbStations = -1;      // Permet de suivre le nombre d'appareils connectés au Wi-Fi

unsigned long chronoRGB = 0;
byte hueRGB = 0;                 // Position dans le dégradé arc-en-ciel
const int vitesseRGB = 15;       // Vitesse de transition des couleurs de l'arc-en-ciel (en ms)

// Configuration de l'écran LCD
#define LCD_ADDR 0x27            // Adresse I2C de l'écran (0x27 est la plus courante)
char lcdBuffer[2][17] = {"                ", "                "}; // Copie virtuelle de ce qui est écrit sur l'écran (2 lignes de 16 caractères)
byte currentRow = 0;             // Ligne actuelle du curseur (0 ou 1)
byte currentCol = 0;             // Colonne actuelle du curseur (0 à 15)

// ==================================================================================
// FONCTIONS AUDIO ET LUMIÈRE
// ==================================================================================

// Rôle : Fait sonner le buzzer avec une fréquence (note) et une durée précise
void playVolumeTone(int freq, int duration) {
  ledcAttach(pinBuzzer, freq, 8);      // Configure la note de musique sur la broche du buzzer
  ledcWrite(pinBuzzer, volumeSon / 2); // Active le son avec le volume choisi
  delay(duration);                     // Attend pendant la durée du son
  ledcWrite(pinBuzzer, 0);             // Coupe le son
  ledcDetach(pinBuzzer);               // Libère la broche du buzzer
}

// Rôle : Permet de mélanger précisément le Rouge, le Vert et le Bleu (de 0 à 255) pour créer n'importe quelle couleur
void setLEDColorPWM(byte r, byte g, byte b) {
  if (anodeCommune) { r = 255 - r; g = 255 - g; b = 255 - b; } // Inverse les valeurs si le matériel le demande
  analogWrite(pinRed, r); 
  analogWrite(pinGreen, g); 
  analogWrite(pinBlue, b);
}

// Rôle : Raccourci simple pour allumer ou éteindre directement les couleurs (true = allumé, false = éteint)
void setLEDColor(bool r, bool g, bool b) {
  setLEDColorPWM(r ? 255 : 0, g ? 255 : 0, b ? 255 : 0);
}

// ==================================================================================
// FONCTIONS TECHNIQUES POUR L'ÉCRAN LCD (Communication I2C directe sans bibliothèque externe)
// ==================================================================================

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

// Rôle : Déplace le curseur d'écriture à une position précise (Colonne, Ligne)
void setCursor(byte col, byte row) { 
  currentCol = col; currentRow = row;
  byte row_offsets[] = {0x00, 0x40, 0x14, 0x54}; 
  sendCommand(0x80 | (col + row_offsets[row])); 
}

// Rôle : Écrit un texte sur l'écran et met à jour la copie virtuelle (le buffer)
void printText(const char* text) { 
  while (*text) { 
    if (currentCol < 16 && currentRow < 2) lcdBuffer[currentRow][currentCol] = *text;
    currentCol++; sendData(*text++); 
  } 
}

// Rôle : Efface entièrement une ligne de l'écran (remplit d'espaces vides)
void clearLine(byte row) { 
  setCursor(0, row); 
  printText("                "); 
  setCursor(0, row); 
}

// Rôle : Met à jour la première ligne de l'écran avec le numéro de la question et le nombre de joueurs connectés
void majLigneQuestion() {
  String q = "Question " + String(numeroQuestion);
  String c = "C:" + String(WiFi.softAPgetStationNum()); // "C:2" signifie 2 téléphones/consoles connectés au Wi-Fi
  while (q.length() < 16 - c.length()) { q += " "; }     // Aligne le compteur de connexion tout à droite
  q += c; 
  setCursor(0, 0); printText(q.c_str());
}

// ==================================================================================
// INTERFACE INTERNET (Le code HTML / CSS / JS envoyé aux téléphones des joueurs)
// ==================================================================================

void handleRoot() {
  server.send(200, "text/html",
R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <style>
    /* ==========================================================================
       SECTION 1 : L'APPARENCE VISUELLE (CSS)
       C'est ici que vous pouvez modifier les couleurs, les tailles et les arrondis.
       ========================================================================== */

    /* Le fond de la page entière */
    body { 
      background: #111;          /* Couleur de fond (noir très sombre) */
      color: white;              /* Couleur du texte par défaut (blanc) */
      font-family: Arial, sans-serif; 
      display: flex; 
      justify-content: center; 
      padding: 10px; 
      margin: 0; 
    }
    
    /* Le boîtier principal (la grosse boîte noire qui contient tout) */
    .maquette { 
      display: flex; 
      flex-direction: column; 
      gap: 15px;                 /* Espace entre les blocs (en pixels) */
      width: 100%; 
      max-width: 500px;          /* Largeur maximale de la console à l'écran */
      margin-top: 10px; 
    }
    
    /* Le style de base de toutes les boîtes/cadres */
    .box { 
      border: 4px solid #fff;    /* Bordure blanche de 4 pixels d'épaisseur */
      border-radius: 8px;        /* Arrondi des angles (plus le chiffre est grand, plus c'est rond) */
      background: #222;          /* Couleur de fond interne (gris foncé) */
      text-align: center; 
    }
    
    /* Aligne les éléments sur une même ligne (ex: les 3 joueurs côte à côte) */
    .row { 
      display: flex; 
      gap: 15px;                 /* Espace entre les colonnes des joueurs */
      width: 100%; 
    }
    
    /* La case individuelle de chaque joueur (Bouton virtuel) */
    .col { 
      flex: 1; 
      display: flex; 
      flex-direction: column; 
      align-items: center; 
      justify-content: center; 
      border: 4px solid #fff; 
      border-radius: 8px; 
      padding: 15px;             /* Espace intérieur pour que la case soit gonflée */
      background: #222; 
      cursor: pointer;           /* Change le curseur de la souris en "main" au survol */
      transition: 0.1s; 
    }
    /* Effet visuel : la case rétrécit légèrement quand on clique dessus (effet "bouton mécanique") */
    .col:active { 
      transform: scale(0.95); 
    }
    
    /* Style général pour TOUS les boutons de l'interface */
    button { 
      background: transparent; 
      color: white; 
      font-size: 16px; 
      font-weight: bold; 
      cursor: pointer; 
      border: none; 
      width: 100%; 
      height: 100%; 
      padding: 15px; 
      transition: 0.1s;
    }
    /* Effet d'enfoncement pour les boutons */
    button:active { 
      transform: scale(0.95); 
    }
    
    /* Le gros bouton orange "LANCER LE QUIZZ" */
    .btn-lancer { 
      background: #ff9900;       /* Couleur orange (Code HTML : #ff9900) */
      color: #111;               /* Écriture en noir pour le contraste */
      font-size: 22px;           /* Taille du texte plus grande */
      padding: 25px;             /* Bouton très épais */
    }
    
    /* Le petit encadré turquoise pour afficher le temps (en millisecondes) */
    .tps-box { 
      border: 3px solid #00ffcc; /* Bordure turquoise */
      padding: 10px; 
      margin-top: 10px; 
      width: 80%; 
      font-family: monospace;    /* Style d'écriture type "ordinateur rétro" */
      font-size: 16px; 
      background: #111; 
      color: #00ffcc;            /* Texte turquoise lumineux */
    }
    
    /* La zone de texte centrale (Annonce du vainqueur) */
    .main-box { 
      font-size: 20px; 
      color: #ff9900; 
      font-weight: bold; 
      padding: 25px; 
    }
    
    /* Boutons de navigation (Précédent / Suivant) */
    .nav-btn { 
      background: #444;          /* Fond gris */
      display: flex; 
      flex-direction: column; 
      align-items: center; 
      gap: 5px; 
    }
    /* Les petites flèches triangulaires (◀ et ▶) */
    .triangle { 
      font-size: 35px; 
      color: #00ffcc; 
    } 
    
    /* Simulation de l'écran LCD physique (vert rétroéclairé) */
    .lcd-container { 
      background: #7a9a3b;       /* Couleur vert "GameBoy" d'origine */
      border: 4px solid #333; 
      border-radius: 8px; 
      padding: 10px; 
      display: flex; 
      justify-content: center; 
      box-shadow: inset 0 0 10px rgba(0,0,0,0.7); /* Ombre interne pour donner du relief */
    }
    /* Le texte à l'intérieur de l'écran LCD */
    .lcd-screen { 
      font-family: 'Courier New', Courier, monospace; /* Police rétro */
      font-size: 22px; 
      font-weight: bold; 
      color: #111; 
      white-space: pre;          /* Permet de garder les retours à la ligne comme dans le code */
      line-height: 1.3; 
    }
    
    /* Zone d'affichage des petits cœurs (vies) */
    .vies { 
      font-size: 14px; 
      margin-top: 5px; 
      letter-spacing: 2px;       /* Espace entre les cœurs */
    }
  </style>
</head>
<body>

  <!-- ==========================================================================
       SECTION 2 : LA STRUCTURE HTML (le contenu affiché à l'écran)
       Chaque bouton ci-dessous envoie une requête au serveur (fetch) vers une
       route précise. C'est cette route (ex: /buzz?joueur=1) qui doit être gérée
       côté serveur (backend / microcontrôleur) pour déclencher une action.
       ========================================================================== -->

  <!-- Conteneur principal : regroupe tous les blocs de l'interface les uns
       sous les autres (grâce à flex-direction: column dans le CSS) -->
  <div class="maquette">

    <!-- Bloc contenant le bouton principal "LANCER LE QUIZZ" -->
    <!-- style="padding:0" retire l'espace intérieur pour que le bouton orange
         remplisse tout le cadre ; border-color le repasse en orange -->
    <div class="box" style="padding:0; border-color: #ff9900;">
      <!-- Bouton qui démarre le quizz : au clic, envoie une requête GET vers
           la route "/start_quiz" (le serveur doit écouter cette route) -->
      <button class="btn-lancer" onclick="fetch('/start_quiz')">LANCER LE QUIZZ</button>
    </div>
    
    <!-- Cadre extérieur qui simule le boîtier d'un écran LCD physique -->
    <div class="lcd-container">
      <!-- Écran LCD : id="lcd-div" permet au code JavaScript (non montré ici)
           de venir modifier ce texte dynamiquement (ex: afficher la question) -->
      <div class="lcd-screen" id="lcd-div"> <br> </div>
    </div>
    
    <!-- Ligne horizontale regroupant les 3 colonnes des joueurs -->
    <div class="row">
      
      <!-- Colonne du Joueur 1 : toute la case est cliquable et envoie
           une requête vers "/buzz?joueur=1" quand on appuie dessus -->
      <div class="col" onclick="fetch('/buzz?joueur=1')">
        <!-- Nom du joueur 1 : id="nom-j1" pour pouvoir le renommer dynamiquement -->
        <div style="font-size:18px; font-weight:bold;" id="nom-j1">Joueur 1</div>
        <!-- Zone d'affichage des vies (cœurs) du joueur 1 : id="vies-j1" -->
        <div class="vies" id="vies-j1"></div>
        <!-- Temps de réaction du joueur 1 en millisecondes : id="t1" -->
        <div class="tps-box" id="t1">- ms</div>
      </div>
      
      <!-- Colonne du Joueur 2 : même logique que le Joueur 1, mais envoie
           la requête vers "/buzz?joueur=2" -->
      <div class="col" onclick="fetch('/buzz?joueur=2')">
        <div style="font-size:18px; font-weight:bold;" id="nom-j2">Joueur 2</div>
        <div class="vies" id="vies-j2"></div>
        <div class="tps-box" id="t2">- ms</div>
      </div>
      
      <!-- Colonne du Joueur 3 : même logique, requête vers "/buzz?joueur=3" -->
      <div class="col" onclick="fetch('/buzz?joueur=3')">
        <div style="font-size:18px; font-weight:bold;" id="nom-j3">Joueur 3</div>
        <div class="vies" id="vies-j3"></div>
        <div class="tps-box" id="t3">- ms</div>
      </div>
      
    </div>
    
    <!-- Zone centrale qui affiche le message d'état (ex: "En attente...",
         "Joueur X a gagné !") : id="vainqueur" pour la mise à jour dynamique -->
    <div class="box main-box" id="vainqueur">En attente d'une question...</div>
    
    <!-- Bloc contenant le bouton rouge "MAUVAISE RÉPONSE" -->
    <div class="box" style="padding:0; border-color: #ff3333; margin-top: 5px;">
      <!-- Bouton qui relance la manche en cas de mauvaise réponse : envoie
           une requête vers la route "/relancer" -->
      <button style="background: #ff3333; color: white; font-size: 18px; padding: 15px;" onclick="fetch('/relancer')">
        ❌ MAUVAISE RÉPONSE (Relancer)
      </button>
    </div>

    <!-- Ligne contenant les 2 boutons de navigation entre les questions -->
    <div class="row" style="margin-top: 5px;">
      <!-- Bloc du bouton "Précédente" (flex:1 = prend la moitié de la largeur) -->
      <div class="box" style="flex:1; padding:0;">
        <!-- Bouton qui revient à la question précédente : requête vers "/prev" -->
        <button class="nav-btn" onclick="fetch('/prev')">
          <span class="triangle">◀</span><span>Précédente</span>
        </button>
      </div>
      
      <!-- Bloc du bouton "Suivante" -->
      <div class="box" style="flex:1; padding:0;">
        <!-- Bouton qui passe à la question suivante : requête vers "/next" -->
        <button class="nav-btn" onclick="fetch('/next')">
          <span class="triangle">▶</span><span>Suivante</span>
        </button>
      </div>
    </div>

  </div>


  
  <script>
    /* --- LOGIQUE DE RAFRAÎCHISSEMENT AUTOMATIQUE (JavaScript) --- */
    // Cette fonction s'exécute en boucle toutes les 150 millisecondes pour mettre la page à jour
    setInterval(() => {
      // Demande au "Cerveau" ESP32 ses variables au format JSON
      fetch('/etat').then(r => r.json()).then(d => {
        
        // 1. Mise à jour des chronos des joueurs
        document.getElementById('t1').innerText = d.t1 > 0 ? d.t1 + " ms" : "- ms";
        document.getElementById('t2').innerText = d.t2 > 0 ? d.t2 + " ms" : "- ms";
        document.getElementById('t3').innerText = d.t3 > 0 ? d.t3 + " ms" : "- ms";
        
        // 2. Fonction interne pour redessiner l'interface de chaque joueur (Nom + Vies)
        function updatePlayerUI(num, lives, isBlockedForAns, isExcluded) {
          let blocked = (isBlockedForAns == num || isExcluded == 1);
          
          // Si le joueur est bloqué ou exclu, on lui met un logo interdit 🚫
          document.getElementById('nom-j'+num).innerText = blocked ? "Joueur " + num + " 🚫" : "Joueur " + num + (num==1?" (Toi)":"");
          
          // Choix des couleurs des emojis coeurs selon le numéro du joueur
          let symboleVie = "";
          if (num == 1)      symboleVie = "🔴"; // Rouge pour J1
          else if (num == 2) symboleVie = "🔵"; // Bleu pour J2
          else if (num == 3) symboleVie = "🟢"; // Vert pour J3

          // Si le joueur est bloqué, on cache ses vies temporairement, sinon on les répète (ex: 🔵🔵)
          document.getElementById('vies-j'+num).innerText = blocked ? "" : symboleVie.repeat(lives);
        }

        // Application de la mise à jour pour les 3 joueurs
        updatePlayerUI(1, d.v1, d.b, d.e1);
        updatePlayerUI(2, d.v2, d.b, d.e2);
        updatePlayerUI(3, d.v3, d.b, d.e3);

        // 3. Changement du grand texte central selon l'état du jeu (En attente, Préparez-vous, GO)
        let mainText = "En attente...";
        if (d.v > 0)          mainText = "👑 Le Joueur " + d.v + " a la main !";
        else if (d.etat == 1) mainText = "Préparez-vous...";
        else if (d.etat == 2) mainText = "GO ! BUZZEZ !";
        
        document.getElementById('vainqueur').innerText = mainText;
        
        // 4. Copie exacte du texte de l'écran LCD physique vers l'écran LCD de la page web
        document.getElementById('lcd-div').innerText = d.lcd0 + '\n' + d.lcd1;
      });
    }, 150); // Temps de rafraîchissement (150 millisecondes)
  </script>
</body>
</html>
)rawliteral"
  );
}

// ==================================================================================
// API ÉTAT (Convertit les variables Arduino en texte JSON lisible par le site web)
// ==================================================================================
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

// ==================================================================================
// LOGIQUE ET RÈGLES DU JEU (Actions déclenchées par les boutons de la page web)
// ==================================================================================

// Action : L'animateur clique sur "MAUVAISE RÉPONSE". Relance la même question en bloquant le joueur qui s'est trompé.
void handleRelancer() {
  if (etatQuiz == SHOW_SCORE) {
    playVolumeTone(150, 400); // Bip grave d'erreur
    joueurBloque = vainqueur; // Le joueur qui avait la main est maintenant bloqué
    vainqueur = 0; tempsJoueur[1] = 0; tempsJoueur[2] = 0; tempsJoueur[3] = 0;
    setLEDColor(false, false, false); // Éteint la LED de victoire
    
    doitDefiler = false; clearLine(1); printText("Relance...");
    etatQuiz = DELAY; 
    chronoQuiz = millis() + random(1500, 3000); // Relance un compte à rebours aléatoire de 1.5 à 3 secondes
    server.send(200, "text/plain", "OK");
  } else {
    server.send(200, "text/plain", "Attente");
  }
}

// Action : L'animateur lance la question
void handleStartQuiz() {
  if (etatQuiz != IDLE) return; // Ne fait rien si une question est déjà en cours
  tempsJoueur[1] = 0; tempsJoueur[2] = 0; tempsJoueur[3] = 0; 
  vainqueur = 0; joueurBloque = 0; // Réinitialise les blocages
  
  doitDefiler = false; clearLine(1); printText("Pret...");
  etatQuiz = DELAY; 
  chronoQuiz = millis() + random(2000, 5000); // Attente aléatoire entre 2 et 5 secondes avant le "GO !"
  server.send(200, "text/plain", "OK");
}

// Action : Passe à la question suivante
void handleNext() {
  numeroQuestion++; 
  etatQuiz = IDLE; vainqueur = 0; joueurBloque = 0;
  exclusQuestion[1] = false; exclusQuestion[2] = false; exclusQuestion[3] = false; // Redonne une chance aux exclus
  majLigneQuestion(); 
  clearLine(1); server.send(200, "text/plain", "OK");
}

// Action : Revient à la question précédente
void handlePrev() {
  if (numeroQuestion > 1) numeroQuestion--; 
  etatQuiz = IDLE; vainqueur = 0; joueurBloque = 0;
  exclusQuestion[1] = false; exclusQuestion[2] = false; exclusQuestion[3] = false;
  majLigneQuestion(); 
  clearLine(1); server.send(200, "text/plain", "OK");
}

// Action : Déclenchée lorsqu'un joueur appuie sur son bouton "BUZZ"
void handleBuzz() {
  int j = 2; // Joueur par défaut si bug
  if (server.hasArg("joueur")) j = server.arg("joueur").toInt(); // Récupère le numéro du joueur qui a cliqué
  
  // Sécurité : On vérifie si le joueur est bloqué (erreur au tour d'avant) ou totalement exclu (plus de vies)
  if (j == joueurBloque || exclusQuestion[j]) {
    server.send(200, "text/plain", "Bloque");
    return;
  }

  // CAS 1 : Le joueur buzze au bon moment (pendant le "GO !")
  if (etatQuiz == GO && vainqueur == 0) {
    tempsJoueur[j] = millis() - tempsDebutReaction; // Calcule le temps de réaction en millisecondes
    vainqueur = j; 
    etatQuiz = SHOW_SCORE; // Arrête le chrono, on affiche le score
    
    // Attribution des couleurs de la LED centrale selon le joueur qui gagne la main
    if (j == 1)      setLEDColor(false, false, true);  // Joueur 1 = LED Bleue
    else if (j == 2) setLEDColor(true, false, false); // Joueur 2 = LED Rouge
    else if (j == 3) setLEDColor(false, true, false); // Joueur 3 = LED Verte

    playVolumeTone(800, 600); // Bip aigu de victoire
    clearLine(1); 
    String score = "J" + String(j) + " GAGNE: " + String(tempsJoueur[j]) + "ms"; 
    printText(score.c_str()); // Affiche sur l'écran LCD
  } 
  // CAS 2 : Le joueur buzze TRICHE (Trop tôt, pendant le "Pret...")
  else if (etatQuiz == DELAY) {
    etatQuiz = IDLE; // Le quiz est annulé et repasse en attente
    setLEDColor(false, false, false); 
    playVolumeTone(150, 400); // Son de punition
    
    // Retrait d'une vie de triche
    if (viesTriche[j] > 0) {
      viesTriche[j]--; 
      messageActuel = "J" + String(j) + " TRICHE! Reste " + String(viesTriche[j]) + " vie(s)    "; 
    } else {
      exclusQuestion[j] = true; // Plus de vies ? Le joueur est banni pour cette question
      messageActuel = "J" + String(j) + " EXCLU POUR LA Q!       "; 
    }
    
    doitDefiler = true; // Lance l'animation de défilement du texte d'erreur sur l'écran LCD
    indexDefilement = 0;
  }
  server.send(200, "text/plain", "OK");
}

// ==================================================================================
// INITIALISATION (S'exécute une seule fois au démarrage de l'ESP32)
// ==================================================================================
void setup() {
  Serial.begin(115200); // Ouvre la communication avec l'ordinateur pour le débogage
  
  // Configure les broches de la LED RGB en mode Sortie (émet de la lumière)
  pinMode(pinRed, OUTPUT); pinMode(pinGreen, OUTPUT); pinMode(pinBlue, OUTPUT);
  setLEDColor(false, false, false); // Éteint la LED au démarrage
  
  // Démarre la communication I2C avec l'écran (Broche 21 = SDA, Broche 22 = SCL)
  Wire.begin(21, 22); delay(50);
  
  // Séquence technique d'initialisation de l'écran LCD (Configuration du mode 4-bits)
  sendCommand(0x33); delay(5);
  sendCommand(0x32); delay(5);
  sendCommand(0x28); delay(5); // Mode 2 lignes, polices 5x8
  sendCommand(0x0C); delay(5); // Allume l'écran, cache le curseur clignotant
  sendCommand(0x06); delay(5); // Mode d'entrée automatique du texte
  sendCommand(0x01); delay(10);// Efface tout l'écran au départ
  
  majLigneQuestion(); // Affiche la première ligne ("Question 1  C:0")
  
  // Démarre le point d'accès Wi-Fi de l'ESP32
  // Configuration : Nom, Mot de passe, Canal 1, Masquer le SSID (0=Non), Max 4 connexions simultanées
  WiFi.softAP(ssid, password, 1, 0, 4);
  
  // Associe les adresses internet tapées aux fonctions correspondantes de l'ESP32
  server.on("/", handleRoot); 
  server.on("/etat", handleEtat); 
  server.on("/relancer", handleRelancer);
  server.on("/start_quiz", handleStartQuiz); 
  server.on("/buzz", handleBuzz);
  server.on("/next", handleNext); 
  server.on("/prev", handlePrev); 
  
  server.begin(); // Démarre officiellement le serveur web
}

// ==================================================================================
// BOUCLE PRINCIPALE (S'exécute en boucle à l'infini, des milliers de fois par seconde)
// ==================================================================================
void loop() {
  server.handleClient(); // Traite les demandes de la page web en arrière-plan (Crucial !)
  
  // Vérifie si un nouveau joueur s'est connecté ou déconnecté du Wi-Fi
  int nbStations = WiFi.softAPgetStationNum();
  if (nbStations != dernierNbStations) {
    dernierNbStations = nbStations;
    majLigneQuestion(); // Rafraîchit instantanément l'affichage du compteur de joueurs
  }
  
  // --- GESTION DES ANIMATIONS DE LA LED DE STATUT ---
  
  // ÉTAT En attente (IDLE) : Fait tourner un effet arc-en-ciel fluide sur la LED RGB
  if (etatQuiz == IDLE) {
    if (millis() - chronoRGB >= vitesseRGB) {
      chronoRGB = millis(); 
      hueRGB++; // Change doucement de teinte
      int r, g, b;
      // Calcul mathématique pour créer la transition de couleurs
      if (hueRGB < 85) { r = 255 - hueRGB * 3; g = 0; b = hueRGB * 3; }
      else if (hueRGB < 170) { int t = hueRGB - 85; r = 0; g = t * 3; b = 255 - t * 3; }
      else { int t = hueRGB - 170; r = t * 3; g = 255 - t * 3; b = 0; }
      setLEDColorPWM(r, g, b); 
    }
  } 
  // ÉTAT Préparez-vous (DELAY) : Fait clignoter la LED rapidement en Blanc
  else if (etatQuiz == DELAY) {
    if (millis() - chronoLed >= vitesseLed) { 
      chronoLed = millis(); 
      etatLed = !etatLed; // Inverse l'état (allumé <-> éteint)
      setLEDColor(etatLed, etatLed, etatLed); 
    }
  } 
  // ÉTAT Buzzer actif (GO) : Éteint la LED centrale pour laisser les joueurs se concentrer
  else if (etatQuiz == GO) { 
    setLEDColor(false, false, false); 
  }
  
  // --- GESTION DU CHRONO DE JEU ---
  
  // Si le compte à rebours aléatoire est terminé, on passe à l'état "GO !!!!!!!"
  if (etatQuiz == DELAY && millis() >= chronoQuiz) { 
    etatQuiz = GO; 
    clearLine(1); 
    printText("GO !!!!!!!"); 
    tempsDebutReaction = millis(); // On capture l'instant précis du top départ
  }
  
  // --- GESTION DU TEXTE DÉFILANT SUR L'ÉCRAN LCD ---
  
  // Si un message de triche ou d'exclusion doit défiler (uniquement en mode attente)
  if (doitDefiler && etatQuiz == IDLE) {
    if (millis() - chronoDefilement > vitesseDefilement) {
      chronoDefilement = millis(); 
      // Découpe une portion de 16 caractères dans le texte complet pour créer l'effet de mouvement
      String affichage = messageActuel.substring(indexDefilement, indexDefilement + 16);
      setCursor(0, 1); 
      printText(affichage.c_str()); 
      indexDefilement = (indexDefilement + 1) % (messageActuel.length() - 15); // Recommence au début à la fin du texte
    }
  }
}