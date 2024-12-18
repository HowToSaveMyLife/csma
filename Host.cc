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
    gate("in")->setDeliverImmediately(true);

    stateSignal = registerSignal("state");
    server = getModuleByPath("server");

    txRate = par("txRate");
    iaTime = &par("iaTime");
    pkLenBits = &par("pkLenBits");

    slotTime = par("slotTime");
    WATCH(slotTime);

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


    channelBusy = 0;
    backoffTime = 0;
    maxBackoffs = par("maxBackoffs");
    backoffCount = 0;
    backoff = new cMessage("backoff");
    
    channelStateSignal = registerSignal("channelState");
    server->subscribe("channelState", this);

    numOtherHosts = getVectorSize() - 1;
    otherHostGate = new cGate *[numOtherHosts];
    otherHosts = new cModule *[numOtherHosts];
    otherHostDelay = new simtime_t [numOtherHosts];
    char hostname[10] = "host[x]";
    int index = 0;
    for (int i = 0; i < numOtherHosts + 1; i++) {
        if (i != getIndex()) {
            snprintf(hostname, sizeof(hostname), "host[%d]", i);
            otherHosts[index] = getModuleByPath(hostname);
            otherHostGate[index] = otherHosts[index]->gate("in");

            double dist = std::sqrt((x-otherHosts[index]->par("x").doubleValue()) * \
                    (x-otherHosts[index]->par("x").doubleValue()) + \
                    (y-otherHosts[index]->par("y").doubleValue()) * \
                    (y-otherHosts[index]->par("y").doubleValue()));
            otherHostDelay[index] = dist / propagationSpeed;
            index++;
        }
    }

    DIFSEvent = new cMessage("DIFSEvent");
    DIFS = par("DIFS");
    DIFS_FLAG = 0;
    RTS_TIME = par("RTS");
    SIFS = par("SIFS");

    RTSEvent = new cMessage("RTSEvent");

    scheduleAt(getNextTransmissionTime(), DIFSEvent);
}

void Host::handleMessage(cMessage *msg)
{
    //  ASSERT(msg == endTxEvent || msg == backoff || endListen);

    if (hasGUI() && msg == endTxEvent)
        getParentModule()->getCanvas()->setAnimationSpeed(transmissionEdgeAnimationSpeed, this);

    if (msg == DIFSEvent) {
        if (channelBusy == 0) {
            if (state == IDLE) {
                // while channel is free and state is idle, wait for DIFS
                scheduleAt(simTime() + DIFS, RTSEvent);
            } else if (state == FREEZE) {
                // maybe this part is not needed, its implementation is below
                scheduleAt(simTime() + DIFS + backoffTime, RTSEvent);
                DIFS_FLAG = simTime();
            }
        } else {
            scheduleAt(simTime(), backoff);
        }
    } else if (msg == RTSEvent){
        sendRTS();
    } else if (msg == endTxEvent) {
        if (state == BEFORE_SNED) {
            // generate packet
            snprintf(pkname, sizeof(pkname), "pk-%d-#%d", getIndex(), pkCounter++);
            EV << "generating packet " << pkname << endl;
            pk = new cPacket(pkname);
            pk->setBitLength(pkLenBits->intValue());

            sendPacket(pk);
        } else if (state == TRANSMIT) {
            // endTxEvent indicates end of transmission
            state = IDLE;
            emit(stateSignal, state);

            // schedule next sending
            scheduleAt(getNextTransmissionTime(), DIFSEvent);

            // send to other hosts the finish signal
            for (int i = 0; i < numOtherHosts; i++) {
                endListen = new cMessage("endListen");
                sendDirect(endListen, otherHostDelay[i], 0, otherHostGate[i]);
            }
        } else {
            throw cRuntimeError("invalid state");
        }
    } else if (msg == backoff) {
        // only compute backofftime
        EV << "host " << getIndex() << " backoff\n";
        state = FREEZE;
        if (backoffCount < maxBackoffs) {
            backoffCount += 1;
        }
        backoffTime = generateBackofftime();
    } else {
        if (strcmp(msg->getName(), endListenName) == 0) {
            // EV << "finish receive other host\n";
            channelBusy--;
            delete msg;

            if (state == FREEZE && channelBusy == 0) {
                // At begin, this part is schedule a DIFSEvent, then execute below code
                // Now, this part is executed directly
                scheduleAt(simTime() + DIFS + backoffTime, RTSEvent);
                DIFS_FLAG = simTime();
            }
        } else if (strcmp(msg->getName(), "CTS") == 0) {
            if (state == WAIT_CTS) {
                // confirm that the CTS is received
                cPacket *pkt = check_and_cast<cPacket *>(msg);
                state = BEFORE_SNED;
                cancelEvent(backoff);
                scheduleAt(simTime() + SIFS + pkt->getDuration(), endTxEvent);
                delete pkt;
            } else {
                delete msg;
            }
        } else if (strcmp(msg->getName(), "RTS") == 0) {
            if (state == FREEZE) {
                // contention period
                EV << "host " << getIndex() << " content fail and freeze\n";
                // while RTS collision, don't receive CTS, then create a new backoffTime below
                // then next other RTS arrive, backoffTime is not changed
                if (backoffTime - (simTime() - DIFS_FLAG - DIFS) > 0) {
                    backoffTime = backoffTime - (simTime() - DIFS_FLAG - DIFS);
                }
                cancelEvent(RTSEvent);
            }
            delete msg;
        }
        else {
            cMessage *pkt = check_and_cast<cMessage *>(msg);

            ASSERT(pkt->isReceptionStart());

            if (state == WAIT_CTS) {
                // channel becomes busy while waiting for DIFS
                // backoff
                EV << "host " << getIndex() << " backoff while waiting for DIFS\n";
                scheduleAt(simTime(), backoff);
                cancelEvent(endTxEvent);
            }

            channelBusy++;
            delete pkt;
        }
    }

}

