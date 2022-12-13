#include <QCanBus>
#include <QCanBusDevice>
#include <QCanBusFrame>
#include <QCoreApplication>
#include <QDebug>
#include <QList>
#include <QThread>
#include <stdio.h>

// messages
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

// controllers
enum Controller {
  all,
  controller1,
  controller2,
  controller3,
  controller4,
  sim
};

enum Proc {
  Initialization = 0,
  TankSim,
  TempSim,
  SetBottle,
  BottleFillingSim,
  ConveyorSim
};
enum State {
  InitializationBegin,
  TankSimBegin,
  TempSimBegin,
  SetBottleBegin,
  SetBottleCheckRisingEdge,
  BottleFillingSimBegin,
  ConveyorSimBegin
};

int currProc;
State currState[] = {
    InitializationBegin,   TankSimBegin,    TempSimBegin, SetBottleBegin,
    BottleFillingSimBegin, ConveyorSimBegin}; // init states for each proccess
bool procActive[] = {true,  false, false,
                     false, false, false}; // running processes

enum SimVars {
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

const bool ON = true;
const bool OFF = false;

/*===== P1 VARIABLES =====*/
bool iLowLevel;
bool iHighLevel;
bool oFillTank;

/*===== P2 VARIABLES =====*/
bool iLowTemp;
bool iHighTemp;
bool oSteam;

/*===== P3 VARIABLES =====*/
bool iBottleLevel;
bool oFillBottle;

/*===== P4 VARIABLES =====*/
bool iBottlePosition;
bool oConveyor;

/*===== PLANT VARIABLES =====*/
bool iSetBottle;

float TankLevel;
float TankTemp;
float BottleCoord;
float BottleLevel;

#define ON true
#define OFF false

const float INFLOW_RATE = 1.0;
const float OUTFLOW_RATE = 0.01;
const float TANK_LOW_LEVEL = 10.0;
const float TANK_HIGH_LEVEL = 100.0;
const float TANK_MAX_LEVEL = 120.0;

const float HEATING_RATE = 0.01;
const float COOLING_RATE = 0.05;
const float ROOM_TEMP = 20.0;
const float STEAM_TEMP = 200.0;
const float LOW_TEMP = 95.0;
const float HIGH_TEMP = 110.0;

const float FILLING_RATE = 1.0;
const float BOTTLE_FULL_LEVEL = 20.0;
const float BOTTLE_MAX_LEVEL = 20.0;
const float MIN_UNDER_NOZZLE = 145.0;
const float MAX_UNDER_NOZZLE = 150.0;

const float CONVEYOR_RATE = 1.0;
const float CONVEYOR_LENGTH = 160.0;

bool Prev;

class SerialClass {

public:
  void init() {}
  void static println(QString s) {
    printf("%s\n", s.toLocal8Bit().constData());
  }
  void static println(int s) { printf("%d\n", s); }
  void static print(int s) { printf("%d", s); }
  void static print(QString s) { printf("%s", s.toLocal8Bit().constData()); }
};

SerialClass Serial;



class ControllerThread : public QThread {

  QCanBusDevice *device;

  void setup_CAN() {

    if (QCanBus::instance()->plugins().contains(QStringLiteral("socketcan"))) {
      QString errorString;

      device = QCanBus::instance()->createDevice(
          QStringLiteral("socketcan"), QStringLiteral("can0"), &errorString);

      if (!device) {
        Serial.println(errorString);
        exit(1);
      }

      device->setConfigurationParameter(QCanBusDevice::BitRateKey,
                                        QVariant()); // fix
      if (!device->connectDevice()) {
        Serial.println("CAN device is not connected !");
        exit(1);
      }

    } else {
      Serial.println("no Socket CAN");
      exit(1);
    }

    Serial.println("CAN ready!");
  }


  void message(int to, int type) {
    QCanBusFrame newFrame;
    newFrame.setFrameId(to);
    QByteArray arr;
    arr.append(type);
    newFrame.setPayload(arr);
    newFrame.setFrameType(QCanBusFrame::DataFrame);
    device->writeFrame(newFrame); // id=to, type
  }


  void sim_message(int type, bool val) {
    QCanBusFrame newFrame;
    newFrame.setFrameId(sim);
    QByteArray arr;
    arr.append(type);
    arr.append(val);
    newFrame.setPayload(arr);
    newFrame.setFrameType(QCanBusFrame::DataFrame);
    device->writeFrame(newFrame); // id=sim, type, val
  }


