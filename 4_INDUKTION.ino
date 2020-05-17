class Induction
{
  unsigned long timeTurnedOff;

  long timeOutCommand = 5000;  // TimeOut für Seriellen Befehl
  long timeOutReaction = 2000; // TimeOut für Induktionskochfeld
  unsigned long lastInterrupt;
  unsigned long lastCommand;
  bool inputStarted = false;
  byte inputCurrent = 0;
  byte inputBuffer[33];
  bool isError = false;
  byte error = 0;

  long powerSampletime = 20000;
  unsigned long powerLast;
  long powerHigh = powerSampletime; // Dauer des "HIGH"-Anteils im Schaltzyklus
  long powerLow = 0;

public:
  byte PIN_WHITE = 9;     // RELAIS
  byte PIN_YELLOW = 9;    // AUSGABE AN PLATTE
  byte PIN_INTERRUPT = 9; // EINGABE VON PLATTE
  byte power = 0;
  byte newPower = 0;
  byte CMD_CUR = 0;          // Aktueller Befehl
  boolean isRelayon = false; // Systemstatus: ist das Relais in der Platte an?
  boolean isInduon = false;  // Systemstatus: ist Power > 0?
  boolean isPower = false;
  String mqtttopic = "";
  boolean isEnabled = false;
  int delayAfteroff = 120; // in seconds

  Induction()
  {
  }

  void change(byte pinwhite, byte pinyellow, byte pinblue, String topic, long delayoff, bool is_enabled)
  {
    if (isEnabled)
    {
      // aktuelle PINS deaktivieren
      if (isPin(PIN_WHITE))
      {
        digitalWrite(PIN_WHITE, HIGH);
        pins_used[PIN_WHITE] = false;
      }
      if (isPin(PIN_YELLOW))
      {
        digitalWrite(PIN_YELLOW, HIGH);
        pins_used[PIN_YELLOW] = false;
      }
      if (isPin(PIN_INTERRUPT))
      {
        digitalWrite(PIN_INTERRUPT, HIGH);
        pins_used[PIN_INTERRUPT] = false;
      }
      mqtt_unsubscribe();
    }

    PIN_WHITE = pinwhite;
    PIN_YELLOW = pinyellow;
    PIN_INTERRUPT = pinblue;

    mqtttopic = topic;
    delayAfteroff = delayoff;

    isEnabled = is_enabled;

    if (isEnabled)
    {
      // neue PINS aktiveren
      if (isPin(PIN_WHITE))
      {
        pinMode(PIN_WHITE, OUTPUT);
        digitalWrite(PIN_WHITE, LOW);
        pins_used[PIN_WHITE] = true;
      }
      if (isPin(PIN_YELLOW))
      {
        pinMode(PIN_YELLOW, OUTPUT);
        digitalWrite(PIN_YELLOW, LOW);
        pins_used[PIN_YELLOW] = true;
      }
      if (isPin(PIN_INTERRUPT))
      {
        pinMode(PIN_INTERRUPT, INPUT_PULLUP);
        pins_used[PIN_INTERRUPT] = true;
      }
      if (client.connected())
      {
        mqtt_subscribe();
      }
    }
  }

  void mqtt_subscribe()
  {
    if (isEnabled)
    {
      if (client.connected())
      {
        char subscribemsg[50];
        mqtttopic.toCharArray(subscribemsg, 50);
        Serial.print("Subscribing to ");
        Serial.println(subscribemsg);
        client.subscribe(subscribemsg);
      }
    }
  }

  void mqtt_unsubscribe()
  {
    if (client.connected())
    {
      char subscribemsg[50];
      mqtttopic.toCharArray(subscribemsg, 50);
      Serial.print("Unsubscribing from ");
      Serial.println(subscribemsg);
      client.unsubscribe(subscribemsg);
    }
  }

  void handlemqtt(char *payload)
  {
    StaticJsonDocument<128> jsonDocument;
    DeserializationError error = deserializeJson(jsonDocument, payload);
    if (error)
    {
      Serial.println(error.c_str());
      return;
    }
    String state = jsonDocument["state"];
    if (state == "off")
    {
      newPower = 0;
      return;
    }
    else
    {
      newPower = atoi(jsonDocument["power"]);
    }
  }

  bool updateRelay()
  {
    if (isInduon == true && isRelayon == false)
    {
      digitalWrite(PIN_WHITE, HIGH);
      return true;
    }
    if (isInduon == false && isRelayon == true)
    {
      if (millis() > timeTurnedOff + (delayAfteroff * 1000))
      {
        digitalWrite(PIN_WHITE, LOW);
        return false;
      }
    }
    if (isInduon == false && isRelayon == false)
    { /* Ist aus, bleibt aus. */
      return false;
    }
    return true; /* Ist an, bleibt an. */
  }

  void Update()
  {
    updatePower();

    isRelayon = updateRelay();

    if (isInduon && power > 0)
    {
      if (millis() > powerLast + powerSampletime)
      {
        powerLast = millis();
      }
      if (millis() > powerLast + powerHigh)
      {
        sendCommand(CMD[CMD_CUR - 1]);
        isPower = false;
      }
      else
      {
        sendCommand(CMD[CMD_CUR]);
        isPower = true;
      }
    }
    else if (isRelayon)
    {
      sendCommand(CMD[0]);
    }
  }

  void updatePower()
  {
    lastCommand = millis();
    if (power != newPower)
    { /* Neuen Befehl empfangen */
      if (newPower > 100)
      {
        newPower = 100; /* Nicht > 100 */
      }
      if (newPower < 0)
      {
        newPower = 0; /* Nicht < 0 */
      }
      power = newPower;
      timeTurnedOff = 0;
      isInduon = true;
      long difference = 0;
      if (power == 0)
      {
        CMD_CUR = 0;
        timeTurnedOff = millis();
        isInduon = false;
        difference = 0;
        goto setPowerLevel;
      }
      for (int i = 1; i < 7; i++)
      {
        if (power <= PWR_STEPS[i])
        {
          CMD_CUR = i;
          difference = PWR_STEPS[i] - power;
          goto setPowerLevel;
        }
      }
    setPowerLevel: /* Wie lange "HIGH" oder "LOW" */
      if (difference != 0)
      {
        powerLow = powerSampletime * difference / 20L;
        powerHigh = powerSampletime - powerLow;
      }
      else
      {
        powerHigh = powerSampletime;
        powerLow = 0;
      };
    }
  }

  void sendCommand(const int command[33])
  {
    digitalWrite(PIN_YELLOW, HIGH);
    delay(SIGNAL_START);
    digitalWrite(PIN_YELLOW, LOW);
    delay(SIGNAL_WAIT);

    for (int i = 0; i < 33; i++)
    {
      digitalWrite(PIN_YELLOW, HIGH);
      delayMicroseconds(command[i]);
      digitalWrite(PIN_YELLOW, LOW);
      delayMicroseconds(SIGNAL_LOW);
    }
  }
}

