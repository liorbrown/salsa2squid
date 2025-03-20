#pragma once

#include "PeerSelectState.h"

class Salsa2{
    static void checkDigestsHits(FwdServer** severs);
    static void selectPeers(FwdServer** severs);
    
    public:
        static void peerSelection(PeerSelector*, FwdServer**);
};