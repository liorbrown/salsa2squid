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

    
    if (this->possExclusionProbability)
        delete[] this->possExclusionProbability;

    if (this->negExclusionProbability)
        delete[] this->negExclusionProbability;
}

void Salsa2Parent::newReq(HttpRequest::Pointer request, const unordered_set<string>& posIndications)
{
    char hostname[100];
    gethostname(hostname, 100);

    size_t nPosIndications = posIndications.size();    
    request->posIndications = nPosIndications;

    request->isPositive = posIndications.find(hostname) != posIndications.end();    

    debugs(96, DBG_CRITICAL,
        "\nrequest->posIndications = " << request->posIndications <<
        "\nrequest->isPositive = " << request->isPositive <<
        "\nNew request arrived: " << request->url.host() <<
        "\nNumber of positive indications: " << nPosIndications <<
        "\nNumber of requests for this amount: " << ++this->reqNum[nPosIndications]);
}

void Salsa2Parent::newMiss(HttpRequest::Pointer request)
{
    // Checks if need to update the number of misses for possitive indication
    // or for negative indication
    size_t* missArray = request->isPositive ? this->missPosArr : this->missNegArr;

    debugs(96, DBG_CRITICAL,
           "Salsa2: Misses request: " << request->url.host() <<
            "\nNumber of positive indications for request: " << request->posIndications <<
            "\nNumber of misses for this amount: " << ++missArray[request->posIndications]);
    
     for (size_t i = 0; i <= this->nCaches; i++)
        debugs(96, DBG_CRITICAL, '[' << i << "] : " << this->missPosArr[i]);

    debugs(96, DBG_CRITICAL, "Salsa2: missNegArr:");
    
    for (size_t i = 0; i <= this->nCaches; i++)
        debugs(96, DBG_CRITICAL, '[' << i << "] : " << this->missNegArr[i]);
    
}

Salsa2Parent& Salsa2Parent::getInstance(size_t caches)
{
    // Creates instance only for parent caches, not proxy
    // Check this is parent by check if have no peers in its conf file
    // because proxy know its parents, but parents dont know proxy
    if (caches && Config.salsa2 && !Config.npeers && !instance)
    {
        debugs(96,DBG_CRITICAL,"Salsa2: Creates salsa2parent instance, with " << caches);
        instance = new Salsa2Parent(caches);
    }

    return *instance;
}

void Salsa2Parent::parse(
    const String header, 
    size_t &nCaches, 
    unordered_set<string> &posIndications)
{
    debugs(96, DBG_CRITICAL, "salsa2: header: [" << header << ']');
    string segment;
    istringstream tokenizer(header.rawBuf());

    getline(tokenizer, segment, ',');
    nCaches = stoul(segment);

    while (std::getline(tokenizer, segment, ','))
        posIndications.insert(segment.substr(1));
}