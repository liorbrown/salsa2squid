#include "salsa2parent.h"

#define DELTA_PI (0.25)

Salsa2Parent* Salsa2Parent::instance = nullptr;

Salsa2Parent::~Salsa2Parent()
{
    if (this->missCounter)
    {
        delete[] this->missCounter[0];
        delete[] this->missCounter[1];
        delete[] this->missCounter;
    }

    if (this->reqCounter)
    {
        delete[] this->reqCounter[0];
        delete[] this->reqCounter[1];
        delete[] this->reqCounter;
    }

    if (this->exclusionProbability)
    {
        delete[] this->exclusionProbability[0];
        delete[] this->exclusionProbability[1];
        delete[] this->exclusionProbability;
    }
}

void Salsa2Parent::reEstimateExclusionProb(HttpRequest::Pointer request)
{
    //the update is initiate by parent
    size_t& misses = this->missCounter[request->isPositive][request->posIndications];
    double& exclusionProb = this->exclusionProbability[request->isPositive][request->posIndications];

    exclusionProb = DELTA_PI * misses / this->reEstimateWindow + (1 - DELTA_PI) * exclusionProb;
    misses = 0;

    stringstream stream{"Misses counter:"};
    
    for (size_t i = 0; i < 2; i++)
    {
        stream << "\nIs possitive: " << i << " - ";
        
        for (size_t j = i; j < this->nCaches + i; j++)
            stream << '[' << j << "] " << this->missCounter[i][j] 
                    << " / " << this->reqCounter[i][j] 
                    << " = " << this->exclusionProbability[i][j] << " | ";
    }

    debugs(96, DBG_CRITICAL, "Salsa2: " << stream.str());
}

void Salsa2Parent::newReq(HttpRequest::Pointer request, const unordered_set<string>& posIndications)
{
    char hostname[100];
    gethostname(hostname, 100);

    size_t nPosIndications = posIndications.size();    
    request->posIndications = nPosIndications;

    request->isPositive = posIndications.find(hostname) != posIndications.end();
    
    size_t &requests = this->reqCounter[request->isPositive][nPosIndications];

    // Check if reach window size, so re-estimate exclusion probability
    // need to do it before increasing requests count, because its not yet counting the miss
    // if will be for this request
    if (requests && !(requests % this->reEstimateWindow))
        this->reEstimateExclusionProb(request);

    ++requests;

    debugs(96, DBG_CRITICAL,
        "\nrequest->posIndications = " << request->posIndications <<
        "\nrequest->isPositive = " << request->isPositive <<
        "\nNew request arrived: " << request->url.host() <<
        "\nNumber of positive indications: " << nPosIndications <<
        "\nNumber of requests for this amount: " << requests);
}

void Salsa2Parent::newMiss(HttpRequest::Pointer request)
{
    size_t misses = ++this->missCounter[request->isPositive][request->posIndications];

    debugs(96, DBG_CRITICAL,
           "Salsa2: Misses request: " << request->url.host() <<
            "\nNumber of positive indications for request: " << request->posIndications <<
            "\nNumber of misses for this amount: " << misses);
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