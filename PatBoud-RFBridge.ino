/*
 * PatBoud-RFBridge
 * v1.1
 * 2017-01-05
 * 
 * Par: PatBoud
 * 
 * Recoit un message MQTT sur topic rf/commandes/xxxxxx, et publie une confirmation sur topic rf/etat/xxxxxx .
 * xxxxxx est défini par l'utilisateur et représente une switch en particulier.
 * 
 * Par exemple: rf/commandes/woodsA
 * 
 * Le message doit être au format JSON. Voici des exemples de messages:
 * 
 * NOM DE SWITCH    :  MESSAGE MQTT
 * 
 * WOODS A ON : {"Freq":315,"Code":12345,"Bits":15,"Prot":2,"PL":820}
 * WOODS A OFF : {"Freq":315,"Code":54321,"Bits":15,"Prot":2,"PL":820}
 * 
 * PL est le PulseLength. Si on ne définit pas sa valeur, le PulseLength par défaut
 * pour le protocole choisi sera utilisé.
 * 
 * Voici un exemple où le PulseLength n'est pas défini:
 * 
 * NOM DE SWITCH    :  MESSAGE MQTT
 * 
 * NEXXTECH C 1 ON : {"Freq":433,"Code":35786772,"Bits":26,"Prot":1}
 * NEXXTECH C 1 OFF : {"Freq":433,"Code":19009556,"Bits":26,"Prot":1}
 * 
 * 
 * Conçu pour s'utiliser parfaitement de concert avec Home-Assistant ( https://home-assistant.io/ ).
 * 
 * Exemple de configuration de Switch dans Home-Assistant:
 * 
 * switch:
 *   - platform: mqtt
 *     name: woods_a
 *     command_topic: "rf/commandes/woodsA"
 *     state_topic: "rf/etat/woodsA"
 *     payload_on: {"Freq":315,"Code":12345,"Bits":15,"Prot":2,"PL":820}
 *     payload_off: {"Freq":315,"Code":54321,"Bits":15,"Prot":2,"PL":820}
 *     retain: true
 * 
 * 
 */
 
#include <RCSwitch.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// DÉBUT DES PARAMÈTRES À MODIFIER

// Numéros de pin auxquelles les transmetteurs RF sont branchés
#define pin_433 5
#define pin_315 14

// Paramètres de WiFi
#define wifi_ssid "xxxxxxxxxxxxx"
#define wifi_password "xxxxxxxxxxxxx"

// Paramètres MQTT
#define mqtt_server "xxx.xxx.xxx.xxx"
#define mqtt_port 1883
#define mqtt_clientID "ESP8266_RF"
#define mqtt_user "xxxxxxxxxxxxx"
#define mqtt_pass "xxxxxxxxxxxxx"

#define mqtt_inTopic "rf/commandes/#"
#define mqtt_outTopicBase "rf/etat/"

// FIN DES PARAMÈTRES À MODIFIER

WiFiClient espClient;
PubSubClient client(espClient);

RCSwitch mySwitch433 = RCSwitch();
RCSwitch mySwitch315 = RCSwitch();

void setup()
{
  pinMode(0, OUTPUT); // Led rouge intégrée au Adafruit ESP8266 Huzzah.

  // Led Rouge ON. Restera allumée jusqu'a ce que nous soyons connecté au serveur MQTT.
  // Donc, si elle reste allumée, il y a un problème de connexion quelconque à vérifier.
  digitalWrite(0, LOW);
    
  Serial.begin(9600);
  
  // Transmetteur 433 Mhz
  mySwitch433.enableTransmit(pin_433);
  mySwitch433.setRepeatTransmit(15);

  // Transmetteur 315 Mhz
  mySwitch315.enableTransmit(pin_315);
  mySwitch315.setRepeatTransmit(15);


  // Initialisation WiFi et MQTT
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}


// *******************
// INITIALISATION WIFI
// *******************
void setup_wifi() {

  delay(10);
  // Connexion WiFi
  Serial.println();
  Serial.print("Connexion a ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connecte");
  Serial.println("IP: ");
  Serial.println(WiFi.localIP());
}