simtime_t Host::generateBackofftime()
{
    int CW = pow(2, 2 + backoffCount) - 1;
    int slots = intrand(CW + 1);
    EV << "slots: " << slots << endl;
    return slots * slotTime;
};

simtime_t Host::getNextTransmissionTime()
{
    simtime_t t = simTime() + iaTime->doubleValue();

    return t;
}

void Host::sendRTS(){
    cPacket *RTS = new cPacket("RTS");
    sendDirect(RTS, radioDelay, RTS_TIME, server->gate("in"));

    for (int i = 0; i < numOtherHosts; i++) {
        RTS = new cPacket("RTS");
        sendDirect(RTS, otherHostDelay[i], RTS_TIME, otherHostGate[i]);
    }

    // if don't get CTS, backoff
    scheduleAt(simTime() + RTS_TIME + SIFS * 5, backoff);
    state = WAIT_CTS;
}

void Host::sendPacket(cPacket *pk) {
    EV << "send packet " << pkname << endl;
    state = TRANSMIT;
    emit(stateSignal, state);
    
    simtime_t duration = pk->getBitLength() / txRate;
    sendDirect(pk, radioDelay, duration, server->gate("in"));

    for (int i = 0; i < numOtherHosts; i++) {
        snprintf(broadcast, sizeof(broadcast), "from-%d, to-%d", getIndex(), otherHosts[i]->getIndex());
        // EV << "generating packet " << broadcast << endl;

        cMessage *broadcastPacket = new cMessage(broadcast);

        sendDirect(broadcastPacket, otherHostDelay[i], \
                    0, otherHostGate[i]);
    }

    scheduleAt(simTime()+duration, endTxEvent);
    
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

void Host::receiveSignal(cComponent *source, simsignal_t signalID, intval_t i, cObject *details)
{
    // EV << "host listen long" << endl;
    // if (signalID == channelStateSignal) {
    //     if (i > 0) {
    //         channelBusy = true;
    //     } else {
    //         channelBusy = false;
    //     }
    // }
}

}; //namespace
