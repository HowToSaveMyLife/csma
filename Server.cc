//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 1992-2015 Andras Varga
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//

#include "Server.h"

namespace csma {

Define_Module(Server);

Server::~Server()
{
    cancelAndDelete(endRxEvent);
}

void Server::initialize()
{
    channelStateSignal = registerSignal("channelState");
    endRxEvent = new cMessage("end-reception");
    channelBusy = false;
    emit(channelStateSignal, IDLE);

    gate("in")->setDeliverImmediately(true);

    currentCollisionNumFrames = 0;
    receiveCounter = 0;
    WATCH(currentCollisionNumFrames);

    receiveBeginSignal = registerSignal("receiveBegin");
    receiveSignal = registerSignal("receive");
    collisionSignal = registerSignal("collision");
    collisionLengthSignal = registerSignal("collisionLength");

    emit(receiveSignal, 0L);
    emit(receiveBeginSignal, 0L);

    getDisplayString().setTagArg("p", 0, par("x").doubleValue());
    getDisplayString().setTagArg("p", 1, par("y").doubleValue());

    SIFS = par("SIFS");
    CTS_TIME = par("CTS");

    CTS = new cPacket("CTS");

    x = par("x").doubleValue();
    y = par("y").doubleValue();

    numHosts = par("numHosts");
    Hosts = new cModule *[numHosts];
    HostGate = new cGate *[numHosts];
    HostDelay = new simtime_t[numHosts];
    char hostname[10] = "host[x]";
    for (int i = 0; i < numHosts; i++) {
        snprintf(hostname, sizeof(hostname), "host[%d]", i);
        Hosts[i] = getModuleByPath(hostname);
        HostGate[i] = Hosts[i]->gate("in");

        double dist = std::sqrt((x-Hosts[i]->par("x").doubleValue()) * \
                    (x-Hosts[i]->par("x").doubleValue()) + \
                    (y-Hosts[i]->par("y").doubleValue()) * \
                    (y-Hosts[i]->par("y").doubleValue()));
        HostDelay[i] = dist / propagationSpeed;
    }
    CTS_flag = false;
}

void Server::handleMessage(cMessage *msg)
{
    if (msg == endRxEvent) {
        EV << "reception finished\n";
        channelBusy = false;
        emit(channelStateSignal, IDLE);

        // update statistics
        simtime_t dt = simTime() - recvStartTime;
        if (currentCollisionNumFrames == 0) {
            // start of reception at recvStartTime
            cTimestampedValue tmp(recvStartTime, (intval_t)1);
            emit(receiveSignal, &tmp);
            // end of reception now
            emit(receiveSignal, 0);
        }
        else {
            // start of collision at recvStartTime
            cTimestampedValue tmp(recvStartTime, currentCollisionNumFrames);
            emit(collisionSignal, &tmp);

            emit(collisionLengthSignal, dt);
        }

        currentCollisionNumFrames = 0;
        receiveCounter = 0;
        emit(receiveBeginSignal, receiveCounter);
    } else if (strcmp(msg->getName(), RTS) == 0){
        // TODO: if many hosts send RTS at the same time, the CTS will only be sent to last one
        // method1: only send CTS to the last one
        // method2: send CTS to all hosts, but carry the index of the host, and the host will check if the CTS is for itself
        if (!CTS_flag) {
            cPacket *pkt = check_and_cast<cPacket *>(msg);
            simtime_t duration = pkt->getDuration();
            scheduleAt(simTime() + duration + SIFS, CTS);
            CTS_flag = true;
            delete pkt;
        } else {
            cancelEvent(CTS);
            CTS_flag = false;
            delete msg;
        }
    } else if (msg == CTS) {
        for (int i = 0; i < numHosts; i++) {
            cPacket *CTS_send = new cPacket("CTS");
            sendDirect(CTS_send, HostDelay[i], CTS_TIME, HostGate[i]);
        }
        CTS_flag = false;
    }
    else {
        cPacket *pkt = check_and_cast<cPacket *>(msg);

        ASSERT(pkt->isReceptionStart());
        simtime_t endReceptionTime = simTime() + pkt->getDuration();

        emit(receiveBeginSignal, ++receiveCounter);

        if (!channelBusy) {
            EV << "started receiving\n";
            recvStartTime = simTime();
            channelBusy = true;
            emit(channelStateSignal, TRANSMISSION);
            scheduleAt(endReceptionTime, endRxEvent);
        }
        else {
            EV << "another frame arrived while receiving -- collision!\n";
            emit(channelStateSignal, COLLISION);

            if (currentCollisionNumFrames == 0)
                currentCollisionNumFrames = 2;
            else
                currentCollisionNumFrames++;

            if (endReceptionTime > endRxEvent->getArrivalTime()) {
                cancelEvent(endRxEvent);
                scheduleAt(endReceptionTime, endRxEvent);
            }

            // update network graphics
            if (hasGUI()) {
                char buf[32];
                snprintf(buf, sizeof(buf), "Collision! (%" PRId64 " frames)", currentCollisionNumFrames);
                bubble(buf);
                getParentModule()->getCanvas()->holdSimulationFor(par("animationHoldTimeOnCollision"));
            }
        }
        channelBusy = true;
        delete pkt;
    }
}

void Server::refreshDisplay() const
{
    if (!channelBusy) {
        getDisplayString().setTagArg("i2", 0, "status/off");
        getDisplayString().setTagArg("t", 0, "");
    }
    else if (currentCollisionNumFrames == 0) {
        getDisplayString().setTagArg("i2", 0, "status/yellow");
        getDisplayString().setTagArg("t", 0, "RECEIVE");
        getDisplayString().setTagArg("t", 2, "#808000");
    }
    else {
        getDisplayString().setTagArg("i2", 0, "status/red");
        getDisplayString().setTagArg("t", 0, "COLLISION");
        getDisplayString().setTagArg("t", 2, "#800000");
    }
}

void Server::finish()
{
    EV << "duration: " << simTime() << endl;

    recordScalar("duration", simTime());
}

}; //namespace
