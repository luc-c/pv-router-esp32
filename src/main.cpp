#include <Arduino.h>


#include "WiFiClientSecure.h"

  #include <driver/adc.h>
  #include "config/config.h"
  #include "config/enums.h"
  #include "config/traduction.h"
  #ifdef S3
  #include "pin_config.h"
  #endif

  #if  NTP
  #include <NTPClient.h>
  #include "tasks/fetch-time-from-ntp.h"
  #endif

#include <AsyncElegantOTA.h>
//#include <WebSerial.h>

// File System
#include <FS.h>
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include <ArduinoJson.h> // ArduinoJson : https://github.com/bblanchon/ArduinoJson

  #include "tasks/updateDisplay.h"
  #include "tasks/switchDisplay.h"
 
  //#include "tasks/mqtt-aws.h"
  #include "tasks/wifi-connection.h"
  //#include "tasks/wifi-update-signalstrength.h"
  #include "tasks/measure-electricity.h"
  //#include "tasks/mqtt-home-assistant.h"
  #include "tasks/Dimmer.h"
  #include "tasks/gettemp.h"

  #include "tasks/Serial_task.h"

  //#include "functions/otaFunctions.h"
  #include "functions/spiffsFunctions.h"
  #include "functions/Mqtt_http_Functions.h"
  #include "functions/webFunctions.h"

  #include "functions/froniusFunction.h"
  #include "functions/enphaseFunction.h"
  #include "functions/WifiFunctions.h"
  #include "functions/homeassistant.h"

  #include "functions/minuteur.h"

  #include "uptime.h"
  #include <driver/adc.h>
#if DALLAS
// Dallas 18b20
  #include <OneWire.h>
  #include <DallasTemperature.h>
  #include "tasks/dallas.h"
  #include "functions/dallasFunction.h"
#endif

#if DIMMERLOCAL 
#include <RBDdimmer.h>   /// the corrected librairy  in RBDDimmer-master-corrected.rar , the original has a bug
#include "functions/dimmerFunction.h"
#endif



//***********************************
//************* Afficheur Oled
//***********************************
#ifdef  DEVKIT1
// Oled
#include "SSD1306Wire.h" /// Oled ( https://github.com/ThingPulse/esp8266-oled-ssd1306 ) 
const int I2C_DISPLAY_ADDRESS = 0x3c;
SSD1306Wire  display(0x3c, SDA, SCL); // pin 21 SDA - 22 SCL
#endif

#ifdef  TTGO
#include <TFT_eSPI.h>
#include <SPI.h>
TFT_eSPI display = TFT_eSPI();   // Invoke library
#endif


DisplayValues gDisplayValues;
//EnergyMonitor emon1;
Config config; 
Configwifi configwifi; 
Configmodule configmodule; 

///déclaration des programmateurs 
Programme programme; 

/// declare logs 
Logs logging;
/// declare MQTT 
Mqtt configmqtt;


int retry_wifi = 0;
void connect_to_wifi();
void handler_before_reset();
void reboot_after_lost_wifi(int timeafterlost);
void IRAM_ATTR function_off_screen();
void IRAM_ATTR function_next_screen();

#if  NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER, NTP_OFFSET_SECONDS, NTP_UPDATE_INTERVAL_MS);
#endif

// Place to store local measurements before sending them off to AWS
unsigned short measurements[LOCAL_MEASUREMENTS];
unsigned char measureIndex = 0;

//***********************************
//************* Dallas
//***********************************
#if DALLAS
Dallas dallas; 
#endif

//String loguptime();
char *loguptime2();
#ifndef LIGHT_FIRMWARE
    HA device_dimmer; 
    HA device_routeur; 
    HA device_grid;
    HA device_inject;
    HA compteur_inject;
    HA compteur_grid;
    HA switch_1;
    HA temperature_HA;
    HA power_factor;
    HA power_vrms;
    HA power_irms;
    HA power_apparent;
    HA enphase_cons_whLifetime;
    HA enphase_prod_whLifetime;
    HA enphase_current_power_consumption;
    HA enphase_current_power_production;
    HA surplus_routeur;
    HA device_state; 
