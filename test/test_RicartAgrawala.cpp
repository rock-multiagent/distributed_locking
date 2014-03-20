#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

#include <iostream>

#include <distributed_locking/DLM.hpp>

#include "TestHelper.hpp"

using namespace fipa;
using namespace fipa::distributed_locking;
using namespace fipa::acl;

/**
 * Test taken from the ruby script.
 */
BOOST_AUTO_TEST_CASE(ricart_agrawala_test_from_ruby_script)
{
    std::cout << "ricart_agrawala_test_from_ruby_script" << std::endl;
    // Create 3 Agents
    Agent a1 ("agent1"), a2 ("agent2"), a3 ("agent3");
    // Define critical resource
    std::string rsc1 = "resource";
    // and a vector containing it
    std::vector<std::string> rscs;
    rscs.push_back(rsc1);
    
    // Create 3 DLMs
    DLM* dlm1 = DLM::dlmFactory(protocol::RICART_AGRAWALA, a1, rscs);
    DLM* dlm2 = DLM::dlmFactory(protocol::RICART_AGRAWALA, a2, std::vector<std::string>());
    DLM* dlm3 = DLM::dlmFactory(protocol::RICART_AGRAWALA, a3, std::vector<std::string>());

    // Now we lock
    dlm1->lock(rsc1, boost::assign::list_of(a2)(a3));
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));

    // And check it is being locked
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::LOCKED);

    // Agent two tries to lock
    dlm2->lock(rsc1, boost::assign::list_of(a1)(a3));
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1)(dlm3));
    // Agent 1 release
    dlm1->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));

    // Check it is being locked by a2 now
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::LOCKED);

    // A3 locks, sleep, A1 locks
    dlm3->lock(rsc1, boost::assign::list_of(a1)(a2));
    forwardAllMessages(boost::assign::list_of(dlm3)(dlm2)(dlm1));
    dlm1->lock(rsc1, boost::assign::list_of(a2)(a3));
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));

    // Unlock and see that he is NOT_INTERESTED
    dlm2->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1)(dlm3));
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::NOT_INTERESTED);

    // Check it is being locked by a3 now
    BOOST_CHECK(dlm3->getLockState(rsc1) == lock_state::LOCKED);

    // Agent 3 release
    dlm3->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm3)(dlm2)(dlm1));

    // Check it is being locked by a1 now
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::LOCKED);
    // Agent 1 releasese
    dlm1->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));

    // Check no one is interested any more
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::NOT_INTERESTED);
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::NOT_INTERESTED);
    BOOST_CHECK(dlm3->getLockState(rsc1) == lock_state::NOT_INTERESTED);
}

/**
 * Simple test with 3 agents, where a1 requests, obtains, and releases the lock for a resource
 */
BOOST_AUTO_TEST_CASE(ricart_agrawala_basic_hold_and_release)
{
    std::cout << "ricart_agrawala_basic_hold_and_release" << std::endl;
    // Create 3 Agents
    Agent a1 ("agent1"), a2 ("agent2"), a3 ("agent3");
    // Define critical resource
    std::string rsc1 = "resource";
    // and a vector containing it
    std::vector<std::string> rscs;
    rscs.push_back(rsc1);
    
    // Create 3 DLMs
    DLM* dlm1 = DLM::dlmFactory(protocol::RICART_AGRAWALA, a1, rscs);
    DLM* dlm2 = DLM::dlmFactory(protocol::RICART_AGRAWALA, a2, std::vector<std::string>());
    DLM* dlm3 = DLM::dlmFactory(protocol::RICART_AGRAWALA, a3, std::vector<std::string>());

    // dlm1 should not be interested in the resource.
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::NOT_INTERESTED);

    // Let dlm1 lock rsc1
    dlm1->lock(rsc1, boost::assign::list_of(a2)(a3));
    // He should be INTERESTED now
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::INTERESTED);

    // OutgoingMessages should contain one messages (2 receivers)
    BOOST_CHECK(dlm1->hasOutgoingMessages());
    ACLMessage dlm1msg = dlm1->popNextOutgoingMessage();
    BOOST_CHECK(!dlm1->hasOutgoingMessages());
    BOOST_REQUIRE_EQUAL(2, dlm1msg.getAllReceivers().size());

    // Lets foward this mail to our agents
    dlm2->onIncomingMessage(dlm1msg);
    dlm3->onIncomingMessage(dlm1msg);

    // They should now have responded to agent1
    BOOST_CHECK(dlm2->hasOutgoingMessages());
    BOOST_CHECK(dlm3->hasOutgoingMessages());
    ACLMessage dlm2msg = dlm2->popNextOutgoingMessage();
    ACLMessage dlm3msg = dlm3->popNextOutgoingMessage();
    BOOST_CHECK(!dlm2->hasOutgoingMessages());
    BOOST_CHECK(!dlm3->hasOutgoingMessages());

    // Lets forward the responses
    dlm1->onIncomingMessage(dlm2msg);
    dlm1->onIncomingMessage(dlm3msg);

    // Now, agent1 should hold the lock
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::LOCKED);

    // We release the lock and check it has been released
    dlm1->unlock(rsc1);
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::NOT_INTERESTED);
}

