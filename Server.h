//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 1992-2015 Andras Varga
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//

#ifndef __ALOHA_SERVER_H_
#define __ALOHA_SERVER_H_

#include <omnetpp.h>

using namespace omnetpp;

namespace csma {

/**
 * CSMA server; see NED file for more info.
 */
class Server : public cSimpleModule
{
  private:
    // state variables, event pointers
    bool channelBusy;
    cMessage *endRxEvent = nullptr;

    intval_t currentCollisionNumFrames;
    intval_t receiveCounter;
    simtime_t recvStartTime;
    enum { IDLE = 0, TRANSMISSION = 1, COLLISION = 2 };
    simsignal_t channelStateSignal;

    // statistics
    simsignal_t receiveBeginSignal;
    simsignal_t receiveSignal;
    simsignal_t collisionLengthSignal;
    simsignal_t collisionSignal;

    simtime_t SIFS;
    simtime_t CTS_TIME;

    char RTS[4] = "RTS";

    const double propagationSpeed = 299792458.0;
    double x, y;

    int numHosts;
    cModule **Hosts;
    cGate **HostGate;
    simtime_t *HostDelay;

    cMessage *CTS;
    bool CTS_flag;
    int CTS_direction;
    cMessage *CTS_UNFREEZE;
    bool CTS_FREEZE_flag;

  public:
    virtual ~Server();

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    virtual void refreshDisplay() const override;
};

}; //namespace

#endif

