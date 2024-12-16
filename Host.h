//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 1992-2015 Andras Varga
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//

#ifndef __ALOHA_HOST_H_
#define __ALOHA_HOST_H_

#include <omnetpp.h>

using namespace omnetpp;

namespace csma {

/**
 * CSMA host; see NED file for more info.
 */
class Host : public cSimpleModule, public cListener
{
  private:
    // parameters
    simtime_t radioDelay;
    double txRate;
    cPar *iaTime;
    cPar *pkLenBits;
    simtime_t slotTime;
    bool isSlotted;

    simtime_t unit_backoffTime;

    // state variables, event pointers etc
    cModule *server;
    cMessage *endTxEvent = nullptr;
    enum { IDLE = 0, TRANSMIT = 1, BACKOFF = 2 } state;
    simsignal_t stateSignal;
    simsignal_t channelStateSignal;
    int pkCounter;

    // position on the canvas, unit is m
    double x, y;

    // speed of light in m/s
    const double propagationSpeed = 299792458.0;

    // animation parameters
    const double ringMaxRadius = 2000; // in m
    const double circlesMaxRadius = 1000; // in m
    double idleAnimationSpeed;
    double transmissionEdgeAnimationSpeed;
    double midtransmissionAnimationSpeed;

    // figures and animation state
    cPacket *lastPacket = nullptr; // a copy of the last sent message, needed for animation
    mutable cRingFigure *transmissionRing = nullptr; // shows the last packet
    mutable std::vector<cOvalFigure *> transmissionCircles; // ripples inside the packet ring


    bool channelBusy;
    simtime_t backoffTime;
    int maxBackoffs;
    int backoffCount;
    cMessage *backoffTimer = nullptr;

    cPacket *pk;
    char pkname[40];

    int numOtherHosts;
    cModule *otherHosts[];
    cGate *otherHostGate[];
    double otherHostDelay[];
    
    cMessage *endListen = nullptr;

  public:
    virtual ~Host();

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void refreshDisplay() const override;
    simtime_t getNextTransmissionTime();
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, bool b, cObject *details) override;
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, intval_t i, cObject *details) override;
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, uintval_t i, cObject *details) override;
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, double d, cObject *details) override;
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, const SimTime &t, cObject *details) override;
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, const char *s, cObject *details) override;
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) override;
    void sendPacket(cPacket *pk);
};

}; //namespace

#endif

