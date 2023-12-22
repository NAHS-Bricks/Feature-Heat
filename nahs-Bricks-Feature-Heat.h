#ifndef NAHS_BRICKS_FEATURE_HEAT_H
#define NAHS_BRICKS_FEATURE_HEAT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <nahs-Bricks-Lib-CoIC.h>
#include <nahs-Bricks-Feature-BaseClass.h>
#include <nahs-Bricks-Lib-RTCmem.h>
#include <nahs-Bricks-Lib-FSmem.h>

class NahsBricksFeatureHeat : public NahsBricksFeatureBaseClass {
    private:  // Variables
        static const uint16_t version = 1;
        typedef struct {
            DeviceAddress tempSensorAddr;
            bool tempPrecisionRequested;
            bool tempCorrRequested;
            bool tempSensorConnected;
            uint8_t tempSensorPrecision;
            uint8_t heatOffState;
            float tempSensorCorrection;
        } _RTCdata;
        _RTCdata* RTCdata = RTCmem.registerData<_RTCdata>();
        JsonObject FSdata = FSmem.registerData("w");
        NahsBricksLibCoIC_Expander* _expander;
        uint8_t _expanderPin;
        uint8_t _oneWirePin;
        OneWire _oneWire;
        DallasTemperature _DS18B20;

    public: // BaseClass implementations
        NahsBricksFeatureHeat();
        String getName();
        uint16_t getVersion();
        void begin();
        void start();
        void deliver(JsonDocument* out_json);
        void feedback(JsonDocument* in_json);
        void end();
        void printRTCdata();
        void printFSdata();
        void brickSetupHandover();

    public:  // Brick-Specific setter
        void setHeatPin(NahsBricksLibCoIC_Expander &expander, uint8_t pinIndex);
        void setTempPin(uint8_t pin);

    private:  // internal Helpers
        String _deviceAddrToString(DeviceAddress deviceAddress);
        String _tempSensorName();
        void _transmitPrecisionToSensors(bool inBackground);
        void _initHeat();
        void _turnHeatOn();
        void _turnHeatOff();

    private:  // BrickSetup Helpers
        void _printMenu();
        void _setHeatDefaultOffState();
        void _bsTurnHeatOn();
        void _bsTurnHeatOff();
        void _readTempSensorRaw();
        void _readTempSensorCorr();
        void _setTempDefaultPrecision();
        void _setTempDefaultCorr();
};

#if !defined(NO_GLOBAL_INSTANCES)
extern NahsBricksFeatureHeat FeatureHeat;
#endif

#endif // NAHS_BRICKS_FEATURE_HEAT_H
