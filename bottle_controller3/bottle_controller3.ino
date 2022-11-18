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

enum  Proc {BottleFilling=0, /* foreign */ };
enum  State {
    BottleFillingBegin=0
};


Proc currProc;
State currState[] = {BottleFillingBegin}; //init states for each proccess
bool procActive[] = {false}; //running processes

bool oFillBottle = false;
bool iBottleLevel = false;

#define ON true
#define OFF false

// the cs pin of the version after v1.1 is default to D9
// v0.9b and v1.0 is default D10
const int SPI_CS_PIN = 9; //! for this arduino
MCP_CAN CAN(SPI_CS_PIN);


void setup_CAN() {
  Serial.begin(115200);
  CAN.init_CS(SPI_CS_PIN);
  Serial.println("hi I am bottle controller3");
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
      if (canId == controller3 || canId == all) {
        if (buf[0] == startBottleFilling) {
          procActive[Proc::BottleFilling] = true;
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
  currProc = Proc::BottleFilling;
  currState[Proc::BottleFilling] = BottleFillingBegin;
}


//Arduino loop
void loop() {
  receive_messages();
  switch (currProc) {
    case Proc::BottleFilling: if (procActive[Proc::BottleFilling]) {
      switch (currState[Proc::BottleFilling]) {
        case BottleFillingBegin: {
          Serial.println("BottleFilling:Begin");
          oFillBottle = ON;
          if (iBottleLevel == ON) {
              iBottleLevel = OFF;
              Serial.println("iBottleLevel = OFF");
              procActive[Proc::BottleFilling] = false;
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
