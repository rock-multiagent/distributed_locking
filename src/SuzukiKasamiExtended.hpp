#ifndef DISTRIBUTED_LOCKING_SUZUKI_KASAMI_EXTENDED_HPP
#define DISTRIBUTED_LOCKING_SUZUKI_KASAMI_EXTENDED_HPP

#include "SuzukiKasami.hpp"
#include "Agent.hpp"
#include <fipa_acl/fipa_acl.h>


namespace fipa {
namespace distributed_locking {
/**
 * Extension of the Suzuki Kasami algorithm. PROBE->SUCCESS messages have been added, to check if agents are alive.
 * Also, the token is always forwarded via the resource owner, which makes it possible for him to keep track of the
 * token Owner and realize its failure.
 */
class SuzukiKasamiExtended : public SuzukiKasami
{
public:
    /**
     * The implemented protocol
     */
    static const protocol::Protocol protocol;
    
    /**
     * Default constructor
     */
    SuzukiKasamiExtended();
    /**
     * Constructor
     */
    SuzukiKasamiExtended(const Agent& self, const std::vector<std::string>& resources);
    
    /**
     * Forwards the token to the next person in the queue, via the resource owner.
     */
    virtual void forwardToken(const std::string& resource);
    /**
     * Return whether the given agent owns (owned last) the token for the given resource. This algorithm
     * extension keeps track of that.
     */
    virtual bool isTokenHolder(const std::string& resource, const std::string& agentName);
    /**
     * Send the token to the receiver and sends PROBEs if neccesary.
     */
    virtual void sendToken(const fipa::acl::AgentID& receiver, const std::string& resource, const std::string& conversationID);
    /**
     * Handles an incoming response
     */
    virtual void handleIncomingResponse(const fipa::acl::ACLMessage& message);
    /**
     * Tries to lock a resource. Subsequently, isLocked() must be called to check the status.
     */
    virtual void lock(const std::string& resource, const std::list<Agent>& agents);
    
private:
    // The (logical) token holders of the owned resources. Maps resource->agent.
    // Will be equivalent to mLckHolders MOST OF THE TIME.
    std::map<std::string, std::string> mTokenHolders;

};
} // namespace distributed_locking
} // namespace fipa

#endif // DISTRIBUTED_LOCKING_SUZUKI_KASAMI_EXTENDED_HPP