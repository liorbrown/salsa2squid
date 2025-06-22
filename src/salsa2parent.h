#include <cstddef>
#include <unordered_set>
#include "squid.h"
#include "SquidConfig.h"
#include "HttpRequest.h"

/// @brief This is singltone class that contain for this cache 
/// all the statistic data needed for salsa2
class Salsa2Parent{
    private:
        const double delta[2];
        const size_t nCaches;
        size_t updateInterval;

        // All statistics matrices have 2 rows, first for spectular requests
        // (this cahce have negative indication)
        // and seconed for regular requests (this cahce have possitive indication)
        size_t** reqCounter;
        size_t* clampsingCounter;
        size_t** missCounter;
        double** exclusionProbability;
    
        Salsa2Parent(size_t caches);

        ~Salsa2Parent();

        /// @brief Get window size of re estimate exclusion probability
        /// @return The window size of re estimate exclusion probability
        size_t getReEstimateWindowSize(){return (this->updateInterval / 10);}

        /// @brief Get window size of when to clamps v[i] to V_INIT
        /// for ensure its not make and stay to high, and then it will not get requests
        /// any more and this will damgage our statistics
        /// @return The window size of when to clamps v[i]
        size_t getVClampsInterval(){return (this->updateInterval * 10);}
        
        /// @brief Updating exclusionProbability according to given request details
        /// @param request This request tells salsa2 which probalitiy we need to estimate
        void reEstimateExclusionProb(HttpRequest::Pointer request);

        /// @brief Minimize v[i] to v_init
        /// @param posIndications Number of posIndications to clamps its v[i]
        void VClampsing(size_t posIndications);

        static Salsa2Parent* instance;
    public:

        /// @brief Tihs methos calles when new request arrived
        /// , this is update the num of requests that sended for certain number of 
        /// positive indications
        /// @param request Current request
        /// @param posIndications List of caches that gave possitive indication to proxy
        void newReq(HttpRequest::Pointer request, 
            const std::unordered_set<std::string>& posIndications);
        
        /// @brief Called when this cache miss request.
        /// Updates the number of misses
        /// @param request The requset that missed
        void newMiss(HttpRequest::Pointer request);

        static Salsa2Parent& getInstance(size_t caches = 0);

        /// @brief Checks if given request is handling by salsa2
        /// @param request 
        /// @return True - if handle by salsa2, False - otherwise
        static bool isSalsa(HttpRequest::Pointer request)
            {return request->header.getByName("salsa2").size();}

        /// @brief Parse "salsa2" header entry
        /// @param header "salsa2" header entry
        /// @param nCaches Returns the number of parents
        /// @param posIndications Returns set of cahces 
        /// that gave to proxy positive indication for this request
        static void parse
            (const String header, 
             size_t& nCaches, 
             std::unordered_set<std::string>& posIndications);

        /// @brief Called when this server shutdown 
        static void free(){if (instance) delete instance;}
};