#include "squid.h"
#include "debug/Stream.h"
#include "HttpRequest.h"
#include "salsa2.h"

void Salsa2::peerSelection(PeerSelector* selector, FwdServer** severs){
    debugs(96,0,"Starting salsa2 peer selection for URL: " << selector->request->url.host());

    checkDigestsHits(severs);
    selectPeers(severs);
}

void Salsa2::checkDigestsHits(FwdServer** severs){
    *severs = NULL;
}

void Salsa2::selectPeers(FwdServer** severs){
    *severs = NULL;
}