#endif

/***************************
 *  Dimmer init
 **************************/
#if DIMMERLOCAL
dimmerLamp dimmer_hard(outputPin, zerocross); //initialase port for dimmer for ESP8266, ESP32, Arduino due boards
#endif

void setup()
{
  #if DEBUG == true
    Serial.begin(115200);
  #endif 
  #if CORE_DEBUG_LEVEL > ARDUHAL_LOG_LEVEL_NONE
    Serial.setDebugOutput(true);
  #endif
  Serial.println("\n================== " + String(VERSION) + " ==================");
  strcat(logging.log_init,"197}11}1");
  strcat(logging.log_init,"#################  Restart reason  ###############\r\n");
  esp_reset_reason_t reason = esp_reset_reason();
  strcat(logging.log_init,String(reason).c_str());
  strcat(logging.log_init,"\r\n#################  Starting System  ###############\r\n");
    
  //démarrage file system
  Serial.println("start SPIFFS");
  strcat(logging.log_init,loguptime2());
  strcat(logging.log_init,"Start Filesystem\r\n");
  
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Initialization failed!");
    return;
  }
  //Serial.println(logging.log_init);
  /// Program & FS size
    // size of the compiled program
    uint32_t program_size = ESP.getSketchSize();

    // size of the file system
    uint32_t file_system_size = SPIFFS.totalBytes();

    // used size of the file system
    uint32_t file_system_used = SPIFFS.usedBytes();

    // free size in the flash memory
    uint32_t free_size = ESP.getFlashChipSize() - program_size - file_system_size + file_system_used;

    Serial.println("Program size: " + String(program_size) + " bytes");
    Serial.println("File system size: " + String(file_system_size) + " bytes");
    Serial.println ("File system used: " + String(file_system_used) + " bytes");
    Serial.println("Free space: " + String(free_size) + " bytes");

    pinMode(COOLER, OUTPUT);
    digitalWrite(COOLER, HIGH);
    
/// test ACD 

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_11);
   

    //***********************************
    //************* Setup -  récupération du fichier de configuration
    //***********************************
  
  // Should load default config if run for the first time
  Serial.println(F("Loading configuration..."));
  loadConfiguration(filename_conf, config);

  ///define if AP mode or load configuration
  /*if (loadwifi(wifi_conf, configwifi)) {
    AP=false; 
  }*/
  if (configwifi.recup_wifi()){
     strcat(logging.log_init,loguptime2());
     strcat(logging.log_init,"Wifi config \r\n");
       AP=false; 
  }

  configmodule.enphase_present=false; 
  configmodule.Fronius_present=false;

  loadmqtt(mqtt_conf ,configmqtt);
  // test if Fronius is present ( and load conf )
  configmodule.Fronius_present = loadfronius(fronius_conf, configmodule);

  // test if Enphase is present ( and load conf )
  loadenphase(enphase_conf, configmodule);
 
  /// recherche d'une sonde dallas
  #if DALLAS
  Serial.println("start 18b20");
  sensors.begin();
  /// recherche d'une sonde dallas
  dallas.detect = dallaspresent();
  #endif

  // Setup the ADC
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_11);

  //analogReadResolution(ADC_BITS);
  pinMode(ADC_INPUT, INPUT);

  // déclaration switch
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  digitalWrite(RELAY1, LOW);
  digitalWrite(RELAY2, LOW);




///  WIFI INIT
 
  connect_to_wifi();

#if OLED_ON == true
    Serial.println(OLEDSTART);
    // Initialising OLED
    #ifdef  DEVKIT1
      display.init();
      display.flipScreenVertically();
    
      display.clear();
    #endif
    
    #ifdef TTGO

        pinMode(SWITCH,INPUT_PULLUP);
        pinMode(BUTTON_LEFT,INPUT_PULLUP);

        display.init();
        //digitalWrite(TFT_BL, HIGH);
        display.setRotation(1);
        
        if (config.flip) {
        display.setRotation(3);
        }
        
        //display.begin();               // Initialise the display
        display.fillScreen(TFT_BLACK); // Black screen fill
        display.setCursor(0, 0, 2);
        display.setTextColor(TFT_WHITE,TFT_BLACK);  display.setTextSize(1);
        display.println(BOOTING);
    #endif
