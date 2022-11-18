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

enum  Proc {ForcedSterilization=0, KeepSterilization, /* foreign */ };
enum  State {
    ForcedSterilizationHeatUp=0, ForcedSterilizationSterilizationFor1min, 
    KeepSterilizationWaitLowTemp, KeepSterilizationWaitHighTemp
};


Proc currProc;
State currState[] = {ForcedSterilizationHeatUp, KeepSterilizationWaitLowTemp}; //init states for each proccess
bool procActive[] = {false, false}; //running processes
int timeInState[] = {0, 0, 0, 0}; // for timeouts

bool oSteam = false;
bool iHighTemp = false;
bool iLowTemp = false;


#define ON true
#define OFF false

// the cs pin of the version after v1.1 is default to D9
// v0.9b and v1.0 is default D10
const int SPI_CS_PIN = 9; //! for this arduino
MCP_CAN CAN(SPI_CS_PIN);


void setup_CAN() {
  Serial.begin(115200);
  CAN.init_CS(SPI_CS_PIN);
  Serial.println("hi I am bottle controller2");
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
  currProc = Proc::ForcedSterilization;
  currState[Proc::ForcedSterilization] = ForcedSterilizationHeatUp;
}


//Arduino loop
void loop() {
  receive_messages();
  switch (currProc) {
    case Proc::ForcedSterilization: if (procActive[Proc::ForcedSterilization]) {
      switch (currState[Proc::ForcedSterilization]) {
        case ForcedSterilizationHeatUp: {
          Serial.println("ForcedSterilization:HeatUp");
          oSteam = ON;
          if (iHighTemp == ON) {
              currState[Proc::ForcedSterilization] = ForcedSterilizationSterilizationFor1min;              
              timeInState[State::ForcedSterilizationSterilizationFor1min] = 0;
          }
          contunue_loop();
        }
        case ForcedSterilizationSterilizationFor1min: {
          Serial.println("ForcedSterilization:SterilizationFor1min");
          timeInState[State::ForcedSterilizationSterilizationFor1min]++;
          if (timeInState == 60) {//timeout
            Serial.println("timeour for ForcedSterilization:SterilizationFor1min");
            timeInState[State::ForcedSterilizationSterilizationFor1min] = 0;
            oSteam = OFF;
            procActive[ForcedSterilization] = false;
          }
          contunue_loop();
        }
      }
    case Proc::KeepSterilization: if (procActive[Proc::KeepSterilization]) {
      switch (currState[Proc::KeepSterilization]) {
        case KeepSterilizationWaitLowTemp: {
          Serial.println("KeepSterilization:WaitLowTemp");
          if (iLowTemp != ON) {
            oSteam = ON;
            currState[Proc::KeepSterilization] = KeepSterilizationWaitHighTemp;
          }
          contunue_loop();
        }
        case KeepSterilizationWaitHighTemp: {
          Serial.println("KeepSterilization:WaitHighTemp");
          if (iHighTemp == ON) {
            oSteam = OFF;
            Serial.println("restarting");
            currState[Proc::KeepSterilization] = KeepSterilizationWaitLowTemp; //restart
          }
        }
      }
    }
   }
  }

  SKIP:
  delay(1000);
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
