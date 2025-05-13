#include "squid.h"
#include "debug/Stream.h"
#include "HttpRequest.h"
#include "salsa2proxy.h"
#include <random>
#include "neighbors.h"
#include "mem/AllocatorProxy.h"

using namespace std;

// Copy this class from peer_select.cc
// I prefer to copy only this code instead of include all peer_select.cc
// This class represents a forward server, which could be a peer cache or the origin server.
class FwdServer
{
    MEMPROXY_CLASS(FwdServer);

public:
    // Constructor for the FwdServer.
    // Takes a CachePeer pointer (nullptr if it's the origin server) and a hierarchy code.
    FwdServer(CachePeer *p, hier_code c) :
        _peer(p),
        code(c),
        next(nullptr)
    {}

    // Pointer to the CachePeer object. If it's the origin server, this will be NULL.
    CbcPointer<CachePeer> _peer;
    // Hierarchy code indicating the type of forward server (e.g., parent hit, round-robin).
    hier_code code;
    // Pointer to the next FwdServer in a linked list.
    FwdServer *next;
};

// Static member to keep track of the current peer for round-robin selection.
CachePeer* Salsa2Proxy::currentPeer = nullptr;

#ifdef REQ_UPDATE
Salsa2Proxy* Salsa2Proxy::activeSalsa = nullptr;
#endif

Salsa2Proxy::Salsa2Proxy(PeerSelector* peerSelector, FwdServer** fwdServers):
        selector(peerSelector),
        servers(fwdServers),
        tail(nullptr),
        request(peerSelector->request)
        
        #ifdef REQ_UPDATE
        ,cachesData(new cacheData[Config.npeers])  
        #endif

        {
            #ifdef REQ_UPDATE
            Salsa2Proxy::activeSalsa = nullptr;
            #endif
        }

// This is the main function responsible for selecting peers using the Salsa2 algorithm.
void Salsa2Proxy::peerSelection()
{
    debugs(96,0,"Salsa2: Starting salsa2 peer selection for URL: " << this->request->url.host());

    // Initialize the list of selected forward servers to empty.
    *(this->servers) = NULL;

    // Check for digest hits among the available peers.
    checkDigestsHits();
    // Apply the Salsa2 peer selection logic (currently empty).
    selectPeers();
    // Add peers using a round-robin strategy if no suitable peers were found via digests.
    addRoundRobin();

    debugs(96,0,"Salsa2: Those are the peers that selected by Salsa2:");

    // Iterate through the list of selected forward servers and print their information.
    for(FwdServer* f = *(this->servers); f; f = f->next)
        debugs(96,0,"Salsa2: "<< *(f->_peer));


    #ifdef REQ_UPDATE
        this->getResolutions();
    #else
        this->dispatch();
    #endif    
}

// This function checks the digests of each peer to see if they have the requested content.
void Salsa2Proxy::checkDigestsHits()
{
#ifdef REQ_UPDATE
    // Index to keep track of the current cache in the cachesData array.
    size_t cacheIndex = -1;
#endif

    // Reset the number of choices and ideal choices for hierarchical selection.
    request->hier.n_choices = request->hier.n_ichoices = 0;

    // Generate a public store key for the requested object.
    storeKeyPublicByRequest(this->request);

    // Iterate through the configured peers.
    for (CachePeer* peer = Config.peers; peer; peer = peer->next)
    {
#ifdef REQ_UPDATE
        // Store the name of the current peer in the cachesData array.
        this->cachesData[++cacheIndex].name = peer->name;
#endif

        // Perform a digest lookup for the current peer.
        lookup_t lookup = peerDigestLookup(peer, this->selector);

        debugs(96,0,"Salsa2: Checking digest of " << *peer << " Result: " << lookup);

        // If the digest lookup was not negative (i.e., the peer has some information about the object).
        if (lookup != LOOKUP_NONE)
        {
            // Increment the total number of choices for hierarchical selection.
            this->request->hier.n_choices++;

            // If the digest lookup was a hit and the peer is HTTP okay (available and healthy).
            if (lookup == LOOKUP_HIT && peerHTTPOkay(peer, this->selector))
            {
#ifdef REQ_UPDATE
                // Mark this cache as having an indication (digest hit) and being accessed.
                this->cachesData[cacheIndex].indication =
                    this->cachesData[cacheIndex].accessed = 1;
#endif

                // Increment the number of ideal choices (peers with a digest hit).
                this->request->hier.n_ichoices++;

                // I prefer to omit the next line, because it notes only one digest that is chosen,
                // and in salsa2 we choose many.
                // peerNoteDigestLookup(request, p, LOOKUP_HIT);

                // Add this peer to the list of forward servers with the code indicating a parent hit.
                this->addPeer(peer, CD_PARENT_HIT);

                char buf[MAX_IPSTRLEN];
                this->request->posIndications.insert(peer->addresses[0].toUrl(buf,MAX_IPSTRLEN));
            }
        }
    }
}

