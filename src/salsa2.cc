#include "squid.h"
#include "debug/Stream.h"
#include "HttpRequest.h"
#include "salsa2.h"
#include <random>
#include "neighbors.h"
#include "mem/AllocatorProxy.h"

using namespace std;

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

void Salsa2::peerSelection()
{    
    debugs(96,0,"Salsa2: Starting salsa2 peer selection for URL: " << this->request->url.host());

    *(this->servers) = NULL;

    checkDigestsHits();
    selectPeers();
    addRoundRobin();

    debugs(96,0,"Salsa2: Those are the peers that selected by Salsa2:");

    for(FwdServer* f = *(this->servers); f; f = f->next)
        debugs(96,0,"Salsa2: "<< *(f->_peer));

    #ifdef REQ_UPDATE
    this->updateReq();
    #endif
}

void Salsa2::checkDigestsHits()
{
    #ifdef REQ_UPDATE

    size_t cacheIndex = -1;

    #endif

    request->hier.n_choices = request->hier.n_ichoices = 0;

    // Don't know what this is doing
    storeKeyPublicByRequest(this->request);

    for (CachePeer* peer = Config.peers; peer; peer = peer->next)
    {
        #ifdef REQ_UPDATE

        this->cachesData[++cacheIndex].name = peer->name;

        #endif

        lookup_t lookup;

        lookup = peerDigestLookup(peer, this->selector);

        debugs(96,0,"Salsa2: Checking digest of " << *peer << " Result: " << lookup);

        if (lookup != LOOKUP_NONE)
        {
            // Don't know what is the use of this var
            this->request->hier.n_choices++;

            if (lookup == LOOKUP_HIT && peerHTTPOkay(peer, this->selector))
            {
                #ifdef REQ_UPDATE

                this->cachesData[cacheIndex].indication = 
                    this->cachesData[cacheIndex].accessed = 1;

                #endif

                // Don't know what is the use of this var
                this->request->hier.n_ichoices++;    

                // I prefer to ommit next line, because it note of only one digest that choosen,
                // and in salsa2 we choose many
                // peerNoteDigestLookup(request, p, LOOKUP_HIT);

                this->addPeer(peer, CD_PARENT_HIT);
            }
        }
    }
}

void Salsa2::addPeer(CachePeer* peer, hier_code code)
{
    FwdServer* newTail = new FwdServer(peer, code);

    if (!this->tail)
    {
        *(this->servers) = this->tail = newTail;
    } else
    {
        this->tail = this->tail->next = newTail;
    }
}

void Salsa2::selectPeers()
{
}

#ifdef REQ_UPDATE

#define NOT_FOUND 99999

size_t Salsa2::getPeerIndex(string name)
{
    for (int i = 0; i < Config.npeers; i++)
        if (this->cachesData[i].name == name)
            return i;

    return NOT_FOUND;
}

#endif

void Salsa2::addRoundRobin()
{
    if (!*(this->servers))
    {
        debugs(96,0,"Salsa2: No digest match for URL: " << this->request->url.host());
        
        for (int i = 0; i < Config.npeers; i++)
        {
            if (!currentPeer)
                currentPeer = Config.peers;
            
            assert(currentPeer);

            if (peerHTTPOkay(currentPeer, this->selector))
            {

                this->addPeer(currentPeer,ROUNDROBIN_PARENT);
                debugs(96,0,"Salsa2: Add " << *currentPeer << " as round robin");

                #ifdef REQ_UPDATE
                
                this->cachesData[this->getPeerIndex(currentPeer->name)].accessed = 1;

                #endif

                currentPeer = currentPeer->next;

                return;
            }

            currentPeer = currentPeer->next;
        }

        debugs(96,0,"Salsa2: Not found any online parent");
    }
}

#ifdef REQ_UPDATE

void Salsa2::updateReq()
{    
    stringstream sstr;
    sstr << REQ_UPDATE << " " << this->request->url.host();

    for (int i = 0; i < Config.npeers; i++)
    {        
        sstr << " " << this->cachesData[i].name
        << " " << this->cachesData[i].indication
        << " " << this->cachesData[i].accessed
        << " " << (rand() % 2); // simulate resolution until implemets it
    }
    
    int res = system(sstr.str().c_str());
    
    debugs(96,0,"Salsa2: command " << sstr.str() << " returned code " << res ); 
}

#endif