// ***************************
// RÉCEPTION D'UN MESSAGE MQTT
// ***************************
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Nouveau message [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.print("");

  // Led Rouge ON. Allumée tout au long du traitement du message,
  // de l'envoi du code RF et de la publication du message MQTT de confirmation.
  digitalWrite(0, LOW);

   // Conversion du payload en char array
  char c_payload[length + 1];
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    c_payload[i] = (char)payload[i];
  }
  // Ajout du caractère de fin de chaine en derniere position. IMPORTANT pour éviter bugs imprévisibles!
  c_payload[length] = '\0';

  Serial.println("");

  // Copie du message original pour l'utiliser plus tard comme message d'état, confirmant que la
  // commande a été envoyée. Le message d'état original ne reste pas indéfiniement en mémoire.
  // Il faut le copier maintenant.
  char c_etat[sizeof(c_payload)];
  strcpy(c_etat, c_payload);

  // Création de l'objet JSON
  StaticJsonBuffer<120> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(c_payload);

  if (!root.success()) {
    Serial.println("ERREUR JSON: parseObject() failed");
    return;
  }

  // Chargement des valeurs de l'objet JSON dans des variables utilisees pour l'envoi de la commande RF
  // Exemple:  {"Freq":433, "Code":1234566, "Bits":24, "Prot":1, "PL":840}
  int rfFreq = root["Freq"];
  long rfCode = root["Code"];
  int rfBits = root["Bits"];
  int rfProt = root["Prot"];

  // Gestion du PulseLength. Si il n'est pas défini dans le message MQTT, je le définis à 0 maintenant.
  // Il sera ignoré au moment d'envoyer le signal RF.
  int rfPL = 0;
  if (root.containsKey("PL")) {
    rfPL = root["PL"];
  }

  // Decodage du topic pour identifier le nom de la prise pour publier dans le bon topic d'etat
  // Le topic de publication attendu est:
  // rf/commandes/xxxxxx
  char c_nomPrise[24];
  int longueurTopic = strlen(topic);  
  int dernierSlash = 0;

  // Étape 1: On localise le dernier slash dans le topic de commandes pour connaitre
  // à partir de quelle position on commence à lire le nom de la switch
  for (int i=0;i<longueurTopic;i++) {
    if (topic[i] == '/') {
      dernierSlash = i;
    }
  }

  // Étape 2: On lit le nom de la switch de la position du dernier slash + 1, jusqu'à la fin
  int x = 0;
  for (int i=dernierSlash+1;i<longueurTopic;i++) {
    c_nomPrise[x] = topic[i];
    x++;
  }
  // Important: Ajout du caractère de fin de chaine
  c_nomPrise[x] = '\0';
 

  // Envoi des messages RF en fonction des paramètres reçus via MQTT

  switch (rfFreq) {
    case 315:
      {
        if (rfPL == 0) {
            mySwitch315.setProtocol(rfProt);
        }
        else {
          mySwitch315.setProtocol(rfProt, rfPL);
        }
        mySwitch315.send(rfCode, rfBits);
        break;
      }

    case 433:
      {
        if (rfPL == 0) {
          mySwitch433.setProtocol(rfProt);
        }
        else {
          mySwitch433.setProtocol(rfProt, rfPL);
        }
        
        mySwitch433.send(rfCode, rfBits);
        break;
      }
 
  }

  // Topic final utilisé pour publier l'état de la switch. Sera créé à partir du
  // mqtt_outTopicBase et du nom de la switch xxxxxx utilisé dans le topic de commandes.
  // Longueur maximale de 50 caractères incluant le topic de Base. Exemple: "rf/etat/xxxxxxxxxxxxxxxx"

  char c_mqtt_outTopic[50];
  
  strcpy(c_mqtt_outTopic, mqtt_outTopicBase);
  strcat(c_mqtt_outTopic, c_nomPrise);

  // On publie MQTT sur le topic d'etat pour la switch. Le message, défini précédemment, est simplement une copie identique du message reçu sur
  // le topic de commandes. Ceci confirme à Home-Assistant que le signal RF a bien été envoyé.  
  client.publish(c_mqtt_outTopic, c_etat);

  //Led Rouge OFF
  digitalWrite(0, HIGH);
}


// ************** 
// CONNEXION MQTT 
// ************** 
void reconnect() {
  // Boucle jusqu'à ce qu'on soit connecté
  while (!client.connected()) {
    Serial.print("Connexion au serveur MQTT ... ");
    // Tentative de connexion
    if (client.connect(mqtt_clientID, mqtt_user, mqtt_pass)) {
      Serial.println("connecte!");
      // ... et de resubscribe
      client.subscribe(mqtt_inTopic);
      
      //Led Rouge OFF, tout est OK
      digitalWrite(0, HIGH);
    }
    else {

      //Led Rouge ON, erreur
      digitalWrite(0, LOW);
      
      Serial.print("ERREUR, rc=");
      Serial.print(client.state());
      Serial.println(" Nouvel essai dans 5 secondes");
      // Attente de 5 secondes avant de réessayer
      delay(5000);
    }
  }
}


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}


