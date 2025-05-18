#include <cstddef>
#include "squid.h"
#include "SquidConfig.h"

/// @brief This is singltone class that contain for this cache all the statistic data needed for salsa2
class Salsa2Parent{
    private:
        size_t nCaches;
        size_t* missPosArr;
        size_t* missNegArr;
        size_t* reqNum;
        double* missPos;
        double* missNeg;
        
    
        Salsa2Parent(size_t caches):
            nCaches(caches),
            missPosArr(new size_t[caches]{0}),
            missNegArr(new size_t[caches]{0}),
            reqNum(new size_t[caches]{0}){}
        
        ~Salsa2Parent();
        
        static Salsa2Parent* instance;

    public:

        /// @brief Tihs methos calles when new request arrived
        /// , this is update the num of requests that sended for certain number of 
        /// positive indications
        /// @param posIndications Number of positive indications that proxy got for this request
        void newReq(size_t posIndications){
            debugs(96, 
                   DBG_CRITICAL, 
                   "Salsa2: Got new request with " << 
                   posIndications << 
                   ", Requsets count is " << 
                   this->reqNum[posIndications]++);}
        
        /// @brief Called when this cache miss request.
        /// Updates the number of misses
        /// @param posIndications Number of positive indications proxy recived for this request
        /// @param isPosInd True - if this cache digest give to proxy possitive indication
        void newMiss(size_t posIndications, bool isPosInd);

        static Salsa2Parent& getInstance(size_t caches);

        /// @brief Called when this server shutdown 
        static void free(){if (instance) delete instance;}
};