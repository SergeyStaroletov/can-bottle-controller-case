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

enum  Proc {NextBottle=0, /* foreign */ };
enum  State {
    NextBottleBegin=0, NextBottleWaitBottlePosition
};


Proc currProc;
State currState[] = {NextBottleBegin}; //init states for each proccess
bool procActive[] = {false}; //running processes

bool oConveyor = false;
bool iBottlePosition = false;

#define ON true
#define OFF false

// the cs pin of the version after v1.1 is default to D9
// v0.9b and v1.0 is default D10
const int SPI_CS_PIN = 9; //! for this arduino
MCP_CAN CAN(SPI_CS_PIN);


void setup_CAN() {
  Serial.begin(115200);
  CAN.init_CS(SPI_CS_PIN);
  Serial.println("hi I am bottle controller4");
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
      if (canId == controller4 || canId == all) {
        if (buf[0] == startNextBottle) {
          procActive[Proc::NextBottle] = true;
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
  currProc = Proc::NextBottle;
  currState[Proc::NextBottle] = NextBottleBegin;
}


//Arduino loop
void loop() {
  receive_messages();
  switch (currProc) {
    case Proc::NextBottle: if (procActive[Proc::NextBottle]) {
      switch (currState[Proc::NextBottle]) {
        case NextBottleBegin: {
          Serial.println("NextBottle:Begin");
          oConveyor = ON;
          if (iBottlePosition != ON) {
            currState[Proc::NextBottle] = NextBottleWaitBottlePosition;
          }
          contunue_loop();
        }
        case NextBottleWaitBottlePosition: {
          Serial.println("NextBottle:WaitBottlePosition");
          if (iBottlePosition == ON) {
            oConveyor = OFF;
            Serial.println("oConveyor = OFF");
            procActive[Proc::NextBottle] = false;            
          }
          contunue_loop();
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
