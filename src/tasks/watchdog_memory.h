#ifndef TASK_WATCHDOG_MEMORY
#define TASK_WATCHDOG_MEMORY

//***********************************
//************* LIBRAIRIES ESP
//***********************************
#include <Arduino.h>
#include "HTTPClient.h"

//***********************************
//************* PROGRAMME PVROUTER
//***********************************
#include "../config/enums.h"
#include "../config/config.h"

//***********************************
//************* Variables externes
//***********************************
extern Memory task_mem; 
extern Logs logging;

//***********************************
//************* watchdog_memory
//***********************************
void watchdog_memory(void * parameter){ // NOSONAR
  for (;;){
    bool need_reboot = false;

    // si valeur mémoire en dessous de  100 reboot
    if (task_mem.task_keepWiFiAlive2 < 100) {
      need_reboot = true; 
    }


    /* plus besoin car arret de la tache au bout d'un certain temps
    if (task_mem.task_serial_read_task < 100) {
      need_reboot = true; 
    }
    */


    if (task_mem.task_dallas_read < 100) {
      need_reboot = true; 
    }

    if (task_mem.task_updateDimmer < 100) {
        need_reboot = true; 
    }

    if (task_mem.task_GetDImmerTemp < 100) {
      need_reboot = true; 
    }

    if (task_mem.task_measure_electricity < 100) {
      need_reboot = true; 
    }

    if (task_mem.task_send_mqtt < 100) {
      need_reboot = true; 
    }

    if (task_mem.task_switchDisplay < 100) {
      need_reboot = true; 
    }

    if (task_mem.task_updateDisplay < 100) {
      need_reboot = true; 
    }

    if (task_mem.task_loop < 100) {
      need_reboot = true; 
    }


    /* plus besoin car arret de la tache au bout d'un certain temps
    if (task_mem.task_mdns_discovery < 100) {
      need_reboot = true; 
    }    
    */


    // si need  reboot alors reboot
    if (need_reboot) {
      savelogs("-- reboot watchdog memoire -- ");
      ESP.restart();
    } 

    // Sleep for 10 seconds, avant de refaire une analyse

    vTaskDelay(pdMS_TO_TICKS(20000+(esp_random() % 61) - 30));
  }
}

#endif
