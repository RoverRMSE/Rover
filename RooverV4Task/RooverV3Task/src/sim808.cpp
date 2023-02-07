#include <sim808.h>
SoftwareSerial mySerial(1, 3); //SIM808 Tx & Rx sont connéctés à ESP32 #1 & #3
//la fonction d'affichage des echanges de messages sur le port série

void updateSerial()
{
  delay(500);
  while (Serial.available()) 
  {
    mySerial.write(Serial.read());//Forward what Serial received to Software Serial Port
  }
  while(mySerial.available()) 
  {
    Serial.write(mySerial.read());//Forward what Software Serial received to Serial Port
  }
  
}

void init_sim808(){
    //initier une communication série entre ESP32 et SIM808
    mySerial.begin(9600);

   
//introduction des commandes AT pour la configuration da la SIM808
    mySerial.println("AT"); //voir si la SIM808 est prête
    
    updateSerial();
    mySerial.println("AT+CSQ");// voir la qualité du réseau
    updateSerial();
    
    mySerial.println("AT+CMGF=1");// Configurer le mode text
    updateSerial();
    
    mySerial.println("AT+CNMI=3,2,0,1,1"); // la manière de traiter les nouveaux messages reçus
    
    updateSerial();
}