  void receive_messages() {

    if (device->waitForFramesReceived(0)) { // check if data is coming
      QVector<QCanBusFrame> frames = device->readAllFrames();

      for (auto frame : frames) {
        unsigned short canId = frame.frameId();

        // gremlin for iSetBottle -- todo: by pressing a real button
        if (rand() % 10 == 5) {
          Serial.println("SetBottle is pressed!");
          iSetBottle = ON;
        } else
          iSetBottle = OFF;

        // Handling plant simulation messages
        if (canId == sim) {
          if (frame.payload().at(0) == SimVars::vFillTank) {
            oFillTank = (frame.payload().at(1) > 0);
            Serial.print("[CAN sim msg] oFillTank is ");
            Serial.println(oFillTank);
          }
          if (frame.payload().at(0) == SimVars::vFillBottle) {
            oFillBottle = (frame.payload().at(1) > 0);
            Serial.print("[CAN sim msg] oFillBottle is ");
            Serial.println(oFillBottle);
          }
          if (frame.payload().at(0) == SimVars::vSteam) {
            oSteam = (frame.payload().at(1) > 0);
            Serial.print("[CAN sim msg] oSteam is ");
            Serial.println(oSteam);
          }
          if (frame.payload().at(0) == SimVars::vConveyor) {
            oConveyor = (frame.payload().at(1) > 0);
            Serial.print("[CAN sim msg] oConveyor is ");
            Serial.println(oConveyor);
          }
        }
      }
    }
  }


