#include <cstddef>
#include <unordered_set>
#include "squid.h"
#include "SquidConfig.h"
#include "HttpRequest.h"

using namespace std;

/// @brief This is singltone class that contain for this cache all the statistic data needed for salsa2
class Salsa2Parent{
    private:
        const size_t nCaches;
        const size_t reEstimateWindow;

        // All statistics matrices have 2 rows, first for spectular requests
        // (this cahce have negative indication)
        // and seconed for regular requests (this cahce have possitive indication)
        size_t** reqCounter;
        size_t** missCounter;
        double** exclusionProbability;
    
        Salsa2Parent(size_t caches):
            nCaches(caches),
            reEstimateWindow(3), // TODO: init now arbitary to 3, will be estimated ahead
            reqCounter(new size_t*[2] {new size_t[caches + 1]{0}, new size_t[caches + 1]{0}}),
            missCounter(new size_t*[2] {new size_t[caches + 1]{0}, new size_t[caches + 1]{0}}),
            exclusionProbability(new double*[2] {new double[caches + 1]{0.0}, new double[caches + 1]{0.0}}){}

        ~Salsa2Parent();
        
        /// @brief Updating exclusionProbability according to given request details
        /// @param request This request tells salsa2 which probalitiy we need to estimate
        void reEstimateExclusionProb(HttpRequest::Pointer request);

        static Salsa2Parent* instance;
    public:

        /// @brief Tihs methos calles when new request arrived
        /// , this is update the num of requests that sended for certain number of 
        /// positive indications
        /// @param request Current request
        /// @param posIndications List of caches that gave possitive indication to proxy
        void newReq(HttpRequest::Pointer request, const unordered_set<string>& posIndications);
        
        /// @brief Called when this cache miss request.
        /// Updates the number of misses
        /// @param request The requset that missed
        void newMiss(HttpRequest::Pointer request);

        static Salsa2Parent& getInstance(size_t caches = 0);

        /// @brief Checks if given request is handling by salsa2
        /// @param request 
        /// @return True - if handle by salsa2, False - otherwise
        static bool isSalsa(HttpRequest::Pointer request)
            {return request->header.getByName("salsa2").size();}

        /// @brief Parse "salsa2" header entry
        /// @param header "salsa2" header entry
        /// @param nCaches Returns the number of parents
        /// @param posIndications Returns set of cahces 
        /// that gave to proxy positive indication for this request
        static void parse
            (const String header, 
             size_t& nCaches, 
             unordered_set<string>& posIndications);

        /// @brief Called when this server shutdown 
        static void free(){if (instance) delete instance;}
};