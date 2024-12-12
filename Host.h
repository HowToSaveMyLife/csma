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

namespace aloha {

/**
 * Aloha host; see NED file for more info.
 */
class Host : public cSimpleModule
{
  private:
    // parameters
    simtime_t radioDelay;
    double txRate;
    cPar *iaTime;
    cPar *pkLenBits;
    simtime_t slotTime;
    bool isSlotted;

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

    // CSMA 相关参数
    bool channelBusy;  // 信道是否忙
    double backoffTime;  // 退避时间
    int maxBackoffs;     // 最大退避次数
    int backoffCount;    // 当前退避次数
    cMessage *backoffTimer;  // 退避定时器

    // 发送的包
    cPacket *pk;

  public:
    virtual ~Host();

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void refreshDisplay() const override;
    simtime_t getNextTransmissionTime();
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, bool busy, cObject *details);
};

}; //namespace

#endif

