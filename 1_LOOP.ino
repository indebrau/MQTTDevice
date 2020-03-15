void loop()
{

  // Webserver prüfen
  server.handleClient();

  // Sys Update
  if (millis() > lastToggledSys + SYS_UPDATE)
  {
    // WiFi Status prüfen, ggf. Reconnecten
    if (WiFi.status() != WL_CONNECTED)
    {
      drawDisplayContentError();
      if(!wifiManager.autoConnect(deviceName)) {
        Serial.println("Connection not possible, timeout, restart!");
        rebootDevice();
      } 
    }

    // OTA
    ArduinoOTA.handle();

    // MQTT Status prüfen
    if (!client.connected())
    {
      mqttreconnect();
    }

    drawDisplayContent();
    client.loop();
    lastToggledSys = millis();
  }

  if (millis() > lastToggled + UPDATE)
  {
    // Sensoren aktualisieren
    handleSensors();
    // Aktoren aktualisieren
    handleActors();
    // Induktionskochfeld
    handleInduction();
    lastToggled = millis();
  }
}
