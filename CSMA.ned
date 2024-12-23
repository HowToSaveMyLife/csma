//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 1992-2015 Andras Varga
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//


//
// The Aloha network consists of hosts which talk to the central "server" via
// the Aloha or Slotted Aloha protocol
//
network CSMA
{
    parameters:
        int numHosts;  // number of hosts
        double txRate @unit(bps);  // transmission rate
        double slotTime @unit(ms);  // zero means no slots (pure Aloha)
        double DIFS @unit(ms);
        double SIFS @unit(ms);
        double RTS @unit(ms);
        double CTS @unit(ms);
        int maxBackoffs;
        @display("bgi=background/terrain,s;bgb=1000,1000");
    submodules:
        server: Server{
            CTS = parent.CTS;
            SIFS = parent.SIFS;
            numHosts = parent.numHosts;
        };
        host[numHosts]: Host {
            txRate = parent.txRate;
            slotTime = parent.slotTime;
            maxBackoffs = parent.maxBackoffs;
            DIFS = parent.DIFS;
            SIFS = parent.SIFS;
            RTS = parent.RTS;
        }
}


