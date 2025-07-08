#include "salsa2parent.h"

#define V_INIT (0.85)

Salsa2Parent* Salsa2Parent::instance = nullptr;

Salsa2Parent::Salsa2Parent(size_t caches): nCaches(caches)
{
    for (size_t i = 0; i <= caches; i++)     
        this->exclusionProbability[0][i] = V_INIT;
}

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

    if (this->clampingCounter)
        delete[] this->clampingCounter;
}

void Salsa2Parent::reEstimateExclusionProb(HttpRequest* request, HttpHeader* responseHeader)
{
    // Gets the relevant parameters according to request type (0 - speculative, 1 - regular)
    // and by number of pos indication for current request
    size_t& misses = this->missCounter[request->isPositive][request->posIndications];
    double& exclusionProb = this->exclusionProbability[request->isPositive][request->posIndications];
    double currDelta = this->delta[request->isPositive];

    // ReEstimate exclusion probability using EWMA
    exclusionProb = currDelta * misses / this->getReEstimateWindowSize() + 
        (1 - currDelta) * exclusionProb;
        
    responseHeader->addEntry(new HttpHeaderEntry(Http::OTHER,
                                                 SBuf("salsa2"),
                                                 std::to_string(exclusionProb).c_str()));

    std::stringstream stream;
    
    for (size_t i = 0; i < 2; i++)
    {
        stream << "\nIs possitive: " << i << " - ";
        
        for (size_t j = i; j < this->nCaches + i; j++)
            stream << '[' << j << "] " << this->missCounter[i][j] 
                    << " / " << this->reqCounter[i][j] 
                    << " = " << this->exclusionProbability[i][j] << " | ";
    }

    debugs(96, DBG_CRITICAL, "Salsa2: " << stream.str());

    misses = 0;
}

void Salsa2Parent::VClamping(size_t posIndications)
{
    debugs(96, DBG_CRITICAL, "Salsa2: Clamping v[" << posIndications << ']');

    double &trueNegativeProb = this->exclusionProbability[0][posIndications];

    // Minimize v[i] to V_INIT
    trueNegativeProb = min(trueNegativeProb, V_INIT);
}

void Salsa2Parent::newReq(HttpRequest::Pointer request, 
    const std::unordered_set<std::string>& posIndications)
{
    char hostname[100];
    gethostname(hostname, 100);

    size_t nPosIndications = posIndications.size();

    // Update request with the request salsa2 data 
    request->posIndications = nPosIndications;

    // The indication if this request is spectular or regular, is if this hostname
    // is in the list of squid hosts that give possitive indication.
    // Note that squid proxy can't send implicity to parent if its spectular or regular
    // because squid mechanism send the same request with same data to all parents
    request->isPositive = posIndications.find(hostname) != posIndications.end();
}

void Salsa2Parent::newMiss(HttpRequest::Pointer request)
{
    debugs(96, DBG_CRITICAL,"Salsa2: new Miss\n" << *request);

    // Sets request to miss
    request->isMiss = true;
}

void Salsa2Parent::reEstimateProbabilities(HttpRequest* request, HttpHeader* responseHeader)
{
    debugs(96, DBG_CRITICAL, "Salsa2: new ReEstimation\n" << *request);

    if (Salsa2Parent::isSalsa(request))
    {
        size_t &requests = this->reqCounter[request->isPositive][request->posIndications];
        size_t &clampingCount = this->clampingCounter[request->posIndications];
        size_t &missCount = this->missCounter[request->isPositive][request->posIndications];

        // Increase requests counter for current request type
        ++requests;

        // If current request is specular, add clamping count of v[i]
        clampingCount += !request->isPositive;

        // Increase missCounter if request missed
        missCount += request->isMiss;

        // Check if reach clamsing count, so clamp v[i] to V_INIT 
        if (clampingCount == this->getVClampInterval())
        {
            this->VClamping(request->posIndications);
            clampingCount = 0;
        }
            
        // Check if reach window size, so re-estimate exclusion probability
        if (requests == this->getReEstimateWindowSize())
        {
            this->reEstimateExclusionProb(request, responseHeader);
            requests = 0;
        }
    }
}

Salsa2Parent &Salsa2Parent::getInstance(size_t caches)
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
    std::unordered_set<std::string> &posIndications)
{
    debugs(96, DBG_CRITICAL, "salsa2: header: [" << header << ']');
    std::string segment;
    std::istringstream tokenizer(header.rawBuf());

    getline(tokenizer, segment, ',');
    nCaches = stoul(segment);

    while (std::getline(tokenizer, segment, ','))
        posIndications.insert(segment.substr(1));
}

void Salsa2Parent::newReq(HttpRequest::Pointer request)
{
    debugs(96, DBG_CRITICAL, "Salsa2: new req\n" << *request);

    if (Salsa2Parent::isSalsa(request))
    {                
        String salsaHeader = request->header.getByName("salsa2");
        size_t nCaches;
        std::unordered_set<std::string> posIndications;

        Salsa2Parent::parse(salsaHeader, nCaches, posIndications);

        Salsa2Parent::getInstance(nCaches).newReq(request, posIndications);
        
    }
}

std::ostream& operator<<(std::ostream& stream, HttpRequest &request)
{
    stream << "Req address = " << &request
           << "\nStore ID = " << request.storeId()
           << "\nclient address = " << request.client_addr
           << "\nisPossitive = " << std::boolalpha << request.isPositive
           << "\nposIndications = " << request.posIndications
           << "\nHeader =";

    for(auto entry : request.header.entries)
        stream << '\n' << entry->name.c_str() << ": " << entry->value;

    return stream;
}