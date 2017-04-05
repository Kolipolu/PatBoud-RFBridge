# PatBoud-RFBridge

*Pont MQTT vers RF pour Arduino ESP8266, qui reçoit un message MQTT contenant une commande, et envoie cette commande via RF*

*v1.1 - 2017-01-05*  
*Par: PatBoud*

##### Requiert les librairies suivantes
RCSwitch  
PubSubClient  
ArduinoJson  

##### Détails

Recoit un message MQTT sur topic rf/commandes/xxxxxx, et publie une confirmation sur topic rf/etat/xxxxxx .  
xxxxxx est défini par l'utilisateur et représente une switch en particulier.

Par exemple, le topic pourrait être: rf/commandes/woodsA

Le message doit être au format JSON. Voici des exemples de messages:

NOM DE SWITCH | MESSAGE MQTT  
--- | ---
WOODS A ON | {"Freq":315,"Code":12345,"Bits":15,"Prot":2,"PL":820}  
WOODS A OFF | {"Freq":315,"Code":54321,"Bits":15,"Prot":2,"PL":820}

PL est le PulseLength. Si on ne définit pas sa valeur, le PulseLength par défaut
pour le protocole choisi sera utilisé.


Voici un exemple où le PulseLength n'est pas défini:

NOM DE SWITCH | MESSAGE MQTT
--- | ---
NEXXTECH C 1 ON | {"Freq":433,"Code":35786772,"Bits":26,"Prot":1}  
NEXXTECH C 1 OFF | {"Freq":433,"Code":19009556,"Bits":26,"Prot":1}

---
Conçu pour s'utiliser parfaitement de concert avec Home-Assistant ( https://home-assistant.io/ ).

**Exemple de configuration de Switch dans Home-Assistant:**
```
switch:
  - platform: mqtt
    name: woods_a
    command_topic: "rf/commandes/woodsA"
    state_topic: "rf/etat/woodsA"
    payload_on: {"Freq":315,"Code":12345,"Bits":15,"Prot":2,"PL":820}
    payload_off: {"Freq":315,"Code":54321,"Bits":15,"Prot":2,"PL":820}
    retain: true
```
