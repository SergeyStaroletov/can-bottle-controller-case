// loovee, 2014-6-13
// sergey, 2022

#include <SPI.h>
#include "mcp_can.h"


//messages
enum Msg {
  startForcedSterilization,
  startKeepSterilization,
  stopKeepSterilization,
  startBottleFilling,
  startNextBottle,
  activeForcedSterilization,
  passiveForcedSterilization,
  activeBottleFilling,
  passiveBottleFilling,
  activeNextBottle,
  passiveNextBottle,
  activeKeepSterilization,
  passiveKeepSterilization
};

//controllers
enum Controller {all, controller1, controller2, controller3, controller4};

enum Proc {Initialization=0, TankFilling, MainLoop, /* foreign */ ForcedSterilization, KeepSterilization /*?*/, NextBottle, BottleFilling };
enum State {
  InitializationBegin, InitializationWaitForFilling, InitializationWaitForSterilization, 
  MainLoopBegin, MainLoopWaitForNextBottle, MainLoopWaitForFilling,
  TankFillingBegin, TankFillingTankFilled
  };


Proc currProc;
State currState[] = {InitializationBegin, MainLoopBegin, TankFillingBegin}; //init states for each proccess
bool procActive[] = {true, false, false, false, false, false, false}; //running processes

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
  CAN.init_CS(10);
  Serial.println("hi I am bottle controller1!");
  Serial.println("Init");
  START_INIT:
    if (CAN_OK == CAN.begin(CAN_500KBPS, MCP_8MHz))  // init can bus : baudrate = 500k
    {
      Serial.println("CAN BUS Shield init ok");
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
      //Handling messages to start/stop something..
      if (canId == all) {
        if (buf[0] == Msg::activeForcedSterilization) {
          Serial.println("[can] ForcedSterilization is active");          
          procActive[Proc::ForcedSterilization] = true;
        }
        if (buf[0] == Msg::passiveForcedSterilization) {
          Serial.println("[can] ForcedSterilization is passive");          
          procActive[Proc::ForcedSterilization] = false;
        }
        if (buf[0] == Msg::activeKeepSterilization) {
          Serial.println("[can] KeepSterilization is active");          
          procActive[Proc::KeepSterilization] = true;
        }
        if (buf[0] == Msg::passiveKeepSterilization) {
          Serial.println("[can] KeepSterilization is passive");          
          procActive[Proc::KeepSterilization] = false;
        }
        if (buf[0] == Msg::activeNextBottle) {
          Serial.println("[can] NextBottle is active");          
          procActive[Proc::NextBottle] = true;
        }
        if (buf[0] == Msg::passiveNextBottle) {
          Serial.println("[can] NextBottle is passive");          
          procActive[Proc::NextBottle] = false;
        }
      }
    }
}

void message(int to, int type) {
  byte buf[1];
  buf[0] = type;
  CAN.sendMsgBuf((unsigned long)to, 0, sizeof(buf), buf, true);
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
          break;
        }
        case InitializationWaitForFilling: {
          Serial.println("Initialization:WaitForFilling");
          if (!procActive[Proc::TankFilling]) {
            Serial.println("Start process ForcedSterilization [controller2]");
            message(Controller::controller2, Msg::startForcedSterilization);
            currState[Proc::Initialization] = InitializationWaitForSterilization;
            break;     
          }
        }
        case InitializationWaitForSterilization: {
          Serial.println("Initialization:WaitForSterilization");
          if (!procActive[Proc::ForcedSterilization]) {
            Serial.println("Start process KeepSterilization [controller2]");
            message(Controller::controller2, Msg::startKeepSterilization);
            Serial.println("Start process MainLoop [local]");
            procActive[Proc::MainLoop] = true;
            procActive[Proc::Initialization] = false;
            Serial.println("Stopped process Initialization [local]");
            break;        
          }
        }
      }
      break;
    }
    case Proc::MainLoop: if (procActive[Proc::MainLoop]) {
      switch (currState[Proc::MainLoop]) {
        case MainLoopBegin: {
          Serial.println("Start process NextBottle [controller4]");
          message(Controller::controller4, Msg::startNextBottle);
          currState[Proc::MainLoop] = MainLoopWaitForNextBottle;
          break;
        }
        case MainLoopWaitForNextBottle: {
          if (!procActive[Proc::NextBottle]) {
            Serial.println("Start process BottleFilling [controller3]");
            message(Controller::controller3, startBottleFilling);
            currState[Proc::MainLoop] = MainLoopWaitForFilling;
            break;
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
              break;
            } else {
              //restart
              Serial.println("Restart process MainLoop [local]");
              currState[Proc::MainLoop] = MainLoopBegin;
              break;
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
            break;
        }
        case TankFillingTankFilled: {
          Serial.println("TankFilling:TankFilled");
          if (iHighLevel == ON) {
            oFillTank = OFF;
            Serial.println("oFillTank = OFF");
            procActive[Proc::TankFilling] = false;
            break;
          }
        }
      }
      break;
    }
  }

  SKIP:
  delay(1000);
  //RR strategy
  currProc = currProc + 1;
  if (currProc > 2) currProc = 0;  
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
