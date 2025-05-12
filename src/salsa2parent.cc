#include "salsa2.h"

Salsa2Parent* Salsa2Parent::instance = nullptr;

// TODO: find where to delete instance
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
    size_t* missArray = isPosInd ? this->missPosArr : this->missNegArr;

    missArray[posIndications]++;
}

