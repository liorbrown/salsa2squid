#pragma once

#include "SquidConfig.h"
#include "PeerSelectState.h"
#include "CachePeer.h"

#define REQ_UPDATE "python3 /home/lior/Salsa2Simulator/reqUpdate.py"

using namespace std;

class Salsa2Proxy{
    
    static CachePeer* currentPeer;

    PeerSelector* selector;
    FwdServer** servers;
    FwdServer* tail;
    HttpRequest* request;
    int pingsWaiting;

    #ifdef REQ_UPDATE

    typedef struct{
        char* name;
        int indication = 0;
        int accessed = 0;
        int resolution = 0;
    }  cacheData ;

    cacheData* cachesData;
    size_t getPeerIndex(char*);
    void updateReq();
    void getResolutions();    

    #endif

    void checkDigestsHits();
    void selectPeers();
    void addRoundRobin();
    void addPeer(CachePeer*, hier_code);
    void dispatch();
    
    public:
        Salsa2Proxy(PeerSelector* peerSelector, FwdServer** fwdServers);
        void peerSelection();

        #ifdef REQ_UPDATE
        static Salsa2Proxy* activeSalsa;
        void getIcp(CachePeer*, icp_common_t*);
        #endif
};