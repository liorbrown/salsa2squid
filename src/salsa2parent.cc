#include "salsa2parent.h"

Salsa2Parent* Salsa2Parent::instance = nullptr;

Salsa2Parent::~Salsa2Parent()
{
    if (this->missNegArr)
        delete[] missNegArr;

    if (this->missPosArr)
        delete[] this->missPosArr;

    if (this->reqNum)
        delete[] this->missPosArr;
}

void Salsa2Parent::newMiss(size_t posIndications, bool isPosInd)
{
    // Checks if need to update the number of misses for possitive indication
    // or for negative indication
    size_t* missArray = isPosInd ? this->missPosArr : this->missNegArr;

    missArray[posIndications]++;
}

Salsa2Parent& Salsa2Parent::getInstance(size_t caches)
{
    // Creates instance only for parent caches, not proxy
    // Check this is parent by check if have no peers in its conf file
    // because proxy know its parents, but oarents dont know proxy
    if (caches && Config.salsa2 && !Config.npeers && !instance)
    {
        debugs(96,DBG_CRITICAL,"Salsa2: Creates salsa2parent instance, with " << caches);
        instance = new Salsa2Parent(caches);
    }

    return *instance;
}