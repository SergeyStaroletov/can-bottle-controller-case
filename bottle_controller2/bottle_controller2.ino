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

//processes needed for the current controller 
enum Proc {ForcedSterilization=0, KeepSterilization, /* foreign */} ;

//states
enum State {
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
        if (buf[0] == SimVars::vLowTemp) {
          iLowTemp = (buf[1] > 0);
          Serial.print("[CAN sim msg] LowTemp is ");
          Serial.println(iLowTemp);          
        }
        if (buf[0] == SimVars::vHighTemp) {
          iHighTemp = (buf[1] > 0);
          Serial.print("[CAN sim msg] iHighTemp is ");
          Serial.println(iHighTemp);          
        }
      }

      if (canId == controller2 || canId == all) {
        if (buf[0] == Msg::startForcedSterilization) {
          Serial.println("[can] request to startForcedSterilization");          
          procActive[Proc::ForcedSterilization] = true;
          //notify all about it
          message(Controller::all, Msg::activeForcedSterilization);
        }
        if (buf[0] == Msg::startKeepSterilization) {
          Serial.println("[can] request to startKeepSterilization");
          procActive[Proc::KeepSterilization] = true;
          //notify all about it
          message(Controller::all, Msg::activeKeepSterilization);          
        }
        if (buf[0] == Msg::stopKeepSterilization) {
          Serial.println("[can] request to stopKeepSterilization");
          procActive[Proc::KeepSterilization] = false;
          //notify all about it
          message(Controller::all, Msg::passiveKeepSterilization);           
        }
      }
    }
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
          Serial.println("oSteam = ON");
          oSteam = ON;
          sim_message(SimVars::vSteam, oSteam);
          if (iHighTemp == ON) {
              currState[Proc::ForcedSterilization] = ForcedSterilizationSterilizationFor1min;              
              timeInState[State::ForcedSterilizationSterilizationFor1min] = 0;
          }
          break;
        }
        case ForcedSterilizationSterilizationFor1min: {
          Serial.println("ForcedSterilization:SterilizationFor1min");
          timeInState[State::ForcedSterilizationSterilizationFor1min]++;
          if (timeInState[State::ForcedSterilizationSterilizationFor1min] == 60) {//timeout
            Serial.println("timeour for ForcedSterilization:SterilizationFor1min");
            timeInState[State::ForcedSterilizationSterilizationFor1min] = 0;
            Serial.println("oSteam = OFF");
            oSteam = OFF;
            sim_message(SimVars::vSteam, oSteam);
            procActive[Proc::ForcedSterilization] = false;
            //notify
            message(Controller::all, Msg::passiveForcedSterilization);
          }
        }
        break;
      }
    case Proc::KeepSterilization: if (procActive[Proc::KeepSterilization]) {
      switch (currState[Proc::KeepSterilization]) {
        case KeepSterilizationWaitLowTemp: {
          Serial.println("KeepSterilization:WaitLowTemp");
          if (iLowTemp != ON) {
            Serial.println("oSteam = ON");
            oSteam = ON;
            sim_message(SimVars::vSteam, oSteam);
            currState[Proc::KeepSterilization] = KeepSterilizationWaitHighTemp;
          }
          break;
        }
        case KeepSterilizationWaitHighTemp: {
          Serial.println("KeepSterilization:WaitHighTemp");
          if (iHighTemp == ON) {
            Serial.println("oSteam = OFF");
            oSteam = OFF;
            sim_message(SimVars::vSteam, oSteam);
            Serial.println("restarting");
            currState[Proc::KeepSterilization] = KeepSterilizationWaitLowTemp; //restart-go to the first state
          }
          break;
        }
      }
    }
   }
  }

  SKIP:
  delay(1000);

  //RR strategy
  currProc = currProc + 1;
  if (currProc > 1) currProc = 0;  
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
