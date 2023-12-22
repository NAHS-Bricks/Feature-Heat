#include <nahs-Bricks-Feature-Heat.h>
#include <nahs-Bricks-Lib-SerHelp.h>
#include <ESP8266WiFi.h>

NahsBricksFeatureHeat::NahsBricksFeatureHeat() {
}

/*
Returns name of feature
*/
String NahsBricksFeatureHeat::getName() {
    return "heat";
}

/*
Returns version of feature
*/
uint16_t NahsBricksFeatureHeat::getVersion() {
    return version;
}

/*
Configures FSmem und RTCmem variables (prepares feature to be fully operational)
*/
void NahsBricksFeatureHeat::begin() {
    _oneWire.begin(_oneWirePin);
    _DS18B20.setOneWire(&_oneWire);
    _DS18B20.begin();

    if (!FSdata.containsKey("sCorr")) FSdata["sCorr"] = 0;  // default temp sensor correction value
    if (!FSdata.containsKey("sPrec")) FSdata["sPrec"] = 11;  // default temp sensor precision
    if (!FSdata.containsKey("hOff")) FSdata["hOff"] = 1;  // default heater off state
    
    if(!RTCmem.isValid()) {
        RTCdata->tempCorrRequested = false;
        RTCdata->tempPrecisionRequested = false;

        RTCdata->tempSensorCorrection = FSdata["sCorr"].as<float>();
        RTCdata->tempSensorPrecision = FSdata["sPrec"].as<uint8_t>();
        RTCdata->heatOffState = FSdata["hOff"].as<uint8_t>();

        if (_DS18B20.getDeviceCount() >= 1) {
            RTCdata->tempSensorConnected = true;
            _DS18B20.getAddress(RTCdata->tempSensorAddr, 0);
        }
        else RTCdata->tempSensorConnected = false;

        _initHeat();
    }
}

/*
Starts background processes like fetching data from other components
*/
void NahsBricksFeatureHeat::start() {
    // if brick just started up, configure the correct precision and do a dummy reading
    if(!RTCmem.isValid()) {
        _transmitPrecisionToSensors(false);
    }

    // start the Temp-Conversion in Background as this takes some time
    if (RTCdata->tempSensorConnected) {
        _DS18B20.setWaitForConversion(false);
        _DS18B20.requestTemperaturesByAddress(RTCdata->tempSensorAddr);
    }
}

/*
Adds data to outgoing json, that is send to BrickServer
*/
void NahsBricksFeatureHeat::deliver(JsonDocument* out_json) {
    // deliver sensors precision if requested
    if (RTCdata->tempPrecisionRequested) {
        RTCdata->tempPrecisionRequested = false;
        out_json->operator[]("p").set(RTCdata->tempSensorPrecision);
    }

    // deliver sensors correction values if requested
    if (RTCdata->tempCorrRequested) {
        RTCdata->tempCorrRequested = false;
        JsonArray c_array;
        if (out_json->containsKey("c"))
            c_array = out_json->operator[]("c").as<JsonArray>();
        else
            c_array = out_json->createNestedArray("c");
        JsonArray s_array = c_array.createNestedArray();
        s_array.add(_tempSensorName());
        s_array.add(RTCdata->tempSensorCorrection);
    }

    // wait for temperature conversion to complete and deliver the temperatures (if a tempSensor is connected)
    if (RTCdata->tempSensorConnected) {
        JsonArray t_array;
        if (out_json->containsKey("t"))
            t_array = out_json->operator[]("t").as<JsonArray>();
        else
            t_array = out_json->createNestedArray("t");
        while(!_DS18B20.isConversionComplete()) delay(1);
        JsonArray s_array = t_array.createNestedArray();
        s_array.add(_tempSensorName());
        s_array.add(_DS18B20.getTempC(RTCdata->tempSensorAddr) + RTCdata->tempSensorCorrection);
    }
}

/*
Processes feedback coming from BrickServer
*/
void NahsBricksFeatureHeat::feedback(JsonDocument* in_json) {
    //set new heat state if delivered
    if (in_json->containsKey("h")) {
        uint8_t state = in_json->operator[]("h").as<uint8>();
        if (state == 0)
            _turnHeatOff();
        else if (state == 1)
            _turnHeatOn();
    }

    // check if new tempSensorPrecision value is delivered
    if (in_json->containsKey("p")) {
        uint8_t p = in_json->operator[]("p").as<uint8_t>();
        if (p >= 9 and p <=12) {
            RTCdata->tempSensorPrecision = p;
            _transmitPrecisionToSensors(true);
        }
    }

    // evaluate requests
    if (in_json->containsKey("r")) {
        for (JsonVariant value : in_json->operator[]("r").as<JsonArray>()) {
            switch(value.as<uint8_t>()) {
                case 4:
                    RTCdata->tempCorrRequested = true;
                    break;
                case 6:
                    RTCdata->tempPrecisionRequested = true;
                    break;
            }
        }
    }
}