#endif

/// init du NTP
ntpinit(); 

//Jotta
  ledcSetup(0, GRIDFREQ , 8);
  ledcAttachPin(JOTTA, 0);
  ledcWrite(0, 0);

#if DIMMERLOCAL 
Dimmer_setup();
#endif

   // vérification de la présence d'index.html
  if(!SPIFFS.exists("/index.html.gz")){
    Serial.println(SPIFFSNO);  
    strcat(logging.log_init,loguptime2());
    strcat(logging.log_init,SPIFFSNO);
  }

  if(!SPIFFS.exists(filename_conf)){
    Serial.println(CONFNO);  
    strcat(logging.log_init,loguptime2());
    strcat(logging.log_init,CONFNO);

  }

/// chargement des conf de minuteries
  Serial.println("Loading minuterie");
  programme.name="/dimmer";
  programme.loadProgramme();
  programme.saveProgramme();

  // Initialize Dimmer State 
  gDisplayValues.dimmer = 0;

#if WIFI_ACTIVE == true
  #if WEBSSERVER == true
  //***********************************
	//************* Setup -  demarrage du webserver et affichage de l'oled
	//***********************************
   Serial.println("start Web server");
   call_pages();
  #endif

  // ----------------------------------------------------------------
  // TASK: Connect to WiFi & keep the connection alive.
  // ----------------------------------------------------------------
  /*if (!AP){
    xTaskCreate(
      keepWiFiAlive,
      "keepWiFiAlive",  // Task name
      8000,            // Stack size (bytes)
      NULL,             // Parameter
      5,                // Task priority
      NULL          // Task handle
      
    );  //pdMS_TO_TICKS(30000)
    } */

    xTaskCreate(
      keepWiFiAlive2,
      "keepWiFiAlive",  // Task name
      8000,            // Stack size (bytes)
      NULL,             // Parameter
      5,                // Task priority
      NULL          // Task handle
      
    );

    
  #endif

  // ----------------------------------------------------------------
  // TASK: Connect to AWS & keep the connection alive.
  // ----------------------------------------------------------------
    xTaskCreate(
      serial_read_task,
      "Serial Read",      // Task name
      3000,            // Stack size (bytes)
      NULL,             // Parameter
      1,                // Task priority
      NULL              // Task handle
    );  //pdMS_TO_TICKS(5000)


  // ----------------------------------------------------------------
  // TASK: Update the display every second
  //       This is pinned to the same core as Arduino
  //       because it would otherwise corrupt the OLED
  // ----------------------------------------------------------------
  #if OLED_ON == true 
  xTaskCreatePinnedToCore(
    updateDisplay,
    "UpdateDisplay",  // Task name
    10000,            // Stack size (bytes)
    NULL,             // Parameter
    2,                // Task priority
    NULL,             // Task handle
    ARDUINO_RUNNING_CORE
  );  //pdMS_TO_TICKS(5000)
  #endif

#if DALLAS
  // ----------------------------------------------------------------
  // Task: Read Dallas Temp
  // ----------------------------------------------------------------
  xTaskCreate(
    dallasread,
    "Dallas temp",  // Task name
    2000,                  // Stack size (bytes)
    NULL,                   // Parameter
    2,                      // Task priority
    NULL                    // Task handle
  );  //pdMS_TO_TICKS(10000)
#endif

#ifdef  TTGO
  // ----------------------------------------------------------------
  // Task: Update Dimmer power
  // ----------------------------------------------------------------
  attachInterrupt(SWITCH, function_off_screen, FALLING);
  attachInterrupt(BUTTON_LEFT, function_next_screen, FALLING);

  xTaskCreate( 
    switchDisplay,
    "Swith Oled",  // Task name
    4000,                  // Stack size (bytes)
    NULL,                   // Parameter
    2,                      // Task priority
    NULL                    // Task handle
  );  // pdMS_TO_TICKS(1000)
  
  #endif



  // ----------------------------------------------------------------
  // Task: measure electricity consumption ;)
  // ----------------------------------------------------------------
  xTaskCreate(
    measureElectricity,
    "Measure electricity",  // Task name
    15000,                  // Stack size (bytes)
    NULL,                   // Parameter
    7,                      // Task priority
    NULL                    // Task handle
  
  );  // pdMS_TO_TICKS(2000)

