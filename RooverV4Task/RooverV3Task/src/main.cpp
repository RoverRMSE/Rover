#include "main.h"
#include "sim808.h"

TaskHandle_t DistanceHandle;
TaskHandle_t EventHandle;
TaskHandle_t ForwardHandle;
TaskHandle_t ServoHandle;


/******   Declaration de la Queue  *****/
QueueHandle_t DistanceQueue;

/******* Declaration du Groupe Event to handle the actions *******/
EventGroupHandle_t ActionsEvent;

/***  Declaration des semaphores pour la synchronisation de l'ajout et le prelevement de la file ****/
SemaphoreHandle_t semaphore;
SemaphoreHandle_t semaphore_2;
SemaphoreHandle_t mutexControlQueue;
SemaphoreHandle_t mutexF;
SemaphoreHandle_t mutexS;

void handleDistance(void *pvParameters){
  while(1){
    //xSemaphoreTake(mutexControlQueue,portMAX_DELAY);
    Serial.println("I am in handleDistance!");
    double distanceToSet = getDistance();
    Serial.println(distanceToSet);
    // xQueueSend permet d'ajouter un element dans la queue de la file
    // DistanceQueue: la file ,&distanceToSet: pointeur vers la valeur a inserer,portMAX_DELAY: delay d'attent si la file est pleine
    xQueueSend(DistanceQueue,&distanceToSet,portMAX_DELAY);
    xSemaphoreGive(semaphore_2); 
   vTaskDelay(200/portTICK_PERIOD_MS); // save distance in queue every 200 ms
  }
}

void handleEvent(void *pvParameters){
  while(1){
    xSemaphoreTake(semaphore_2,portMAX_DELAY);
    Serial.println("I am in handleEvent!");
    double distanceToGet;
    // recuperer la distance de la queue de la file
    if(xQueueReceive(DistanceQueue,&distanceToGet,portMAX_DELAY)==pdTRUE){
      if (distanceToGet >= 80){
        // set the bit Forward
        xEventGroupSetBits(ActionsEvent,FRWARD_EVENT);
        Serial.print("The forward bit is set ");
      }else{
        Serial.print("The Stop bit is set ");
        xEventGroupSetBits(ActionsEvent,STOP_EVENT);
      }
    }else{
      Serial.print("Erreur l\'ors de la reception de la distance !");
    }
  }
}

void TaskForward(void *pvParameters)
{
  while(1){ 
    xEventGroupWaitBits(ActionsEvent,FRWARD_EVENT,pdTRUE,pdTRUE,portMAX_DELAY);
    goForward();
  }
}
/*
void control(void *pvParameters){
  while(1){
    Serial.print(" hi hi hi hi Moving with speed = ");
    analogWrite(enable1Pin,245);
    analogWrite(enable2Pin,245);
  } 
}
*/

void TaskBack(void *pvParameters){
  while(1){
    xEventGroupWaitBits(ActionsEvent,BACK_EVENT,pdTRUE,pdTRUE,portMAX_DELAY);
    Serial.println("Moving back");
    goBack();
    vTaskDelay(200/portTICK_PERIOD_MS);
    vTaskResume(DistanceHandle);
    vTaskDelay(200/portTICK_PERIOD_MS);
    vTaskResume(EventHandle);
  }
}


void Taskstop(void *pvParameters){
  while(1){
    xEventGroupWaitBits(ActionsEvent,STOP_EVENT,pdTRUE,pdTRUE,portMAX_DELAY);
    /** On doit suspendu les deux taches pour donner la main au servo de choisir la direction right or left or back **/
    vTaskSuspend(EventHandle);
    vTaskSuspend(DistanceHandle);

    Serial.println("I am in Stop!");
    stop();
    xSemaphoreGive(semaphore); // donner le tiquer pour le serveau moteur
    delay(1000);
  }
}


void TaskServo(void *pvParameters){
  double distanceRight,distanceLeft;
  while(1){
    xSemaphoreTake(semaphore,portMAX_DELAY);
    Serial.print("I am in serveau task !!!! ");
    distanceRight =lookAround(Degree_90_Right);
    distanceLeft =lookAround(Degree_90_Left);
    Serial.print("distance Gauche = ");
    Serial.println(distanceLeft);
     Serial.print("distance Droite = ");
    Serial.println(distanceRight);
    if((distanceLeft > 80)||(distanceRight > 80)){
      if ( distanceLeft >= distanceRight ){
        xEventGroupSetBits(ActionsEvent,LEFT_EVENT);
      }else{
        xEventGroupSetBits(ActionsEvent,RIGHT_EVENT);
      }
    }else{
      xEventGroupSetBits(ActionsEvent,BACK_EVENT);
    }
  }
}

