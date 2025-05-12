#include <cstddef>
#include "squid.h"
#include "SquidConfig.h"

class Salsa2Parent{
    private:
        size_t nCaches;
        size_t* missPosArr;
        size_t* missNegArr;
        size_t* reqNum;
        double missPos;
        double missNeg;
        
    
        Salsa2Parent(size_t caches):
            nCaches(caches),
            missPosArr(new size_t[caches]{0}),
            missNegArr(new size_t[caches]{0}),
            reqNum(new size_t[caches]{0}){}
        
        ~Salsa2Parent();
        
        static Salsa2Parent* instance;

    public:
        void newReq(size_t posIndications){++this->reqNum[posIndications];}
        void newMiss(size_t posIndications, bool isPosInd);

        static void init(size_t caches){if (caches && Config.salsa2 && !instance) instance = new Salsa2Parent(caches);}
        static Salsa2Parent& getInstance(){return *instance;}
        static void free(){if (instance) delete instance;}
};