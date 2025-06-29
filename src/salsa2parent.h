#include <cstddef>
#include <unordered_set>
#include "squid.h"
#include "SquidConfig.h"
#include "HttpRequest.h"

#define DELTA_V (0.5)
#define DELTA_PI (0.25)
#define MIN_UPDATE_INTERVAL (1)

/// @brief This is singltone class that contain for this cache 
/// all the statistic data needed for salsa2
class Salsa2Parent{
    private:
        const double delta[2]{DELTA_V, DELTA_PI};
        const size_t nCaches;
        size_t updateInterval = MIN_UPDATE_INTERVAL;
        bool deltaMode = false;

        // All statistics matrices have 2 rows, first for spectular requests
        // (this cahce have negative indication)
        // and seconed for regular requests (this cahce have possitive indication)
        
        size_t** reqCounter = new size_t*[2]
            {new size_t[nCaches + 1]{0}, new size_t[nCaches + 1]{0}};

        size_t* clampingCounter = new size_t[nCaches + 1]{0};

        size_t** missCounter = new size_t*[2] 
            {new size_t[nCaches + 1]{0}, new size_t[nCaches + 1]{0}};

        double** exclusionProbability = new double*[2]
            {new double[nCaches + 1], new double[nCaches + 1]{0.0}};
    
        Salsa2Parent(size_t caches);

        ~Salsa2Parent();

        /// @brief Tihs methos calles when new request arrived
        /// this is update the num of requests that sended for certain number of 
        /// positive indications
        /// @param request Current request
        /// @param posIndications List of caches that gave possitive indication to proxy
        void newReq(HttpRequest::Pointer request, 
            const std::unordered_set<std::string>& posIndications);

        /// @brief Get window size of re estimate exclusion probability
        /// @return The window size of re estimate exclusion probability
        size_t getReEstimateWindowSize(){return (max((this->updateInterval / 10), (size_t)1));}

        /// @brief Get window size of when to clamp v[i] to V_INIT
        /// for ensure its not make and stay to high, and then it will not get requests
        /// any more and this will damgage our statistics
        /// @return The window size of when to clamp v[i]
        size_t getVClampInterval(){return (this->updateInterval * 10);}
        
        /// @brief Updating exclusionProbability according to given request details
        /// @param request This request tells salsa2 which probalitiy we need to estimate
        /// @param request Response header to put on the estimation
        void reEstimateExclusionProb(HttpRequest* request, HttpHeader* responseHeader);

        /// @brief Minimize v[i] to v_init
        /// @param posIndications Number of posIndications to clamp its v[i]
        void VClamping(size_t posIndications);

        /// @brief Checks if given request is handling by salsa2
        /// @param request 
        /// @return True - if handle by salsa2, False - otherwise
        static bool isSalsa(HttpRequest::Pointer request)
        {return (Config.salsa2 &&
                 !Config.npeers && 
                 request->header.getByName("salsa2").size());}

        /// @brief Parse "salsa2" header entry
        /// @param header "salsa2" header entry
        /// @param nCaches Returns the number of parents
        /// @param posIndications Returns set of cahces 
        /// that gave to proxy positive indication for this request
        static void parse
            (const String header, 
             size_t& nCaches, 
             std::unordered_set<std::string>& posIndications);

        static Salsa2Parent* instance;
        
    public:
        /// @brief Notify salsa for getting new request
        /// @param request New request that arrived
        static void newReq(HttpRequest::Pointer request);

        /// @brief Called when this cache miss request.
        /// Updates the number of misses
        /// @param request The requset that missed
        void newMiss(HttpRequest::Pointer request);

        /// @brief Updating exclusionProbability according to given request details
        /// @param request This request tells salsa2 which probalitiy we need to estimate
        /// @param request Response header to put on the estimation
        void reEstimateProbabilities(HttpRequest* request, HttpHeader* responseHeader);

        static Salsa2Parent& getInstance(size_t caches = 0);                

        /// @brief Called when this server shutdown 
        static void free(){if (instance) delete instance;}
};

/// @brief For debug, print request data
/// @param stream Stream to write into in
/// @param request The request to write
/// @return Given stream
std::ostream& operator<<(std::ostream& stream, HttpRequest &request);