#pragma once

#include "PeerSelectState.h"
#include "CachePeer.h"

class Salsa2{
    static CachePeer* currentPeer;
    PeerSelector* selector;
    FwdServer** servers;
    FwdServer* tail;

    void checkDigestsHits();
    void selectPeers();
    void addRoundRobin();
    void addPeer(CachePeer*, hier_code);
    
    public:
        Salsa2(PeerSelector*, FwdServer**);
        void peerSelection();
};