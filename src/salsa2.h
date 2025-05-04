#pragma once

#include "SquidConfig.h"
#include "PeerSelectState.h"
#include "CachePeer.h"

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
    //static IRCB icpRecive;
    

    #endif

    void checkDigestsHits();
    void selectPeers();
    void addRoundRobin();
    void addPeer(CachePeer*, hier_code);
    
    public:
        Salsa2(PeerSelector* peerSelector, FwdServer** fwdServers);
        void peerSelection();

        #ifdef REQ_UPDATE
        static Salsa2* activeSalsa;
        IRCB getIcp;
        #endif
};