// Adds a peer to the linked list of forward servers.
void Salsa2Proxy::addPeer(CachePeer* peer, hier_code code)
{
    // Create a new FwdServer object.
    FwdServer* newTail = new FwdServer(peer, code);

    // If the list of servers is currently empty.
    if (!this->tail)
    {
        // Set the head and tail of the list to the new server.
        *(this->servers) = this->tail = newTail;
    } else
    {
        // Append the new server to the end of the list and update the tail.
        this->tail = this->tail->next = newTail;
    }
}

// This function is intended to implement the core Salsa2 peer selection logic.
// Currently, it is empty.
void Salsa2Proxy::selectPeers()
{
    // TODO: Implement the Salsa2 peer selection algorithm here.
    // This function should use the information gathered in checkDigestsHits()
    // and potentially other factors to select the most suitable peers.
}

// Conditional compilation block for request updating functionality.
#ifdef REQ_UPDATE

// Define a constant for indicating that a peer index was not found.
#define NOT_FOUND 99999

// Helper function to get the index of a peer in the cachesData array by its name.
size_t Salsa2Proxy::getPeerIndex(char* name)
{
    // Iterate through the cachesData array.
    for (int i = 0; i < Config.npeers; i++)
        // If the name matches, return the index.
        if (this->cachesData[i].name == name)
            return i;

    // If the peer name is not found, return the NOT_FOUND value.
    return NOT_FOUND;
}

#endif

// Adds peers using a round-robin strategy if no suitable peers were found via digests.
void Salsa2Proxy::addRoundRobin()
{
    // If no servers have been selected yet (no digest hits).
    if (!*(this->servers))
    {
        debugs(96,0,"Salsa2: No digest match for URL: " << this->request->url.host());

        // Iterate through all configured peers.
        for (int i = 0; i < Config.npeers; i++)
        {
            // If the currentPeer pointer is null, initialize it to the first peer in the list.
            if (!currentPeer)
                currentPeer = Config.peers;

            // Ensure that currentPeer is not null.
            assert(currentPeer);

            // If the current peer is HTTP okay (available and healthy).
            if (peerHTTPOkay(currentPeer, this->selector))
            {
                // Add this peer to the list of forward servers with the round-robin code.
                this->addPeer(currentPeer,ROUNDROBIN_PARENT);
                debugs(96,0,"Salsa2: Add " << *currentPeer << " as round robin");

#ifdef REQ_UPDATE
                // Mark this peer as accessed in the cachesData array.
                size_t index = this->getPeerIndex(currentPeer->name);
                if (index != NOT_FOUND)
                    this->cachesData[index].accessed = 1;
#endif

                // Move to the next peer in the list for the next round-robin selection.
                currentPeer = currentPeer->next;

                // Return after adding the first available peer in round-robin.
                return;
            }

            // Move to the next peer even if the current one is not HTTP okay.
            currentPeer = currentPeer->next;
        }

        // If the loop finishes without finding any online parent.
        debugs(96,0,"Salsa2: Not found any online parent");
    }
}

void Salsa2Proxy::dispatch()
{
    this->request->nCaches = Config.npeers;
    this->selector->resolveSelected();    
}

// Conditional compilation block for updating the request with peer selection information.
#ifdef REQ_UPDATE

// Function to update the request information, by executing an external command.
void Salsa2Proxy::updateReq()
{
    Salsa2Proxy::activeSalsa = nullptr;

    // Create a stringstream to build the command.
    stringstream sstr;

    // Start building the command with the REQ_UPDATE flag and the requested URL host.
    sstr << REQ_UPDATE << " " << this->request->url.host();

    // Iterate through the cachesData array to append peer information.
    for (int i = 0; i < Config.npeers; i++)
    {
        // Append peer name, indication (digest hit), accessed status, and a simulated resolution status.
        sstr << " " << this->cachesData[i].name
             << " " << this->cachesData[i].indication
             << " " << this->cachesData[i].accessed
             << " " << this->cachesData[i].resolution;
    }

    // Execute the constructed command using the system() call.
    int res = system(sstr.str().c_str());

    // Log the executed command and its return code.
    debugs(96,0,"Salsa2: command " << sstr.str() << " returned code " << res );

    this->dispatch();

    delete this;
}

void Salsa2Proxy::getResolutions()
{
    Salsa2Proxy::activeSalsa = this;

    int timeout;

    neighborsUdpPing(request,
                    this->selector->entry,
                    nullptr,
                    this->selector,
                    &this->pingsWaiting,
                    &timeout);

    debugs(96,0,"Salsa2: icpReqWaiting = " << this->pingsWaiting);
}

void Salsa2Proxy::getIcp(CachePeer * p, icp_common_t* header)
{    
    this->cachesData[this->getPeerIndex(p->name)].resolution = 
        header->getOpCode() == ICP_HIT;

    debugs(96,0,"Salsa2: Recive new icp response from cache " << p->name << 
        " Result: " << header->getOpCode() <<
        " pingsWaiting is " << this->pingsWaiting);

    if (!--this->pingsWaiting)
        this->updateReq();
}

#endif