inductionCooker = Induction();

void handleInduction()
{
  if (inductionCooker.isEnabled)
  {
    inductionCooker.Update();
  }
}

void handleRequestInduction()
{
  StaticJsonDocument<512> jsonDocument;

  jsonDocument["enabled"] = inductionCooker.isEnabled;
  if (inductionCooker.isEnabled)
  {
    jsonDocument["relayOn"] = inductionCooker.isRelayon;
    jsonDocument["power"] = inductionCooker.power;
    jsonDocument["relayOn"] = inductionCooker.isRelayon;
    jsonDocument["topic"] = inductionCooker.mqtttopic;
    if (inductionCooker.isPower)
    {
      jsonDocument["powerLevel"] = inductionCooker.CMD_CUR;
    }
    else
    {
      jsonDocument["powerLevel"] = max(0, inductionCooker.CMD_CUR - 1);
    }
  }
  String response;
  serializeJson(jsonDocument, response);
  server.send(200, "application/json", response);
}

void handleRequestInductionConfig()
{
  String request = server.arg(0);
  String message;

  if (request == "isEnabled")
  {
    if (inductionCooker.isEnabled)
    {
      message = "1";
    }
    else
    {
      message = "0";
    }
    goto SendMessage;
  }
  if (request == "topic")
  {
    message = inductionCooker.mqtttopic;
    goto SendMessage;
  }
  if (request == "delay")
  {
    message = inductionCooker.delayAfteroff;
    goto SendMessage;
  }
  if (request == "pins")
  {
    int id = server.arg(1).toInt();
    byte pinswitched;
    switch (id)
    {
    case 0:
      pinswitched = inductionCooker.PIN_WHITE;
      break;
    case 1:
      pinswitched = inductionCooker.PIN_YELLOW;
      break;
    case 2:
      pinswitched = inductionCooker.PIN_INTERRUPT;
      break;
    }
    if (isPin(pinswitched))
    {
      message += F("<option>");
      message += PinToString(pinswitched);
      message += F("</option><option disabled>──────────</option>");
    }

    for (int i = 0; i < NUMBER_OF_PINS; i++)
    {
      if (pins_used[PINS[i]] == false)
      {
        message += F("<option>");
        message += PIN_NAMES[i];
        message += F("</option>");
      }
      yield();
    }
    goto SendMessage;
  }

SendMessage:
  server.send(200, "text/plain", message);
}

void handleSetIndu()
{

  byte pin_white = inductionCooker.PIN_WHITE;
  byte pin_blue = inductionCooker.PIN_INTERRUPT;
  byte pin_yellow = inductionCooker.PIN_YELLOW;
  long delayoff = inductionCooker.delayAfteroff;
  bool is_enabled = inductionCooker.isEnabled;
  String topic = inductionCooker.mqtttopic;

  for (int i = 0; i < server.args(); i++)
  {
    if (server.argName(i) == "enabled")
    {
      if (server.arg(i) == "1")
      {
        is_enabled = true;
      }
      else
      {
        is_enabled = false;
      }
    }
    if (server.argName(i) == "topic")
    {
      topic = server.arg(i);
    }
    if (server.argName(i) == "pinwhite")
    {
      pin_white = StringToPin(server.arg(i));
    }
    if (server.argName(i) == "pinyellow")
    {
      pin_yellow = StringToPin(server.arg(i));
    }
    if (server.argName(i) == "pinblue")
    {
      pin_blue = StringToPin(server.arg(i));
    }
    if (server.argName(i) == "delay")
    {
      delayoff = server.arg(i).toInt();
    }
    yield();
  }

  inductionCooker.change(pin_white, pin_yellow, pin_blue, topic, delayoff, is_enabled);

  saveConfig();
  server.send(201, "text/plain", "created");
}
