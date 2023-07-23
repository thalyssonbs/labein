
char*  rst_Reason() {
  char buff[32];
  switch (ESP.getResetInfoPtr()->reason) {
    
    case REASON_DEFAULT_RST: 
      // Inicialização normal
      strcpy_P(buff, PSTR("Power on"));
      break;
      
    case REASON_WDT_RST:
      // Estouro do Hardware Watchdog
      strcpy_P(buff, PSTR("Hardware Watchdog"));
      break;
      
    case REASON_EXCEPTION_RST:
      // Exception Reset
      strcpy_P(buff, PSTR("Exception"));      
      break;
      
    case REASON_SOFT_WDT_RST:
      // Estouro do Software Watchdog
      strcpy_P(buff, PSTR("Software Watchdog"));
      break;
      
    case REASON_SOFT_RESTART: 
      // Software/System Restart
      strcpy_P(buff, PSTR("Software/System restart"));
      break;
      
    case REASON_DEEP_SLEEP_AWAKE:
      // Despertar do modo suspensão
      strcpy_P(buff, PSTR("Deep-Sleep Wake"));
      break;
      
    case REASON_EXT_SYS_RST:
      // Reinicialização externa (Reset Pin)
      strcpy_P(buff, PSTR("External System"));
      break;
      
    default:  
      // Reinicialização desconhecida
      strcpy_P(buff, PSTR("Unknown"));     
      break;
  }
  return buff;
  //umidificador.sendPushNotification("Reinicialização: "+ String(buff));
  
}