  void run() {
    Serial.init();
    Serial.println("hi I am a plant simulator!");
    setup_CAN();

    currProc = Proc::Initialization;
    currState[Proc::Initialization] = InitializationBegin;

    while (true) {
      receive_messages();
      switch (currProc) {
      case Proc::Initialization:
        if (procActive[Proc::Initialization]) {
          switch (currState[Proc::Initialization]) {
          case InitializationBegin: {
            Serial.println("Initialization:Begin");
            TankLevel = 0.0;
            TankTemp = 20.0;
            BottleCoord = 0.0;
            BottleLevel = 0.0;
            Serial.println("start process TankSim");
            procActive[Proc::TankSim] = true;
            Serial.println("start process TempSim");
            procActive[Proc::TempSim] = true;
            Serial.println("start process BottleFillingSim");
            procActive[Proc::BottleFillingSim] = true;
            Serial.println("start process SetBottle");
            procActive[Proc::SetBottle] = true;
            Serial.println("start process ConveyorSim");
            procActive[Proc::ConveyorSim] = true;
            Serial.println("Initialization done.");
            procActive[Proc::Initialization] = false;
          }
          }
        }
      case Proc::TankSim:
        if (procActive[Proc::TankSim]) {
          switch (currState[Proc::TankSim]) {
          case TankSimBegin: {
            if (oFillTank == ON)
              TankLevel += INFLOW_RATE;
            if (oFillBottle == ON)
              TankLevel -= OUTFLOW_RATE;
            if (TankLevel >= TANK_HIGH_LEVEL) {
              iHighLevel = ON;
              sim_message(SimVars::vHighLevel, iHighLevel);
              if (TankLevel >= TANK_MAX_LEVEL)
                TankLevel = TANK_MAX_LEVEL;
            } else {
              iHighLevel = OFF;
              sim_message(SimVars::vHighLevel, iHighLevel);
            }
            if (TankLevel >= TANK_LOW_LEVEL) {
              iLowLevel = ON;
              sim_message(SimVars::vLowLevel, iLowLevel);
            } else {
              iLowLevel = OFF;
              sim_message(SimVars::vLowLevel, iLowLevel);
              if (TankLevel < 0.0)
                TankLevel = 0.0;
            }
            Serial.print("TankLevel:");
            Serial.println(TankLevel);
          }
          }
        }
      case Proc::TempSim:
        if (procActive[Proc::TempSim]) {
          switch (currState[Proc::TempSim]) {
          case TempSimBegin: {
            if (oFillTank == ON)
              TankTemp = (TankLevel * TankTemp + INFLOW_RATE * ROOM_TEMP) /
                         (TankLevel + INFLOW_RATE);
            if (oSteam == ON)
              TankTemp = TankTemp + (STEAM_TEMP - TankTemp) * HEATING_RATE;
            else
              TankTemp = TankTemp - (TankTemp - ROOM_TEMP) * COOLING_RATE;
            if (TankTemp >= HIGH_TEMP) {
              iHighTemp = ON;
              Serial.println("iHighTemp = ON");
              sim_message(SimVars::vHighTemp, iHighTemp);
            } else {
              iHighTemp = OFF;
              Serial.println("iHighTemp = OFF");
              sim_message(SimVars::vHighTemp, iHighTemp);
            }
            if (TankTemp >= LOW_TEMP) {
              iLowTemp = ON;
              Serial.println("iLowTemp = ON");
              sim_message(SimVars::vLowTemp, iLowTemp);
            } else {
              iLowTemp = OFF;
              Serial.println("iLowTemp = OFF");
              sim_message(SimVars::vLowTemp, iLowTemp);
            }
            Serial.print("TankTemp:");
            Serial.println(TankTemp);
          }
          }
        }
      case Proc::SetBottle:
        if (procActive[Proc::SetBottle]) {
          switch (currState[Proc::SetBottle]) {
          case SetBottleBegin: {
            Prev = iSetBottle;
            currState[Proc::SetBottle] = SetBottleCheckRisingEdge;
            break;
          }
          case SetBottleCheckRisingEdge: {
            if ((iSetBottle == ON) && (Prev == OFF)) {
              Serial.println("SetBottle: iSetBottle OFF->ON");
              Prev = iSetBottle;
              if (oConveyor) {
                if (BottleCoord == 0.0) {
                  BottleCoord = MIN_UNDER_NOZZLE;
                  Serial.println("BottleCoord = MIN_UNDER_NOZZLE");
                  BottleLevel = 0.0;
                  Serial.println("BottleLevel = 0");
                }
              }
            }
          }
          }
        }
      case Proc::BottleFillingSim:
        if (procActive[Proc::BottleFillingSim]) {
          switch (currState[Proc::BottleFillingSim]) {
          case BottleFillingSimBegin: {
            iBottleLevel = OFF;
            Serial.println("iBottleLevel = OFF");
            sim_message(SimVars::vBottleLevel, iBottleLevel);
            if ((BottleCoord >= MIN_UNDER_NOZZLE) &&
                (BottleCoord < MAX_UNDER_NOZZLE)) {
              Serial.println(
                  "BottleCoord is in [MIN_UNDER_NOZZLE, MAX_UNDER_NOZZLE)");
              if (oFillBottle == ON) {
                BottleLevel += FILLING_RATE;
                Serial.print("oFillBottle active. BottleLevel: ");
                Serial.println(BottleLevel);
              }
              if (BottleLevel >= BOTTLE_FULL_LEVEL) {
                iBottleLevel = ON;
                Serial.println("iBottleLevel = ON");
                sim_message(SimVars::vBottleLevel, iBottleLevel);
              }
            }
          }
          }
        }
      case Proc::ConveyorSim:
        if (procActive[Proc::ConveyorSim]) {
          switch (currState[Proc::ConveyorSim]) {
          case ConveyorSimBegin: {
            if (oConveyor == ON) {
              iBottlePosition = OFF;
              sim_message(SimVars::vBottlePosition, iBottlePosition);
              Serial.println("iBottlePosition = OFF");
              if (BottleCoord > 0.0) {
                BottleCoord += CONVEYOR_RATE;
                if ((BottleCoord >= MIN_UNDER_NOZZLE) &&
                    (BottleCoord < MAX_UNDER_NOZZLE)) {
                  Serial.println(
                      "BottleCoord is in [MIN_UNDER_NOZZLE, MAX_UNDER_NOZZLE)");
                  iBottlePosition = ON;
                  sim_message(SimVars::vBottlePosition, iBottlePosition);
                  Serial.println("iBottlePosition = ON");
                }
                if (BottleCoord >= CONVEYOR_LENGTH) {
                  BottleCoord = 0.0;
                  BottleLevel = 0.0;
                  Serial.println("BottleCoord = 0; BottleLevel = 0");
                } else
                  Serial.print("BottleCoord: ");
                Serial.println(BottleCoord);
              }
            }
          }
          }
        }
        break;
      }

      QThread::currentThread()->msleep(1000);
      // RR strategy
      currProc = currProc + 1;
      if (currProc > 5)
        currProc = 0;
    }
  }
};

int main(int argc, char *argv[]) {
  QCoreApplication a(argc, argv);

  // we need it to use event loop for can
  ControllerThread thread;
  thread.start();

  return a.exec();
}
