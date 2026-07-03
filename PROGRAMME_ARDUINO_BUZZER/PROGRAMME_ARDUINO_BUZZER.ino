// ==================================================================================
// INCLUSION DES BIBLIOTHÈQUES (Les outils pour le Wi-Fi et les requêtes internet)
// ==================================================================================
#include <WiFi.h>       // Gère la connexion au réseau Wi-Fi du Cerveau
#include <HTTPClient.h> // Permet d'envoyer des commandes (comme un navigateur web miniature)

// ==================================================================================
// 🛠️ CONFIGURATION UNIQUE DU JOUEUR (À modifier pour chaque manette !)
// ==================================================================================
// IMPORTANT : Mettre '2' pour la carte du Joueur 2, '3' pour le Joueur 3, etc.
const int NUMERO_JOUEUR = 2; 

// ==================================================================================
// CONFIGURATION WI-FI (Doit être identique à celle du Cerveau)
// ==================================================================================
const char* ssid = "ESP32_SERVER";  // Nom du réseau Wi-Fi généré par le Cerveau
const char* password = "12345678";  // Mot de passe du Wi-Fi

// ==================================================================================
// CONFIGURATION MATÉRIELLE (Broches / Pins de la manette)
// ==================================================================================
const int pinInterrupteur = 4; // Broche du bouton poussoir (connecté entre D4 et le GND)

// Les deux LED de statut de la manette (s'allument si le joueur a le droit de jouer)
const int pinLedCouleur1 = 22; 
const int pinLedCouleur2 = 25; 

// ==================================================================================
// MEMOIRES ET CHRONOMÈTRES ARRIÈRE-PLAN
// ==================================================================================
bool dernierEtat = HIGH;             // Retient le dernier état du bouton (HIGH = relâché)
unsigned long chronoVerification = 0; // Chronomètre pour interroger le Cerveau à intervalles réguliers

// ==================================================================================
// FONCTIONS DE COMMUNICATION RESEAU
// ==================================================================================

// Rôle : Envoie instantanément un signal de "BUZZ" au Cerveau avec le numéro du joueur
void envoyerBuzz() {
  if (WiFi.status() == WL_CONNECTED) { // Vérifie qu'on est bien connecté au Wi-Fi
    HTTPClient http;
    
    // Construit l'adresse cible, ex: "http://192.168.4.1/buzz?joueur=2"
    String url = "http://192.168.4.1/buzz?joueur=" + String(NUMERO_JOUEUR);
    http.begin(url);
    
    int httpResponseCode = http.GET(); // Envoie la requête au serveur
    
    if (httpResponseCode > 0) {
      Serial.print("BUZZ envoyé pour le Joueur ");
      Serial.println(NUMERO_JOUEUR);
    } else {
      Serial.println("Erreur de transmission réseau.");
    }
    http.end(); // Libère la mémoire de la requête
  }
}

// Rôle : Demande au Cerveau si le joueur est bloqué (erreur) ou exclu (plus de vies)
void verifierEtatCerveau() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://192.168.4.1/etat");
    http.setTimeout(200); // Limite l'attente à 200ms maximum pour ne pas ralentir le bouton
    
    int httpResponseCode = http.GET(); // Récupère le texte de l'état général du jeu (JSON)
    
    if (httpResponseCode == 200) {
      String payload = http.getString(); // Contient les données envoyées par le Cerveau
      
      // On prépare les morceaux de texte textuels à rechercher dans la réponse
      String strBloque = "\"b\":" + String(NUMERO_JOUEUR);       // ex: cherche "b":2 (joueur 2 bloqué)
      String strExclu = "\"e" + String(NUMERO_JOUEUR) + "\":1";  // ex: cherche "e2":1 (joueur 2 exclu)

      // indexOf cherche si le texte est présent. Si oui, résultat >= 0
      if (payload.indexOf(strBloque) >= 0 || payload.indexOf(strExclu) >= 0) {
        // 🔴 Le joueur est bloqué ou exclu -> On éteint les LED de la manette !
        digitalWrite(pinLedCouleur1, LOW);
        digitalWrite(pinLedCouleur2, LOW);
      } else {
        // 🟢 Le joueur a le droit de jouer -> On maintient les LED allumées !
        digitalWrite(pinLedCouleur1, HIGH);
        digitalWrite(pinLedCouleur2, HIGH);
      }
    }
    http.end();
  }
}

// ==================================================================================
// INITIALISATION (S'exécute une seule fois au démarrage de la manette)
// ==================================================================================
void setup() {
  Serial.begin(115200); // Ouvre le port de débogage pour l'ordinateur
  
  // Configure le bouton avec la résistance interne de l'ESP32 (Évite les interférences)
  pinMode(pinInterrupteur, INPUT_PULLUP);
  
  // Configure les broches des LED en mode Sortie
  pinMode(pinLedCouleur1, OUTPUT);
  pinMode(pinLedCouleur2, OUTPUT);
  
  // Allumage initial pour montrer que la carte démarre
  digitalWrite(pinLedCouleur1, HIGH);
  digitalWrite(pinLedCouleur2, HIGH);

  // Connexion au réseau Wi-Fi du Cerveau
  WiFi.begin(ssid, password);
  Serial.print("Joueur ");
  Serial.print(NUMERO_JOUEUR);
  Serial.print(" recherche le Cerveau...");
  
  // Boucle bloquante tant que la manette n'a pas trouvé le réseau du Cerveau
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnecté et prêt à buzzer !");
}

// ==================================================================================
// BOUCLE PRINCIPALE (S'exécute en boucle à l'infini)
// ==================================================================================
void loop() {
  
  // --- 1. SÉCURITÉ ET AUTO-RECONNEXION ---
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(pinLedCouleur1, LOW); // Éteint les LED en cas de perte de connexion
    digitalWrite(pinLedCouleur2, LOW);
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    
    // Patiente jusqu'à retrouver le signal
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }
    
    digitalWrite(pinLedCouleur1, HIGH); // Rallume tout une fois reconnecté
    digitalWrite(pinLedCouleur2, HIGH);
  }

  // --- 2. LECTURE DU BOUTON POUSSOIR ---
  bool etatLecture = digitalRead(pinInterrupteur);

  // Détection d'un changement de position du bouton
  if (etatLecture != dernierEtat) {
    delay(50); // Filtre anti-rebond (attend que le signal électrique se stabilise)
    etatLecture = digitalRead(pinInterrupteur); // Relit pour confirmation

    // Si le bouton vient d'être enfoncé (Passe de HIGH à LOW)
    if (etatLecture == LOW && dernierEtat == HIGH) {
      envoyerBuzz(); // Envoie le signal de buzz au cerveau immédiatement
      
      // Force une vérification immédiate de l'état pour savoir si on vient d'être bloqué
      verifierEtatCerveau(); 
    }
    dernierEtat = etatLecture; // Enregistre la position actuelle pour la prochaine boucle
  }

  // --- 3. INTERROGATION RÉGULIÈRE DU CERVEAU ---
  // Toutes les 1000 millisecondes (1 seconde), la manette demande au serveur si le statut a changé
  if (millis() - chronoVerification > 1000) {
    chronoVerification = millis(); // Relance le chronomètre
    verifierEtatCerveau();         // Met à jour l'allumage ou l'extinction des LED de la manette
  }
}