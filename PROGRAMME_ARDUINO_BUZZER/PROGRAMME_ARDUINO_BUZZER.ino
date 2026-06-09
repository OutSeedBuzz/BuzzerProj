#include <WiFi.h>
#include <HTTPClient.h>

// --- CONFIGURATION WI-FI ---
const char* ssid = "ESP32_SERVER2";
const char* password = "12345678";

// --- CONFIGURATION MATÉRIELLE ---
const int pinInterrupteur = 4; // L'interrupteur est branché sur la broche D4 et GND
bool dernierEtat = HIGH;       // Stocke la position précédente

void setup() {
  Serial.begin(115200);
  
  // Initialise la broche avec la résistance interne de l'ESP32
  pinMode(pinInterrupteur, INPUT_PULLUP);

  // Lancement de la connexion Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Recherche du Serveur ESP32...");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnecté au Cerveau avec succès !");
}

// --- FONCTION D'ENVOI DES MESSAGES ---
void envoyerOrdre(String route) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    String url = "http://192.168.4.1" + route;
    http.begin(url);
    
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      Serial.print("Message transmis : ");
      Serial.println(route);
    } else {
      Serial.print("Erreur de transmission. Code HTTP : ");
      Serial.println(httpResponseCode);
    }
    
    http.end(); 
  } else {
    Serial.println("Erreur : Wi-Fi déconnecté !");
  }
}

// --- BOUCLE PRINCIPALE ---
void loop() {
  bool etatLecture = digitalRead(pinInterrupteur);

  if (etatLecture != dernierEtat) {
    
    delay(100); // Filtre anti-rebond matériel
    
    etatLecture = digitalRead(pinInterrupteur); 

    if (etatLecture != dernierEtat) {
      
      // LA MODIFICATION EST ICI :
      if (etatLecture == LOW) {
        // L'état est LOW (interrupteur fermé, relié au GND)
        Serial.println("Interrupteur sur [ LOW ] - Envoi de : aurevoir");
        envoyerOrdre("/message?texte=Au_revoir"); 
      } 
      else {
        // L'état est HIGH (interrupteur ouvert)
        Serial.println("Interrupteur sur [ HIGH ] - Envoi de : bonjour");
        envoyerOrdre("/message?texte=Bonjour"); 
      }
      
      dernierEtat = etatLecture;
    }
  }
}