//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 1992-2015 Andras Varga
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//

#include <algorithm>

#include "Host.h"
#include "Server.h"

namespace csma {

Define_Module(Host);

Host::~Host()
{
    delete lastPacket;
    cancelAndDelete(endTxEvent);
}

void Host::initialize()
{
    stateSignal = registerSignal("state");
    server = getModuleByPath("server");

    txRate = par("txRate");
    iaTime = &par("iaTime");
    pkLenBits = &par("pkLenBits");

    slotTime = par("slotTime");
    unit_backoffTime = par("unit_backoffTime");
    isSlotted = slotTime > 0;
    WATCH(slotTime);
    WATCH(isSlotted);

    endTxEvent = new cMessage("send/endTx");
    state = IDLE;
    emit(stateSignal, state);
    pkCounter = 0;
    WATCH((int&)state);
    WATCH(pkCounter);

    x = par("x").doubleValue();
    y = par("y").doubleValue();

    double serverX = server->par("x").doubleValue();
    double serverY = server->par("y").doubleValue();

    idleAnimationSpeed = par("idleAnimationSpeed");
    transmissionEdgeAnimationSpeed = par("transmissionEdgeAnimationSpeed");
    midtransmissionAnimationSpeed = par("midTransmissionAnimationSpeed");

    double dist = std::sqrt((x-serverX) * (x-serverX) + (y-serverY) * (y-serverY));
    radioDelay = dist / propagationSpeed;

    getDisplayString().setTagArg("p", 0, x);
    getDisplayString().setTagArg("p", 1, y);


    channelBusy = false;
    backoffTime = 0;
    maxBackoffs = 16;
    backoffCount = 0;
    backoffTimer = new cMessage("backoff");
    
    channelStateSignal = registerSignal("channelState");
    server->subscribe("channelState", this);

    numOtherHosts = getVectorSize() - 1;
    otherHostGate = new cGate *[numOtherHosts];
    otherHosts = new cModule *[numOtherHosts];
    int index = 0;
    for (int i = 0; i < numOtherHosts + 1; i++) {
        if (i == getIndex()) {
            continue;
        }
        otherHosts[index] = getModuleByPath("host[%d]", i);
        otherHostGate[index] = otherHosts[index]->gate("in");


        double dist = std::sqrt((x-otherHosts[index]->par("x").doubleValue()) * \
                (x-otherHosts[index]->par("x").doubleValue()) + \
                (y-otherHosts[index]->par("y").doubleValue()) * \
                (y-otherHosts[index]->par("y").doubleValue()));
        otherHostDelay[index] = dist / propagationSpeed;
        index++;
    }

    scheduleAt(getNextTransmissionTime(), endTxEvent);
}

void Host::handleMessage(cMessage *msg)
{
     ASSERT(msg == endTxEvent || msg == backoffTimer);

    if (hasGUI())
        getParentModule()->getCanvas()->setAnimationSpeed(transmissionEdgeAnimationSpeed, this);

    if (msg == backoffTimer) {
        if (!channelBusy) {
                EV << "send packet " << pkname << endl;
                state = TRANSMIT;
                emit(stateSignal, state);
                
                simtime_t duration = pk->getBitLength() / txRate;
                sendDirect(pk, radioDelay, duration, server->gate("in"));

                scheduleAt(simTime()+duration, endTxEvent);

                backoffCount = 0;
                backoffTime = 0;

                // let visualization code know about the new packet
                if (transmissionRing != nullptr) {
                    delete lastPacket;
                    transmissionRing->setVisible(false);
                    transmissionRing->setAssociatedObject(nullptr);

                    for (auto c : transmissionCircles) {
                        c->setVisible(false);
                        c->setAssociatedObject(nullptr);
                    }

                    lastPacket = pk->dup();
                }
        } else {
            EV << "backoff packet " << pkname << endl;
            state = BACKOFF;
            emit(stateSignal, state);
            backoffCount++;
            backoffTime = pow(2, backoffCount) * unit_backoffTime;
            scheduleAt(simTime() + backoffTime, backoffTimer);
        }
    }
    else {
        if (state == IDLE) {
            // generate packet
            snprintf(pkname, sizeof(pkname), "pk-%d-#%d", getId(), pkCounter++);
            EV << "generating packet " << pkname << endl;
            pk = new cPacket(pkname);
            pk->setBitLength(pkLenBits->intValue());

            if (!channelBusy) {
                EV << "send packet " << pkname << endl;
                state = TRANSMIT;
                emit(stateSignal, state);

                // schedule timer when it ends
                simtime_t duration = pk->getBitLength() / txRate;
                sendDirect(pk, radioDelay, duration, server->gate("in"));

                scheduleAt(simTime()+duration, endTxEvent);

                backoffCount = 0;
                backoffTime = 0;

                // let visualization code know about the new packet
                if (transmissionRing != nullptr) {
                    delete lastPacket;
                    transmissionRing->setVisible(false);
                    transmissionRing->setAssociatedObject(nullptr);

                    for (auto c : transmissionCircles) {
                        c->setVisible(false);
                        c->setAssociatedObject(nullptr);
                    }

                    lastPacket = pk->dup();
                }
            }
            else {
                EV << "backoff packet " << pkname << endl;
                state = BACKOFF;
                emit(stateSignal, state);
                backoffCount++;
                backoffTime = pow(2, backoffCount) * unit_backoffTime;
                scheduleAt(simTime() + backoffTime, backoffTimer);
            }
        }
        else if (state == TRANSMIT) {
            // endTxEvent indicates end of transmission
            state = IDLE;
            emit(stateSignal, state);

            // schedule next sending
            scheduleAt(getNextTransmissionTime(), endTxEvent);
        }
        else {
            throw cRuntimeError("invalid state");
        }
    }
}

simtime_t Host::getNextTransmissionTime()
{
    simtime_t t = simTime() + iaTime->doubleValue();

    if (!isSlotted)
        return t;
    else
        // align time of next transmission to a slot boundary
        return slotTime * ceil(t/slotTime);
}

void Host::refreshDisplay() const
{
    cCanvas *canvas = getParentModule()->getCanvas();
    const int numCircles = 20;
    const double circleLineWidth = 10;

    // create figures on our first invocation
    if (!transmissionRing) {
        auto color = cFigure::GOOD_DARK_COLORS[getId() % cFigure::NUM_GOOD_DARK_COLORS];

        transmissionRing = new cRingFigure(("Host" + std::to_string(getIndex()) + "Ring").c_str());
        transmissionRing->setOutlined(false);
        transmissionRing->setFillColor(color);
        transmissionRing->setFillOpacity(0.25);
        transmissionRing->setFilled(true);
        transmissionRing->setVisible(false);
        transmissionRing->setZIndex(-1);
        canvas->addFigure(transmissionRing);

        for (int i = 0; i < numCircles; ++i) {
            auto circle = new cOvalFigure(("Host" + std::to_string(getIndex()) + "Circle" + std::to_string(i)).c_str());
            circle->setFilled(false);
            circle->setLineColor(color);
            circle->setLineOpacity(0.75);
            circle->setLineWidth(circleLineWidth);
            circle->setZoomLineWidth(true);
            circle->setVisible(false);
            circle->setZIndex(-0.5);
            transmissionCircles.push_back(circle);
            canvas->addFigure(circle);
        }
    }

    if (lastPacket) {
        // update transmission ring and circles
        if (transmissionRing->getAssociatedObject() != lastPacket) {
            transmissionRing->setAssociatedObject(lastPacket);
            for (auto c : transmissionCircles)
                c->setAssociatedObject(lastPacket);
        }

        simtime_t now = simTime();
        simtime_t frontTravelTime = now - lastPacket->getSendingTime();
        simtime_t backTravelTime = now - (lastPacket->getSendingTime() + lastPacket->getDuration());

        // conversion from time to distance in m using speed
        double frontRadius = std::min(ringMaxRadius, frontTravelTime.dbl() * propagationSpeed);
        double backRadius = backTravelTime.dbl() * propagationSpeed;
        double circleRadiusIncrement = circlesMaxRadius / numCircles;

        // update transmission ring geometry and visibility/opacity
        double opacity = 1.0;
        if (backRadius > ringMaxRadius) {
            transmissionRing->setVisible(false);
            transmissionRing->setAssociatedObject(nullptr);

            for (auto c : transmissionCircles) {
                c->setVisible(false);
                c->setAssociatedObject(nullptr);
            }
        }
        else {
            transmissionRing->setVisible(true);
            transmissionRing->setBounds(cFigure::Rectangle(x - frontRadius, y - frontRadius, 2*frontRadius, 2*frontRadius));
            transmissionRing->setInnerRadius(std::max(0.0, std::min(ringMaxRadius, backRadius)));
            if (backRadius > 0)
                opacity = std::max(0.0, 1.0 - backRadius / circlesMaxRadius);
        }

        transmissionRing->setLineOpacity(opacity);
        transmissionRing->setFillOpacity(opacity/5);

        // update transmission circles geometry and visibility/opacity
        double radius0 = std::fmod(frontTravelTime.dbl() * propagationSpeed, circleRadiusIncrement);
        for (int i = 0; i < (int)transmissionCircles.size(); ++i) {
            double circleRadius = std::min(ringMaxRadius, radius0 + i * circleRadiusIncrement);
            if (circleRadius < frontRadius - circleRadiusIncrement/2 && circleRadius > backRadius + circleLineWidth/2) {
                transmissionCircles[i]->setVisible(true);
                transmissionCircles[i]->setBounds(cFigure::Rectangle(x - circleRadius, y - circleRadius, 2*circleRadius, 2*circleRadius));
                transmissionCircles[i]->setLineOpacity(std::max(0.0, 0.2 - 0.2 * (circleRadius / circlesMaxRadius)));
            }
            else
                transmissionCircles[i]->setVisible(false);
        }

        // compute animation speed
        double animSpeed = idleAnimationSpeed;
        if ((frontRadius >= 0 && frontRadius < circlesMaxRadius) || (backRadius >= 0 && backRadius < circlesMaxRadius))
            animSpeed = transmissionEdgeAnimationSpeed;
        if (frontRadius > circlesMaxRadius && backRadius < 0)
            animSpeed = midtransmissionAnimationSpeed;
        if (hasGUI())
            canvas->setAnimationSpeed(animSpeed, this);
    }
    else {
        // hide transmission rings, update animation speed
        if (transmissionRing->getAssociatedObject() != nullptr) {
            transmissionRing->setVisible(false);
            transmissionRing->setAssociatedObject(nullptr);

            for (auto c : transmissionCircles) {
                c->setVisible(false);
                c->setAssociatedObject(nullptr);
            }
            if (hasGUI())
                canvas->setAnimationSpeed(idleAnimationSpeed, this);
        }
    }

    // update host appearance (color and text)
    getDisplayString().setTagArg("t", 2, "#808000");
    if (state == IDLE) {
        getDisplayString().setTagArg("i", 1, "");
        getDisplayString().setTagArg("t", 0, "");
    }
    else if (state == TRANSMIT) {
        getDisplayString().setTagArg("i", 1, "yellow");
        getDisplayString().setTagArg("t", 0, "TRANSMIT");
    }
}

void Host::receiveSignal(cComponent *source, simsignal_t signalID, bool b, cObject *details)
{
}

void Host::receiveSignal(cComponent *source, simsignal_t signalID, intval_t i, cObject *details)
{
//    EV << "host listen long" << endl;
    if (signalID == channelStateSignal) {
        if (i > 0) {
            channelBusy = true;
        } else {
            channelBusy = false;
        }
    }
}

void Host::receiveSignal(cComponent *source, simsignal_t signalID, uintval_t i, cObject *details){}
void Host::receiveSignal(cComponent *source, simsignal_t signalID, double d, cObject *details){}
void Host::receiveSignal(cComponent *source, simsignal_t signalID, const SimTime &t, cObject *details){}
void Host::receiveSignal(cComponent *source, simsignal_t signalID, const char *s, cObject *details){}
void Host::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details){}

void Host::sendPacket(cPacket *pk) {
    // EV << "send packet " << pkname << endl;
    // state = TRANSMIT;
    // emit(stateSignal, state);
    
    // simtime_t duration = pk->getBitLength() / txRate;
    // sendDirect(pk, radioDelay, duration, server->gate("in"));

    // for (int i = 0; i < numOtherHosts; i++) {
    //     sendDirect(pk, otherHostDelay[i], pk->getBitLength() / txRate, otherHostGate[i]);
    // }

    // scheduleAt(simTime()+duration, endTxEvent);

    // backoffCount = 0;
    // backoffTime = 0;

    // // let visualization code know about the new packet
    // if (transmissionRing != nullptr) {
    //     delete lastPacket;
    //     transmissionRing->setVisible(false);
    //     transmissionRing->setAssociatedObject(nullptr);

    //     for (auto c : transmissionCircles) {
    //         c->setVisible(false);
    //         c->setAssociatedObject(nullptr);
    //     }

    //     lastPacket = pk->dup();
    // }
}

}; //namespace