/*
Finalizes feature (closes stuff)
*/
void NahsBricksFeatureHeat::end() {
}

/*
Prints the features RTCdata in a formatted way to Serial (used for brickSetup)
*/
void NahsBricksFeatureHeat::printRTCdata() {
    Serial.print("  tempPrecisionRequested: ");
    SerHelp.printlnBool(RTCdata->tempPrecisionRequested);
    Serial.print("  tempCorrRequested: ");
    SerHelp.printlnBool(RTCdata->tempCorrRequested);
    Serial.print("  tempSensorConnected: ");
    SerHelp.printlnBool(RTCdata->tempSensorConnected);
    Serial.print("  tempSensorPrecision: ");
    Serial.println(RTCdata->tempSensorPrecision);
    Serial.print("  tempSensorCorrection: ");
    Serial.println(RTCdata->tempSensorCorrection);
    Serial.print("  tempSensorAddr: ");
    if (RTCdata->tempSensorConnected)
        Serial.println(_deviceAddrToString(RTCdata->tempSensorAddr));
    else
        Serial.println("---");
    Serial.print("  heatOffState: ");
    Serial.println(RTCdata->heatOffState);
}

/*
Prints the features FSdata in a formatted way to Serial (used for brickSetup)
*/
void NahsBricksFeatureHeat::printFSdata() {
    Serial.print("  defaultTempPrecision: ");
    Serial.println(FSdata["sPrec"].as<uint8_t>());
    Serial.print("  defaultTempCorrection: ");
    Serial.println(FSdata["sCorr"].as<float>());
    Serial.print("  defaultHeatOffState: ");
    Serial.println(FSdata["hOff"].as<uint8_t>());
}

/*
BrickSetup hands over to this function, when features-submenu is selected
*/
void NahsBricksFeatureHeat::brickSetupHandover() {
    _printMenu();
    while (true) {
        Serial.println();
        Serial.print("Select: ");
        uint8_t input = SerHelp.readLine().toInt();
        switch(input) {
            case 1:
                _setHeatDefaultOffState();
                break;
            case 2:
                _bsTurnHeatOn();
                break;
            case 3:
                _bsTurnHeatOff();
                break;
            case 4:
                _readTempSensorRaw();
                break;
            case 5:
                _readTempSensorCorr();
                break;
            case 6:
                _setTempDefaultPrecision();
                break;
            case 7:
                _setTempDefaultCorr();
                break;
            case 9:
                Serial.println("Returning to MainMenu!");
                return;
                break;
            default:
                Serial.println("Invalid input!");
                _printMenu();
                break;
        }
    }
}

/*
Brick-Specific helper to assign the ExpanderPin (or LatchExpanderPin) to be used to turn on and off heating
Frequent calls of this function overwrites the previous set pin. Therefore only one ExpanderPin can be used by this feature
*/
void NahsBricksFeatureHeat::setHeatPin(NahsBricksLibCoIC_Expander &expander, uint8_t pinIndex) {
    _expander = &expander;
    _expanderPin = pinIndex;
}

/*
Brick-Specific helper to assign the ESP-Pin which is used to communicate via OneWire to the Radiator-Temp-Sensor
*/
void NahsBricksFeatureHeat::setTempPin(uint8_t pin) {
    _oneWirePin = pin;
}

/*
Internal helper to convert a sensor's DeviceAddress to a String
*/
String NahsBricksFeatureHeat::_deviceAddrToString(DeviceAddress deviceAddress) {
    String str = "";
    for (uint8_t i = 0; i < 8; ++i) {
        if (deviceAddress[i] < 16 ) str += String(0, HEX);
        str += String(deviceAddress[i], HEX);
    }
    return str;
}

/*
Internal helper to build the name of the temp sensor
*/
String NahsBricksFeatureHeat::_tempSensorName() {
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    mac.toLowerCase();
    return mac + "_rad";
}

/*
Internal helper to configure the precision of sensors
*/
void NahsBricksFeatureHeat::_transmitPrecisionToSensors(bool inBackground) {
    _DS18B20.setResolution(RTCdata->tempSensorPrecision);
    _DS18B20.setWaitForConversion(!inBackground);
    _DS18B20.requestTemperatures();  // Request the temperatures once to be dumped, as the first reading allways returns 85C
}

