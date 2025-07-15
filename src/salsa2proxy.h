#pragma once

#include "SquidConfig.h"
#include "PeerSelectState.h"
#include "CachePeer.h"
#include "salsa2parent.h"
#include <map>

// This is define if we want to update our simulator with the results
// all of the code that check this var is used only for algorithms develops and check itself.
// not shuold be exist in prod
#define REQ_UPDATE "python3 /home/lior/Salsa2Simulator/reqUpdate.py"

using namespace std;

/// @brief This class represents on object that creates only in proxy cahce, not parent
/// and its created for each reuquest and implements the salsa2 algorithm of parents peers choosing  
class Salsa2Proxy{

    private:

        // This 3D array holds all the exclusion probabilities of all parents
        static map<String, ProbabilityMatrix> exclusionProbabilities;  

        // This point to next peer to choose in the round robin mechanism
        static CachePeer* currentPeer;

        PeerSelector* selector;
        FwdServer*& servers;
        FwdServer* tail;
        HttpRequest* request;
        int pingsWaiting;

        #ifdef REQ_UPDATE

        /// @brief This struct contains the data that each parent had for this request
        typedef struct{
            char* name;
            int indication = 0;
            int accessed = 0;
            int resolution = 0;
        }  cacheData ;

        cacheData* cachesData;

        /// @brief Gets peer index by name
        /// @param name Peer name
        /// @return Peer index
        size_t getPeerIndex(const char* name) const;

        /// @brief Update requst data in simulator DB, and dispatch request to peers
        void updateReq();

        /// @brief Sends ICP requests to pers to check if they have the requested data, without ask data itself
        /// its important because if we ask data itself from all peers it will make all peers have all data
        /// and then all the idea of the algorithm will go wrong
        void getResolutions();    

        #endif

        /// @brief Start salsa2 peers selection
        void peerSelection();

        /// @brief Checks for each peer if its digest indicate that it have this request
        void checkDigestsHits();

        /// @brief Select the peers to ask according to digests result
        /// SALSA2 TODO: need implement
        void selectPeers();

        /// @brief If no digest give positive indication, select peer with round robin
        /// SALSA2 TODO: - Will change in the future
        void addRoundRobin();

        /// @brief Add peer to peers list of forword them the requsts
        /// @param peer Peer to add
        /// @param code Code that tell why we choose this peer
        void addPeer(CachePeer* peer, hier_code code);

        /// @brief Send requsts to selected peers
        void dispatch();

        /// @brief Get probabilities matrix. create it if not exist
        /// @return Probabilities matrix
        static map<String, ProbabilityMatrix>& getProbabilities();

    public:
        Salsa2Proxy(PeerSelector* peerSelector, FwdServer*& fwdServers);

        /// @brief Update peer's probabilities matrix when new response arrived
        /// @param reply The reply that came from peer, this give us the new probability
        /// @param request The request for this reply, this tell us witch probability to update
        /// @param peer Peer name that send this responed
        static void updateProbabilty
            (const HttpReply* reply, 
            const HttpRequestPointer request, 
            const CachePeer* peer);

        #ifdef REQ_UPDATE

        static Salsa2Proxy* activeSalsa;

        /// @brief This method called when get response of ICP query we send in getResolution method
        /// @param p Peer that response as the ICP query
        /// @param header ICP query result
        void getIcp(CachePeer * p, icp_common_t* header);

        #endif
};