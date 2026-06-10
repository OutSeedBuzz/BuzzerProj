#include <WiFi.h>
#include <HTTPClient.h>

// =================================================================
// 🛠️ CONFIGURATION DU JOUEUR (Modifie juste ce chiffre !)
// Mets '2' pour ta Carte 2, et mets '3' pour ta Carte 3.
const int NUMERO_JOUEUR = 3; 
// =================================================================

// --- CONFIGURATION WI-FI ---
const char* ssid = "ESP32_SERVER";
const char* password = "12345678";

// --- CONFIGURATION MATÉRIELLE ---
const int pinInterrupteur = 4; // L'interrupteur sur la broche D4 et GND
const int pinLed = 2;          // La petite LED bleue intégrée
bool dernierEtat = HIGH;       

// --- FONCTION D'ENVOI DU BUZZ ---
void envoyerBuzz() {
  if (WiFi.status() == WL_CONNECTED) {
    
    // On allume la LED pour confirmer l'action visuellement
    digitalWrite(pinLed, HIGH); 
    
    HTTPClient http;
    // L'URL s'adapte automatiquement au numéro du joueur !
    String url = "http://192.168.4.1/buzz?joueur=" + String(NUMERO_JOUEUR);
    http.begin(url);
    
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      Serial.print("BUZZ envoyé pour le Joueur ");
      Serial.println(NUMERO_JOUEUR);
    } else {
      Serial.println("Erreur de transmission réseau.");
    }
    
    http.end(); 
    
    // On éteint la LED une fois le message parti
    digitalWrite(pinLed, LOW); 
  } else {
    Serial.println("Erreur : Impossible de buzzer, Wi-Fi déconnecté !");
  }
}

// --- DÉMARRAGE ---
void setup() {
  Serial.begin(115200);
  
  pinMode(pinInterrupteur, INPUT_PULLUP);
  pinMode(pinLed, OUTPUT);
  digitalWrite(pinLed, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Joueur ");
  Serial.print(NUMERO_JOUEUR);
  Serial.print(" recherche le Cerveau...");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnecté et prêt à buzzer !");
}

// --- BOUCLE PRINCIPALE ---
void loop() {
  // 1. Auto-reconnexion magique en cas de coupure du Cerveau
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Signal perdu... Tentative de reconnexion !");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\nReconnecté !");
  }

  // 2. Lecture de l'interrupteur
  bool etatLecture = digitalRead(pinInterrupteur);

  // 3. Détection de mouvement avec filtre anti-rebond
  if (etatLecture != dernierEtat) {
    delay(100); // Pause mécanique
    etatLecture = digitalRead(pinInterrupteur); 

    if (etatLecture != dernierEtat) {
      // Que l'interrupteur bascule vers le HAUT ou vers le BAS, c'est un BUZZ !
      envoyerBuzz();
      
      // On mémorise la position
      dernierEtat = etatLecture;
    }
  }
}