// loovee, 2014-6-13
// sergey, 2022

#include <SPI.h>
#include "mcp_can.h"


//messages
#define startForcedSterilization 1 
#define startKeepSterilization 2
#define stopKeepSterilization 3
#define startBottleFilling 4
#define startNextBottle 5
#define activeForcedSterilization 6
#define passiveForcedSterilization 7
#define activeBottleFilling 8
#define passiveBottleFilling 9
#define activeNextBottle 10
#define passiveNextBottle 11
//processes
#define contunue_loop() goto SKIP; 

#define all 0
#define controller1 1
#define controller2 2
#define controller3 3
#define controller4 4

enum  Proc {Initialization=0, TankFilling, MainLoop, /* foreign */ ForcedSterilization, NextBottle, BottleFilling };
enum  State {
  InitializationBegin, InitializationWaitForFilling, InitializationWaitForSterilization, 
  MainLoopBegin, MainLoopWaitForNextBottle, MainLoopWaitForFilling,
  TankFillingBegin, TankFillingTankFilled
  };


Proc currProc;
State currState[] = {InitializationBegin, MainLoopBegin, TankFillingBegin}; //init states for each proccess
bool procActive[] = {true, false, false, false, false, false}; //running processes

bool iLowLevel = false;
bool iHighLevel = false;
bool oFillTank = false;


#define ON true
#define OFF false

// the cs pin of the version after v1.1 is default to D9
// v0.9b and v1.0 is default D10
const int SPI_CS_PIN = 10;
MCP_CAN CAN(SPI_CS_PIN);


void setup_CAN() {
  Serial.begin(115200);
  CAN.init_CS(9);
  Serial.println("hi I am bottle controller1!");
  Serial.println("Init");
  START_INIT:
    if (CAN_OK == CAN.begin(CAN_500KBPS, MCP_8MHz))  // init can bus : baudrate = 500k
    {
      Serial.println("CAN BUS Shield init ok!");
    } else {
      Serial.println("CAN BUS Shield init fail");
      Serial.println("Init CAN BUS Shield again");
      delay(1000);
      goto START_INIT;
    }
  Serial.println("CAN ready!");
}

void receive_messages() {
  byte buf[8];
  byte len;

  while (CAN_MSGAVAIL == CAN.checkReceive())  // check if data is coming
    {
      CAN.readMsgBuf(&len, buf);            
      unsigned short canId = CAN.getCanId();
      if (canId == controller2 || canId == all) {
        if (buf[0] == startForcedSterilization) {
          procActive[Proc::ForcedSterilization] = true;
        }
      }
    }
}

void message(int to, int type) {
  byte buf[1];
  buf[0] = type;
  CAN.sendMsgBuf(to, 0, buf, sizeof(buf), true);
}


void setup() {
  setup_CAN();
  currProc = Proc::Initialization;
  currState[Proc::Initialization] = InitializationBegin;
}


//Arduino loop
void loop() {
  receive_messages();
  switch (currProc) {
    case Proc::Initialization: if (procActive[Proc::Initialization]) {
      switch (currState[Proc::Initialization]) {
        case InitializationBegin: {
          Serial.println("Initialization:Begin");
          Serial.println("Start process TankFilling [local]");
          procActive[Proc::TankFilling] = true;
          currState[Proc::Initialization] = InitializationWaitForFilling;
          contunue_loop();
        }
        case InitializationWaitForFilling: {
          Serial.println("Initialization:WaitForFilling");
          if (!procActive[Proc::TankFilling]) {
            Serial.println("Start process ForcedSterilization [controller2]");
            message(controller2, startForcedSterilization);
            currState[Proc::Initialization] = InitializationWaitForSterilization;
            contunue_loop();         
          }
        }
        case InitializationWaitForSterilization: {
          Serial.println("Initialization:WaitForSterilization");
          if (!procActive[Proc::ForcedSterilization]) {
            Serial.println("Start process KeepSterilization [controller2]");
            message(controller2, startKeepSterilization);\
            Serial.println("Start process MainLoop [local]");
            procActive[Proc::MainLoop] = true;
            procActive[Proc::Initialization] = false;
            Serial.println("Stoped process Initialization [local]");
            contunue_loop();         
          }
        }
      }
      break;
    }
    case Proc::MainLoop: if (procActive[Proc::MainLoop]) {
      switch (currState[Proc::MainLoop]) {
        case MainLoopBegin: {
          Serial.println("Start process NextBottle [controller4]");
          message(controller4, startNextBottle);
          currState[Proc::MainLoop] = MainLoopWaitForNextBottle;
          contunue_loop();
        }
        case MainLoopWaitForNextBottle: {
          if (!procActive[Proc::NextBottle]) {
            Serial.println("Start process BottleFilling [controller3]");
            message(controller3, startBottleFilling);
            currState[Proc::MainLoop] = MainLoopWaitForFilling;
            contunue_loop();
          }
        }
        case MainLoopWaitForFilling: {
          if (!procActive[Proc::BottleFilling]) {
            if (iLowLevel == false) { 
              Serial.println("Stop process KeepSterilization [controller2]");
              message(controller2, stopKeepSterilization);
              Serial.println("Start process Initialization [local]");
              procActive[Proc::Initialization] = true;
              procActive[Proc::MainLoop] = false;
              Serial.println("Stopped process MainLoop [local]");
              contunue_loop();
            } else {
              //restart
              Serial.println("Restart process MainLoop [local]");
              currState[Proc::MainLoop] = MainLoopBegin;
              contunue_loop();
            }
          }
        }
      }
      break;
    }
    case Proc::TankFilling: if (procActive[Proc::TankFilling]) {
      switch (currState[Proc::TankFilling]) {
        case TankFillingBegin: {
          Serial.println("TankFilling:TankFillingBegin");
          if (iHighLevel != ON) {
            oFillTank = ON;
            Serial.println("oFillTank = ON");
          }
            currState[Proc::TankFilling] = TankFillingTankFilled;
            contunue_loop();
        }
        case TankFillingTankFilled: {
          Serial.println("TankFilling:TankFilled");
          if (iHighLevel == ON) {
            oFillTank = OFF;
            Serial.println("oFillTank = OFF");
            procActive[Proc::TankFilling] = false;
            contunue_loop();
          }
        }
      }
      break;
    }
  }

  SKIP:
  delay(1000);
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