void TaskLeft(void *pvParameters){
  while(1){
   xEventGroupWaitBits(ActionsEvent,LEFT_EVENT,pdTRUE,pdTRUE,portMAX_DELAY);
   goLeft();
   vTaskDelay(200/portTICK_PERIOD_MS);
   vTaskResume(DistanceHandle);
   vTaskDelay(200/portTICK_PERIOD_MS);
   vTaskResume(EventHandle);
  }
}

void TaskRight (void *pvParameters){
  while(1){
    xEventGroupWaitBits(ActionsEvent,RIGHT_EVENT,pdTRUE,pdTRUE,portMAX_DELAY);
    goRight(); 
    vTaskDelay(200/portTICK_PERIOD_MS);
    vTaskResume(DistanceHandle);
    vTaskDelay(200/portTICK_PERIOD_MS);
    vTaskResume(EventHandle);
  }
}

//la tache de la réception de message et de détection de la commande (allumage/extinction/etat)
void TaskSms(void *pvParameters)
{
  //create_serie_object(1, 3);
  SoftwareSerial mySerial(1, 3); //SIM808 Tx & Rx sont connéctés à ESP32 #1 & #3
  String message;
  
  while(1){
    if (mySerial.available() > 0) { // si  des données  sont disponibles sur le port série
        // lit l'octet entrant
        message = mySerial.readString();
        // renvoie l'octet reçu
        Serial.println( message);
        Serial.print('je suis dans SMS');
        //commande d'extinction
        if(message.indexOf("retour")>=0 || message.indexOf("retour")>=0 || message.indexOf("demi tour")>=0){
            xEventGroupSetBits(ActionsEvent,ORDER_BACK_EVENT);
            mySerial.println("AT+CMGS=\"213541646128\"");//commande d'envoie de message
            updateSerial();
            mySerial.println("la lampe est éteinte");
            mySerial.println((char)26); //ctrl Z   
            delay(5000);  
        }   
    }  
  }
}

// la tache qui va s'executer apres la reception du message:
void TaskBackAfterMessage(void *pvParameters){
  while(1){
    xEventGroupWaitBits(ActionsEvent,LEFT_EVENT,pdTRUE,pdTRUE,portMAX_DELAY);
    Serial.print('je suis dans TaskBackAfterMessage');
    vTaskSuspend(EventHandle);
    vTaskSuspend(DistanceHandle);
    vTaskSuspend(ForwardHandle);
    xEventGroupSetBits(ActionsEvent,BACK_EVENT); // set the bit who activate 
    vTaskDelay(1000/portTICK_PERIOD_MS);
    vTaskResume(ForwardHandle);
  }
}


void setup() {
  Serial.begin(9600);
  // creation de la queue: 
  DistanceQueue=xQueueCreate(10,sizeof(double));   // 10 est la taille de la file, sizeof(double) est la taille de l'element

  // creation de l'event Group:
  ActionsEvent = xEventGroupCreate();

  // creation des semaphore et mutex
  semaphore = xSemaphoreCreateBinary();// syncronize stop with servo
  semaphore_2 = xSemaphoreCreateBinary(); // syncronize handleD with handleE tq : handleD ---> handleE
  mutexF = xSemaphoreCreateMutex();
  mutexS = xSemaphoreCreateMutex();
  mutexControlQueue = xSemaphoreCreateMutex();


  // initialiser les pins du radar 
  initPin();

  //speed control:
  //speed_motor();

  // initialiser les pins des motors
  initPinsMotors();

  // initialiser les pins du servo 
  servoInit();

  // initialiser la configuration de la sim8
  //initsim808();

  // for test:
  pinMode(2, OUTPUT);

  //initialiser une communication en serie
  Serial.println("Testing DC Motor...");

  xTaskCreate(handleDistance,"handle distance",1024,NULL,1,&DistanceHandle);
  xTaskCreate(handleEvent,"handle Event",1024,NULL,1,&EventHandle);
  xTaskCreate(TaskForward,"forward",1024,NULL,1,&ForwardHandle);
  xTaskCreate(Taskstop,"stop",1024,NULL,1,NULL);
  xTaskCreate(TaskServo,"servo",1024,NULL,1,NULL);
  xTaskCreate(TaskBack,"Back",1024,NULL,1,NULL);
  xTaskCreate(TaskLeft,"Left",1024,NULL,1,NULL);
  xTaskCreate(TaskRight,"Right",1024,NULL,1,NULL);
  xTaskCreatePinnedToCore(TaskSms, "sms", 6144,NULL, 1, NULL,1);

}

void loop() {
}