/**
 * Two agents want the same resource. First, a2 wants it _later_ than a1, then a1 wants it while
 * a2 holds the lock.
 */
BOOST_AUTO_TEST_CASE(ricart_agrawala_two_agents_conflict)
{
    std::cout << "ricart_agrawala_two_agents_conflict" << std::endl;
    // Create 2 Agents
    Agent a1 ("agent1"), a2 ("agent2");
    // Define critical resource
    std::string rsc1 = "resource";
    // and a vector containing it
    std::vector<std::string> rscs;
    rscs.push_back(rsc1);
    
    // Create 2 DLMs
    DLM* dlm1 = DLM::dlmFactory(protocol::RICART_AGRAWALA, a1, rscs);
    DLM* dlm2 = DLM::dlmFactory(protocol::RICART_AGRAWALA, a2, std::vector<std::string>());

    // Let dlm1 lock rsc1
    dlm1->lock(rsc1, boost::assign::list_of(a2));
    // OutgoingMessages should contain one messages
    BOOST_CHECK(dlm1->hasOutgoingMessages());
    ACLMessage dlm1msg = dlm1->popNextOutgoingMessage();
    BOOST_CHECK(!dlm1->hasOutgoingMessages());

    // Now (later) a2 tries to lock the same resource
    dlm2->lock(rsc1, boost::assign::list_of(a1));
    // OutgoingMessages should contain one messages
    BOOST_CHECK(dlm2->hasOutgoingMessages());
    ACLMessage dlm2msg = dlm2->popNextOutgoingMessage();
    BOOST_CHECK(!dlm2->hasOutgoingMessages());

    // Lets foward both mails
    dlm2->onIncomingMessage(dlm1msg);
    dlm1->onIncomingMessage(dlm2msg);

    // Only a2 should have responded by now
    BOOST_CHECK(dlm2->hasOutgoingMessages());
    BOOST_CHECK(!dlm1->hasOutgoingMessages());
    ACLMessage dlm2rsp = dlm2->popNextOutgoingMessage();
    BOOST_CHECK(!dlm2->hasOutgoingMessages());

    // Lets forward the response
    dlm1->onIncomingMessage(dlm2rsp);
    // Now, agent1 should hold the lock
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::LOCKED);
    // We release the lock and check it has been released
    dlm1->unlock(rsc1);
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::NOT_INTERESTED);
    // Now agent1 should have a new outgoing message, that has been deferred earlier
    BOOST_CHECK(dlm1->hasOutgoingMessages());
    ACLMessage dlm1rsp = dlm1->popNextOutgoingMessage();
    BOOST_CHECK(!dlm1->hasOutgoingMessages());

    // We forward it
    dlm2->onIncomingMessage(dlm1rsp);
    // Now, agent2 should hold the lock
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::LOCKED);

    // a1 requests the same resource again
    dlm1->lock(rsc1, boost::assign::list_of(a2));
    // Let us forward his message directly
    dlm2->onIncomingMessage(dlm1->popNextOutgoingMessage());
    // a2 should defer the response
    BOOST_CHECK(!dlm2->hasOutgoingMessages());

    // Now a2 releases the lock, and should have a new outgoing message, which we forward
    dlm2->unlock(rsc1);
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::NOT_INTERESTED);
    dlm1->onIncomingMessage(dlm2->popNextOutgoingMessage());

    // Finally a1 holds the lock again, and releases it again.
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::LOCKED);
    dlm1->unlock(rsc1);
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::NOT_INTERESTED);
    // Everyone is happy
}


/**
 * Two agents (one simulated) want one resource at the same time. The agent should revoke his interest.
 */
BOOST_AUTO_TEST_CASE(ricart_agrawala_same_time_conflict)
{
    std::cout << "ricart_agrawala_same_time_conflict" << std::endl;
    // Create 2 Agents, a2 is simulated
    Agent a1 ("agent1"), a2 ("agent2");
    // Define critical resource
    std::string rsc1 = "resource";
    // and a vector containing it
    std::vector<std::string> rscs;
    rscs.push_back(rsc1);
    
    // Create 1 DLM
    DLM* dlm1 = DLM::dlmFactory(protocol::RICART_AGRAWALA, a1, rscs);

    // Let dlm1 lock rsc1
    dlm1->lock(rsc1, boost::assign::list_of(a2));
    // Now he should be interested
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::INTERESTED);

    // Get the outgoing message
    ACLMessage dlm1msg = dlm1->popNextOutgoingMessage();

    // Simulated agent 2 has basically the same message. He "revoked" his interest after
    // "receiving" dlm1msg
    ACLMessage simMsg;
    simMsg.setPerformative(dlm1msg.getPerformative());
    simMsg.setContent(dlm1msg.getContent());
    simMsg.setSender(AgentID(a2.identifier));
    simMsg.addReceiver(dlm1msg.getSender());
    simMsg.setConversationID(a2.identifier + "0");
    simMsg.setProtocol(dlm1msg.getProtocol());

    // Send this message to a1
    dlm1->onIncomingMessage(simMsg);
    // Now he shouldn't be interested any more
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::NOT_INTERESTED);
}
