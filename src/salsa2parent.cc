#include "salsa2parent.h"

#define V_INIT (0.85)
#define DELTA_V (0.5)
#define DELTA_PI (0.25)

Salsa2Parent* Salsa2Parent::instance = nullptr;

Salsa2Parent::Salsa2Parent(size_t caches):
    delta{DELTA_V, DELTA_PI},
    nCaches(caches),
    updateInterval(30), // TODO: init now arbitary to 30, will be estimated ahead
    reqCounter(new size_t*[2]
        {new size_t[caches + 1]{0}, new size_t[caches + 1]{0}}),
    clampsingCounter(new size_t[caches + 1]{0}),
    missCounter(new size_t*[2] 
        {new size_t[caches + 1]{0}, new size_t[caches + 1]{0}}),
    exclusionProbability(new double*[2] 
        {new double[caches + 1], new double[caches + 1]{0.0}})
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
}

void Salsa2Parent::reEstimateExclusionProb(HttpRequest::Pointer request)
{
    // Gets the relevant parameters according to request type (0 - spectular, 1 - regular)
    // and by number of pos indication for current request
    size_t& misses = this->missCounter[request->isPositive][request->posIndications];
    double& exclusionProb = this->exclusionProbability[request->isPositive][request->posIndications];
    double currDelta = this->delta[request->isPositive];

    // ReEstimate exclusion probability using EWMA
    exclusionProb = currDelta * misses / this->getReEstimateWindowSize() + 
        (1 - currDelta) * exclusionProb;
        
    misses = 0;

    std::stringstream stream{"Misses counter:"};
    
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

void Salsa2Parent::VClampsing(size_t posIndications)
{
    debugs(96, DBG_CRITICAL, "Salsa2: Clampsing v[" << posIndications << ']');

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
    
    size_t &requests = this->reqCounter[request->isPositive][nPosIndications];
    size_t &clampsingCount = this->clampsingCounter[nPosIndications];

    // Check if reach window size, so re-estimate exclusion probability
    // need to do it before increasing requests count, because its not yet counting the miss
    // if will be for this request
    if (requests == this->getReEstimateWindowSize())
    {
        this->reEstimateExclusionProb(request);
        requests = 0;
    }

    // Check if reach clamsing count, so clamps v[i] to V_INIT 
    if (clampsingCount == this->getVClampsInterval())
    {
        this->VClampsing(nPosIndications);
        clampsingCount = 0;  
    }

    ++requests;
    ++clampsingCount;

    debugs(96, DBG_CRITICAL,
        "\nrequest->posIndications = " << request->posIndications <<
        "\nrequest->isPositive = " << request->isPositive <<
        "\nNew request arrived: " << request->url.host() <<
        "\nNumber of positive indications: " << nPosIndications <<
        "\nNumber of requests for this amount: " << requests);
}

void Salsa2Parent::newMiss(HttpRequest::Pointer request)
{
    // Update miss counter for relevant request
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