/*
Internal helper to set used ExpanderPins as Output and write initial State
*/
void NahsBricksFeatureHeat::_initHeat() {
    _expander->setOutput(_expanderPin);
    _turnHeatOff();
}

/*
Internal helper to turn heat on, therefore bringing corrensponding expander pin into the right state
*/
void NahsBricksFeatureHeat::_turnHeatOn() {
    if (RTCdata->heatOffState == 0)
        _expander->writeOutput(_expanderPin, true);
    else
        _expander->writeOutput(_expanderPin, false);
}

/*
Internal helper to turn heat off, therefore bringing corrensponding expander pin into the right state
*/
void NahsBricksFeatureHeat::_turnHeatOff() {
    _expander->writeOutput(_expanderPin, RTCdata->heatOffState);
}

/*
Helper to print Feature submenu during BrickSetup
*/
void NahsBricksFeatureHeat::_printMenu() {
    Serial.println("1) Set Heat Default Off State");
    Serial.println("2) Turn Heat ON");
    Serial.println("3) Turn Heat OFF");
    Serial.println("4) Read Temp Sensor (raw)");
    Serial.println("5) Read Temp Sensor (with corr)");
    Serial.println("6) Set Temp Default Precision");
    Serial.println("7) Set Temp Sensor Corr");
    Serial.println("9) Return to MainMenu");
}

/*
BrickSetup function to set the default off-state for heater
*/
void NahsBricksFeatureHeat::_setHeatDefaultOffState() {
    Serial.print("Enter 0 (low-signal) or 1 (high-signal): ");
    uint8_t input = SerHelp.readLine().toInt();
    if (input > 1) {
        Serial.println("Invalid state!");
        return;
    }
    FSdata["hOff"] = input;
    RTCdata->heatOffState = input;
    Serial.println("Turning heat off...");
    _turnHeatOff();
    Serial.print("Set off-state to: ");
    Serial.println(input);
}

/*
BrickSetup function to manually turn heater ON
*/
void NahsBricksFeatureHeat::_bsTurnHeatOn() {
    Serial.println("Turning heat on...");
    _turnHeatOn();
}

/*
BrickSetup function to manually turn heater OFF
*/
void NahsBricksFeatureHeat::_bsTurnHeatOff() {
    Serial.println("Turning heat off...");
    _turnHeatOff();
}

/*
BrickSetup function to do a temp-reading on sensor (and put it out WITHOUT correction value)
*/
void NahsBricksFeatureHeat::_readTempSensorRaw() {
    if (RTCdata->tempSensorConnected) {
        Serial.println("Requesting Temperature...");
        _DS18B20.setWaitForConversion(true);
        _DS18B20.requestTemperatures();
        Serial.print(_tempSensorName());
        Serial.print(": ");
        Serial.println(_DS18B20.getTempC(RTCdata->tempSensorAddr));
    }
    else {
        Serial.println("No TempSensor detected...");
    }
}

/*
BrickSetup function to do a temp-reading on sensor (and put it out WITH correction value)
*/
void NahsBricksFeatureHeat::_readTempSensorCorr() {
    if (RTCdata->tempSensorConnected) {
        Serial.println("Requesting Temperature...");
        _DS18B20.setWaitForConversion(true);
        _DS18B20.requestTemperatures();
        Serial.print(_tempSensorName());
        Serial.print(": ");
        Serial.println(_DS18B20.getTempC(RTCdata->tempSensorAddr) + RTCdata->tempSensorCorrection);
    }
    else {
        Serial.println("No TempSensor detected...");
    }
}

/*
BrickSetup function to set the temp default precision
*/
void NahsBricksFeatureHeat::_setTempDefaultPrecision() {
    Serial.print("Enter precision (8 to 12): ");
    uint8_t input = SerHelp.readLine().toInt();
    if (input < 8 || input > 12) {
        Serial.println("Invalid precision!");
        return;
    }
    FSdata["sPrec"] = input;
    RTCdata->tempSensorPrecision = input;
    Serial.println("Configuring sensor...");
    _transmitPrecisionToSensors(false);
    Serial.print("Set precision to: ");
    Serial.println(input);
}

/*
BrickSetup function to set a temp default correction value
*/
void NahsBricksFeatureHeat::_setTempDefaultCorr() {
    Serial.print("Enter correction value: ");
    float corr = SerHelp.readLine().toFloat();
    FSdata["sCorr"] = corr;
    RTCdata->tempSensorCorrection = corr;
    Serial.print("Set correction value to: ");
    Serial.println(corr);
}


//------------------------------------------
// globally predefined variable
#if !defined(NO_GLOBAL_INSTANCES)
NahsBricksFeatureHeat FeatureHeat;
#endif