#if WIFI_ACTIVE == true
  #if DIMMER == true
  // ----------------------------------------------------------------
  // Task: Update Dimmer power
  // ----------------------------------------------------------------
  xTaskCreate(
    updateDimmer,
    "Update Dimmer",  // Task name
    5000,                  // Stack size (bytes)
    NULL,                   // Parameter
    4,                      // Task priority
    NULL                    // Task handle
  ); //pdMS_TO_TICKS(4000)
  
  // ----------------------------------------------------------------
  // Task: Get Dimmer temp
  // ----------------------------------------------------------------

    xTaskCreate(
    GetDImmerTemp,
    "Update temp",  // Task name
    5000,                  // Stack size (bytes)
    NULL,                   // Parameter
    4,                      // Task priority
    NULL                    // Task handle
  );  //pdMS_TO_TICKS(15000)
  #endif

#endif


  // ----------------------------------------------------------------
  // TASK: update time from NTP server.
  // ----------------------------------------------------------------
#if WIFI_ACTIVE == true
  if (!AP) {
    #if NTP  
      #if NTP_TIME_SYNC_ENABLED == true
        xTaskCreate(
          fetchTimeFromNTP,
          "Update NTP time",
          5000,            // Stack size (bytes)
          NULL,             // Parameter
          2,                // Task priority
          NULL              // Task handle
        );
        
      #endif
    #endif
  }
    
#endif

#if WIFI_ACTIVE == true


      #if WEBSSERVER == true
        AsyncElegantOTA.begin(&server);
        server.begin(); 
      #endif
  #ifndef LIGHT_FIRMWARE
      if (!AP) {
          if (config.mqtt) {
            Mqtt_init();

          // HA autoconf
          if (!client.connected()) { reconnect(); delay (1000);}
          if (configmqtt.HA) init_HA_sensor();
            
          }
      }
  #endif
  //if ( config.autonome == true ) {
    gDisplayValues.dimmer = 0; 
    dimmer_change( config.dimmer, config.IDXdimmer, gDisplayValues.dimmer,0 ) ; 
  //}

#endif

  #if OLED_ON == true
    #ifdef  DEVKIT1
      display.clear();
    #endif
  #endif





esp_register_shutdown_handler( handler_before_reset );

logging.power=true; logging.sct=true; logging.sinus=true; 

//WebSerial.begin(&server);
//WebSerial.msgCallback(recvMsg);

}


