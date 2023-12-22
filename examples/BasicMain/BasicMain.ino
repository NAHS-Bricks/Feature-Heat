#include <Arduino.h>
#include <nahs-Bricks-OS.h>
// include all features of brick
#include <nahs-Bricks-Feature-Heat.h>

void setup() {
  // Now register all the features under All
  // Note: the order of registration is the same as the features are handled internally by FeatureAll
  FeatureAll.registerFeature(&FeatureHeat);

  // Set Brick-Specific stuff
  BricksOS.setSetupPin(D7);
  FeatureAll.setBrickType(6);

  // Set Brick-Specific (feature related) stuff
  Wire.begin();
  LatchExpander.begin(45);
  FeatureHeat.setHeatPin(LatchExpander, 0);
  FeatureHeat.setTempPin(D6);

  // Finally hand over to BrickOS
  BricksOS.handover();
}

void loop() {
  // Not used on Bricks
}