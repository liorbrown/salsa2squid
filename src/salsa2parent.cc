#include "salsa2.h"

Salsa2Parent* Salsa2Parent::instance = nullptr;

Salsa2Parent* Salsa2Parent::getInstance(size_t nCaches)
{
    if (!instance)
        instance = new Salsa2Parent(nCaches);
    
    return (instance);
}

// TODO: find where to delete instance
Salsa2Parent::~Salsa2Parent()
{
    if (this->missNegArr)
        delete[] missNegArr;

    if (this->missPosArr)
        delete[] this->missPosArr;
}