/// @brief / Loop function
void loop()
{
//// si perte du wifi après  6h, reboot
  if (AP) {
    reboot_after_lost_wifi(6);
  }

/// redémarrage sur demande
  if (config.restart) {
    //delay(5000);
    Serial.print(PV_RESTART);
    ESP.restart();
  }

/// vérification du buffer log 
 // if (logging.start.length() > LOG_MAX_STRING_LENGTH - 5 ) { 
  //  logging.start = "";
  //}
   ///  vérification de la tailld du buffer log_init ( 600 caractères max ) il est créé à 650 caractères ( enums.h )
   /// pour éviter les buffer overflow et fuite mémoire. 
   if (strlen(logging.log_init) > 600 ) {
    logging.log_init[0] = '\0';
    strcat(logging.log_init,"197}11}1");
   }
// affichage en mode serial de la taille de la chaine de caractère logging.log_init
//Serial.print( "log_init : " );
//Serial.println( strlen(logging.log_init) );


// vérification de la connexion wifi 
  if ( WiFi.status() != WL_CONNECTED ) {
      connect_to_wifi();
      }
    
  if (AP) {
    int number_client = WiFi.softAPgetStationNum(); // Nombre de stations connectées à ESP8266 soft-AP
    if (number_client == 0 ) {
      if (retry_wifi == 10 ) {
        retry_wifi = 0;
        connect_to_wifi();
      }
      if (retry_wifi < 10 ) {
        retry_wifi ++;
      }

    }
  }
/// connexion MQTT
#ifndef LIGHT_FIRMWARE
    if (!AP) {
      #if WIFI_ACTIVE == true
          if (config.mqtt) {
            if (!client.connected()) { reconnect(); }
          client.loop();
          
          }
          // TODO : désactivation MQTT en décochant la case, sans redémarrer
          // else {
          //     if (client.connected()) { client.disconnect(); }
          // }  

      #endif
    }
#endif
//// surveillance des fuites mémoires 
#ifndef LIGHT_FIRMWARE
  client.publish(gDisplayValues.pvname.c_str(), String(esp_get_free_heap_size()).c_str()) ;
  client.publish((gDisplayValues.pvname + " min free").c_str(), String(esp_get_minimum_free_heap_size()).c_str()) ;
#endif

  /// synchrisation de l'information entre le dimmer et l'affichage 
  int dimmer1_state = dimmer_getState();
  /// application uniquement si le dimmer est actif (Tmax non atteint)    
  if (dimmer1_state != 0 ) {  
  gDisplayValues.dimmer = dimmer1_state; 
  }


if (config.dimmerlocal) {
  ///////////////// gestion des activité minuteur 
  //// Dimmer 
    Serial.println(dimmer_hard.getPower());
    if (programme.run) { 
        //  minuteur en cours
        if (programme.stop_progr()) { 
          dimmer_hard.setPower(0); 
          dimmer_off();
          DEBUG_PRINTLN("programme.run");
          //sysvar.puissance=0;
          Serial.println("stop minuteur dimmer");
          //arret du ventilateur
          digitalWrite(COOLER, LOW);
          /// remonté MQTT
          #ifndef LIGHT_FIRMWARE
            Mqtt_send(String(config.IDX), String(dimmer_hard.getPower()),"pourcent"); // remonté MQTT de la commande réelle
            if (configmqtt.HA) {
              int instant_power = dimmer_hard.getPower();
              device_dimmer.send(String(instant_power * config.resistance/100));
            } 
          #endif
        } 
    } 
    else { 
      // minuteur à l'arret
      if (programme.start_progr()){ 
        //sysvar.puissance=config.localfuse; 
        dimmer_on();
        dimmer_hard.setPower(config.localfuse); 
        delay (50);
        Serial.println("start minuteur ");
        //demarrage du ventilateur 
        digitalWrite(COOLER, HIGH);
        
        /// remonté MQTT
        #ifndef LIGHT_FIRMWARE
          Mqtt_send(String(config.IDX), String(dimmer_hard.getPower()),"pourcent"); // remonté MQTT de la commande réelle
          if (configmqtt.HA) {
            int instant_power = dimmer_hard.getPower();
            device_dimmer.send(String(instant_power * config.resistance/100));
          } 
        #endif
        offset_heure_ete(); // on corrige l'heure d'été si besoin
      }
    }
}

/// fonction de reboot hebdomadaire ( lundi 00:00 )

if (!AP) {
    time_reboot();
}


  vTaskDelay(pdMS_TO_TICKS(10000));
}

/// @brief / end Loop function



