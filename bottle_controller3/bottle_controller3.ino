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

enum SimVars{
  vLowLevel,
  vHighLevel,
  vFillTank,
  vLowTemp,
  vHighTemp,
  vSteam,
  vBottleLevel,
  vFillBottle,
  vBottlePosition,
  vConveyor,
  vSetBottle
};

//controllers
enum Controller {all, controller1, controller2, controller3, controller4, sim};

enum Proc {BottleFilling=0, /* foreign */ };
enum State {
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
      Serial.println("CAN BUS Shield init ok");
    } else {
      Serial.println("CAN BUS Shield init fail");
      Serial.println("Init CAN BUS Shield again");
      delay(1000);
      goto START_INIT;
    }
  Serial.println("CAN ready!");
}


void message(int to, int type) {
  byte buf[1];
  buf[0] = type;
  CAN.sendMsgBuf((unsigned long)to, 0, sizeof(buf), buf, true);
}

void sim_message(int type, bool val) {
  byte buf[2];
  buf[0] = type;
  buf[1] = (byte)val;
  int to = sim;
  CAN.sendMsgBuf((unsigned long)to, 0, sizeof(buf), buf, true);
}



void receive_messages() {
  byte buf[8];
  byte len;
  while (CAN_MSGAVAIL == CAN.checkReceive())  // check if data is coming
    {
      CAN.readMsgBuf(&len, buf);            
      unsigned short canId = CAN.getCanId();

      if (canId == sim && len > 1) {
        if (buf[0] == SimVars::vBottleLevel) {
          iBottleLevel = (buf[1] > 0);
          Serial.print("[CAN sim msg] iBottleLevel is ");
          Serial.print(iBottleLevel);          
        }
      }

      if (canId == Controller::controller3 || canId == Controller::all) {
        if (buf[0] == Msg::startBottleFilling) {
          Serial.println("[can] request to startBottleFilling");
          procActive[Proc::BottleFilling] = true;
          //message?
        }
      }
    }
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
          Serial.println("oFillBottle = ON");
          sim_message(SimVars::vFillBottle, oFillBottle);

          if (iBottleLevel == ON) {
              iBottleLevel = OFF;
              sim_message(SimVars::vBottleLevel, iBottleLevel);
              Serial.println("iBottleLevel = OFF");
              procActive[Proc::BottleFilling] = false;
              message(Controller::all, Msg::passiveBottleFilling);
          }
          break;
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
