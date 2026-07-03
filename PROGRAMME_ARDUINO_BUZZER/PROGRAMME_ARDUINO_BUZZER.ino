#include <WiFi.h>
#include <HTTPClient.h>

// =================================================================
// 🛠️ CONFIGURATION DU JOUEUR
// Mets '2' pour ta Carte 2, et mets '3' pour ta Carte 3.
const int NUMERO_JOUEUR = 2; 
// =================================================================

// --- CONFIGURATION WI-FI ---
const char* ssid = "ESP32_SERVER";
const char* password = "12345678";

// --- CONFIGURATION MATÉRIELLE ---
const int pinInterrupteur = 4; // Le bouton poussoir sur la broche D4 et GND

// VOS DEUX LEDS DE COULEUR
const int pinLedCouleur1 = 22; 
const int pinLedCouleur2 = 25; 

// Mémoire du bouton
bool dernierEtat = HIGH;       

// Mémoire du temps pour l'interrogation du serveur
unsigned long chronoVerification = 0;

// --- FONCTION D'ENVOI DU BUZZ ---
void envoyerBuzz() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
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
  }
}

// --- FONCTION POUR VÉRIFIER SI ON EST BLOQUÉ ---
void verifierEtatCerveau() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://192.168.4.1/etat");
    http.setTimeout(200); // On met un délai très court pour ne pas bloquer le bouton
    
    int httpResponseCode = http.GET();
    
    if (httpResponseCode == 200) {
      String payload = http.getString();
      
      // On cherche dans le JSON si notre joueur est bloqué ("b":2) ou exclu ("e2":1)
      String strBloque = "\"b\":" + String(NUMERO_JOUEUR);
      String strExclu = "\"e" + String(NUMERO_JOUEUR) + "\":1";

      if (payload.indexOf(strBloque) >= 0 || payload.indexOf(strExclu) >= 0) {
        // 🔴 Le joueur est bloqué ou exclu -> On éteint tout !
        digitalWrite(pinLedCouleur1, LOW);
        digitalWrite(pinLedCouleur2, LOW);
      } else {
        // 🟢 Le joueur a le droit de jouer -> On maintient allumé !
        digitalWrite(pinLedCouleur1, HIGH);
        digitalWrite(pinLedCouleur2, HIGH);
      }
    }
    http.end();
  }
}

// --- DÉMARRAGE ---
void setup() {
  Serial.begin(115200);
  
  pinMode(pinInterrupteur, INPUT_PULLUP);
  pinMode(pinLedCouleur1, OUTPUT);
  pinMode(pinLedCouleur2, OUTPUT);
  
  // Allumage par défaut
  digitalWrite(pinLedCouleur1, HIGH);
  digitalWrite(pinLedCouleur2, HIGH);

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
  // 1. Auto-reconnexion
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(pinLedCouleur1, LOW); // Éteint si perte de co
    digitalWrite(pinLedCouleur2, LOW);
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }
    digitalWrite(pinLedCouleur1, HIGH); // Rallume quand reconnecté
    digitalWrite(pinLedCouleur2, HIGH);
  }

  // 2. Lecture du bouton poussoir
  bool etatLecture = digitalRead(pinInterrupteur);

  if (etatLecture != dernierEtat) {
    delay(50); // Filtre anti-rebond
    etatLecture = digitalRead(pinInterrupteur); 

    if (etatLecture == LOW && dernierEtat == HIGH) {
      envoyerBuzz();
      // On force une vérification de l'état juste après avoir buzzé
      verifierEtatCerveau(); 
    }
    dernierEtat = etatLecture;
  }

  // 3. Interrogation du Cerveau toutes les 1000 millisecondes (1 seconde)
  if (millis() - chronoVerification > 1000) {
    chronoVerification = millis();
    verifierEtatCerveau();
  }
}