void connect_to_wifi() {
  ///// AP WIFI INIT 
   

  if (AP || strcmp(configwifi.SID,"AP") == 0 ) {
      APConnect(); 
      //AP=true; 
      gDisplayValues.currentState = UP;
      gDisplayValues.IP = String(WiFi.softAPIP().toString());
      btStop();
      return; 
  }
  else {
      #if WIFI_ACTIVE == true
      WiFi.mode(WIFI_STA);
      //Esp_wifi_set_ps (WIFI_PS_NONE);
      WiFi.setSleep(false);
      WiFi.begin(configwifi.SID, configwifi.passwd); 
      int timeoutwifi=0;
      strcat(logging.log_init,loguptime2());
      strcat(logging.log_init,"Start Wifi Network");
      strcat(logging.log_init,String(configwifi.SID).c_str());
      strcat(logging.log_init,"\r\n");
      
      while ( WiFi.status() != WL_CONNECTED ) {
        delay(500);
        Serial.print(".");
        timeoutwifi++; 

        if (timeoutwifi > 20 ) {
              strcat(logging.log_init,loguptime2());
              strcat(logging.log_init,"timeout, go to AP mode \r\n");
              strcat(logging.log_init,loguptime2());
              strcat(logging.log_init,"Wifi State :");
              strcat(logging.log_init,loguptime2());
              
              switch (WiFi.status()) {
                  case 1:
                      strcat(logging.log_init,"SSID is not available");
                      break;
                  case 4:

                      strcat(logging.log_init,"The connection fails for all the attempts");
                      break;
                  case 5:
                      strcat(logging.log_init,"The connection is lost");
                      break;
                  case 6:
                      strcat(logging.log_init,"Disconnected from the network");
                      break;
                  default:
                      break;
          
              strcat(logging.log_init,"\r\n");
              } 
              break;
        }
    }

        //// timeout --> AP MODE 
        if ( timeoutwifi > 20 ) {
              WiFi.disconnect(); 
              //AP=true; 
              serial_println("timeout, go to AP mode ");
              
              gDisplayValues.currentState = UP;
             // gDisplayValues.IP = String(WiFi.softAPIP().toString());
              APConnect(); 
        }


      serial_println("WiFi connected");
      strcat(logging.log_init,loguptime2());
      strcat(logging.log_init,"Wifi connected\r\n");
      serial_println("IP address: ");
      serial_println(WiFi.localIP());
        serial_print("force du signal:");
        serial_print(WiFi.RSSI());
        serial_println("dBm");
      gDisplayValues.currentState = UP;
      gDisplayValues.IP = String(WiFi.localIP().toString());
      btStop();
      #endif
  }
}
/*
String loguptime() {
  String uptime_stamp;
  uptime::calculateUptime();
  uptime_stamp = String(uptime::getDays())+":"+String(uptime::getHours())+":"+String(uptime::getMinutes())+":"+String(uptime::getSeconds())+ "\t";
  return uptime_stamp;
} */


char *loguptime2() {
  static char uptime_stamp[20]; // Vous devrez définir une taille suffisamment grande pour stocker votre temps

  uptime::calculateUptime();

  // Utilisez snprintf pour formater le texte dans le tableau de caractères
  snprintf(uptime_stamp, sizeof(uptime_stamp), "%d:%d:%d:%d\t", uptime::getDays(), uptime::getHours(), uptime::getMinutes(), uptime::getSeconds());

  return uptime_stamp;
}

void handler_before_reset() {
  #ifndef LIGHT_FIRMWARE
  client.publish("panic", " ESP reboot" ) ;
  #endif
}

void reboot_after_lost_wifi(int timeafterlost) {
  uptime::calculateUptime();
  if ( uptime::getHours() > timeafterlost ) { 
    delay(15000);  
    config.restart = true; 
  }
}

void IRAM_ATTR function_off_screen() {
  gDisplayValues.screenbutton = true;
}

void IRAM_ATTR function_next_screen(){
  gDisplayValues.nextbutton = true;
  gDisplayValues.option++; 
  if (gDisplayValues.option > 2 ) { gDisplayValues.option = 1 ;}; 
}

/*
gDisplayValues.screenstate == HIGH ){ // if right button is pressed or HTTP call 
        if (digitalRead(TFT_PIN)==HIGH) {             // and the status flag is LOW
          gDisplayValues.screenstate = LOW ;  
 
          digitalWrite(TFT_PIN,LOW);     // and turn Off the OLED
          }                           // 
        else {                        // otherwise...      
          Serial.println("button left/bottom pressed");
          gDisplayValues.screenstate = LOW ;

          digitalWrite(TFT_PIN,HIGH);      // and turn On  the OLED
          if (config.ScreenTime !=0 ) {
            timer = millis();
          }
        }
      }*/