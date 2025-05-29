#include <cstddef>
#include <unordered_set>
#include "squid.h"
#include "SquidConfig.h"
#include "HttpRequest.h"

using namespace std;

/// @brief This is singltone class that contain for this cache all the statistic data needed for salsa2
class Salsa2Parent{
    private:
        size_t nCaches;
        size_t* missPosArr;
        size_t* missNegArr;
        size_t* reqNum;
        double* possExclusionProbability;
        double* negExclusionProbability;
        
    
        Salsa2Parent(size_t caches):
            nCaches(caches),          
            missPosArr(new size_t[caches + 1]{0}),
            missNegArr(new size_t[caches + 1]{0}),
            reqNum(new size_t[caches + 1]{0}),
            possExclusionProbability(new double[caches + 1]),
            negExclusionProbability(new double[caches + 1]){}
        
        ~Salsa2Parent();
        
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