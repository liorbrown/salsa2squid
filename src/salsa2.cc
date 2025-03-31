#include "squid.h"
#include "debug/Stream.h"
#include "HttpRequest.h"
#include "salsa2.h"
#include "SquidConfig.h"
#include "neighbors.h"
#include "mem/AllocatorProxy.h"

// Copy this class from peer_select.cc
// i prefer to copy only this code instead of include all peer_select.cc
class FwdServer
{
    MEMPROXY_CLASS(FwdServer);

public:
    FwdServer(CachePeer *p, hier_code c) :
        _peer(p),
        code(c),
        next(nullptr)
    {}

    CbcPointer<CachePeer> _peer;                /* NULL --> origin server */
    hier_code code;
    FwdServer *next;
};

CachePeer* Salsa2::currentPeer = NULL;

Salsa2::Salsa2(PeerSelector* peerSelector, FwdServer** fwdServers){
    this->selector = peerSelector;
    this->servers = fwdServers;
    this->tail = NULL;
}

void Salsa2::peerSelection(){
    debugs(96,0,"Starting salsa2 peer selection for URL: " << this->selector->request->url.host());

    *(this->servers) = NULL;

    checkDigestsHits();
    selectPeers();
    addRoundRobin();

    debugs(96,0,"Those are the peers that selected by Salsa2:");

    for(FwdServer* f = *(this->servers); f; f = f->next)
        debugs(96,0,*(f->_peer));
}

void Salsa2::checkDigestsHits(){

    HttpRequest *request = this->selector->request;

    request->hier.n_choices = request->hier.n_ichoices = 0;

    // Don't know what this is doing
    storeKeyPublicByRequest(request);

    for (CachePeer* peer = Config.peers; peer; peer = peer->next) {
        lookup_t lookup;

        lookup = peerDigestLookup(peer, this->selector);

        debugs(96,0,"Checking digest of " << *peer << " Result: " << lookup);

        if (lookup != LOOKUP_NONE){

            // Don't know what is the use of this var
            request->hier.n_choices++;

            if (lookup == LOOKUP_HIT && peerHTTPOkay(peer, this->selector)){

                // Don't know what is the use of this var
                request->hier.n_ichoices++;    

                // I prefer to commit next line, because it note of only one digest that choosen,
                // and in salsa2 we choose many
                // peerNoteDigestLookup(request, p, LOOKUP_HIT);

                this->addPeer(peer, CD_PARENT_HIT);
            }
        }
    }
}

void Salsa2::addPeer(CachePeer* peer, hier_code code){

    FwdServer* newTail = new FwdServer(peer, code);

    if (!this->tail){
        *(this->servers) = this->tail = newTail;
    } else {
        this->tail = this->tail->next = newTail;
    }
}

void Salsa2::selectPeers(){
}

void Salsa2::addRoundRobin(){
    if (!*(this->servers)){
        debugs(96,0,"No digest match for URL: " << this->selector->request->url.host());
        
        for (int i = 0; i < Config.npeers; i++)
        {
            if (!currentPeer)
                currentPeer = Config.peers;
            
            assert(currentPeer);

            if (peerHTTPOkay(currentPeer, this->selector)){
                this->addPeer(currentPeer,ROUNDROBIN_PARENT);
                debugs(96,0,"Add " << *currentPeer << " as round robin");

                currentPeer = currentPeer->next;
                return;
            }

            currentPeer = currentPeer->next;
        }

        debugs(96,0,"Not found any online parent");
    }
}