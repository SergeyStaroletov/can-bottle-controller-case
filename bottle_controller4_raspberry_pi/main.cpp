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

// controllers
enum Controller {
  all,
  controller1,
  controller2,
  controller3,
  controller4,
  sim
};

enum Proc { NextBottle = 0, /* foreign */ };
enum State { NextBottleBegin = 0, NextBottleWaitBottlePosition };

Proc currProc;
State currState[] = {NextBottleBegin}; // init states for each proccess
bool procActive[] = {false};           // running processes

bool oConveyor = false;
bool iBottlePosition = false;

#define ON true
#define OFF false

class SerialClass {

public:

  void init() {  }

  void static println(QString s) { printf("%s\n", s.toLocal8Bit().constData()); }

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

        if (canId == sim && frame.payload().length() > 1) {
          if (frame.payload().at(0) == SimVars::vBottlePosition) {
            iBottlePosition = (frame.payload().at(1) > 0);
            Serial.print("[CAN sim msg] iBottlePosition is ");
            Serial.print(iBottlePosition);
          }
        }

        if (canId == Controller::controller4 || canId == Controller::all) {
          if (frame.payload().at(0) == Msg::startNextBottle) {
            Serial.println("[can] request to startNextBottle");
            procActive[Proc::NextBottle] = true;
            // notify
            message(Controller::all, Msg::activeNextBottle);
          }
        }
      }
    }
  }

  void run() {

    Serial.init();

    Serial.println("hi I am bottle controller4");
    setup_CAN();

    currProc = Proc::NextBottle;
    currState[Proc::NextBottle] = NextBottleBegin;

    while (true) {
      receive_messages();
      switch (currProc) {
      case Proc::NextBottle:
        if (procActive[Proc::NextBottle]) {
          switch (currState[Proc::NextBottle]) {
          case NextBottleBegin: {
            Serial.println("NextBottle:Begin");
            oConveyor = ON;
            Serial.println("oConveyor = ON");
            sim_message(SimVars::vConveyor, oConveyor);

            if (iBottlePosition != ON) {
              currState[Proc::NextBottle] = NextBottleWaitBottlePosition;
            }
            break;
          }
          case NextBottleWaitBottlePosition: {
            Serial.println("NextBottle:WaitBottlePosition");
            if (iBottlePosition == ON) {
              oConveyor = OFF;
              Serial.println("oConveyor = OFF");
              sim_message(SimVars::vConveyor, oConveyor);

              procActive[Proc::NextBottle] = false;
              // notify
              message(Controller::all, Msg::passiveNextBottle);
            }
            break;
          }
          }
        }
      }

      QThread::currentThread()->msleep(1000);
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
