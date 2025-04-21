#pragma once

#include "SquidConfig.h"
#include "PeerSelectState.h"
#include "CachePeer.h"
#include <string>

#define REQ_UPDATE "python3 /home/lior/Salsa2Simulator/reqUpdate.py"

using namespace std;

class Salsa2{
    
    static double missPos;
    static double missNeg;
    static CachePeer* currentPeer;

    PeerSelector* selector;    
    FwdServer** servers;
    FwdServer* tail;
    HttpRequest* request;

    #ifdef REQ_UPDATE

    typedef struct{
        string name;
        int indication = 0;
        int accessed = 0;
        int resolution = 0;
    }  cacheData ;

    cacheData* cachesData;
    size_t getPeerIndex(string);
    void updateReq();

    #endif

    void checkDigestsHits();
    void selectPeers();
    void addRoundRobin();
    void addPeer(CachePeer*, hier_code);
    
    public:
        Salsa2(PeerSelector* peerSelector, FwdServer** fwdServers):
        selector(peerSelector),
        servers(fwdServers),
        tail(nullptr),
        request(peerSelector->request)
        
        #ifdef REQ_UPDATE    
        ,cachesData(new cacheData[Config.npeers])  
        #endif

        {}
        void peerSelection();
};