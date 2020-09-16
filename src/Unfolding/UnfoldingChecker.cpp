#include "UnfoldingChecker.h"
#include <math.h>
#include <tuple>

namespace uc
{

    class g_var
    {
    public:
        static unsigned int nb_events;
        static unsigned int nb_traces;
        static EventSet U;
        static EventSet G;
        static EventSet gD;
    };

    unsigned int g_var::nb_events = 0;
    unsigned int g_var::nb_traces = 0;
    EventSet g_var::U;
    EventSet g_var::G;
    EventSet g_var::gD;

    void print_debug(const UnfoldingEvent *e, EventSet const &eset)
    {
        std::cout << "id: " << e->id;
        std::cout << "\n";
        std::cout << "tr: "
                  << e->transition.id
                  << ", "
                  << e->transition.actor_id
                  << ", "
                  << e->transition.mailbox_id
                  << ", "
                  << e->transition.commId
                  << ", "
                  << e->transition.executed
                  << ", "
                  << e->transition.access_var
                  << ", "
                  << e->transition.lockId
                  << ", "
                  << e->transition.mutexID
                  << ", "
                  << e->transition.read_write;
        std::cout << "\n";
        for (auto i : e->appState.actors_)
        {
            std::cout << "a: " << i.id << ", " << i.nb_trans;
            std::cout << "\n";
            for (auto j : i.trans)
            {
                std::cout << "a.tr: "
                          << j.id
                          << ", "
                          << j.actor_id
                          << ", "
                          << j.read_write
                          << ", "
                          << j.executed;
                std::cout << "\n";
            }
        }
        std::string enc_events{""};
        for (auto i : eset)
        {
            enc_events += std::to_string(i->id) + ", ";
        }
        std::cout << "enc_events: " << enc_events;
        std::cout << "\n\n";
    }

    // std::tuple<check_is_true, return_from_func, normal_end>
    std::tuple<bool, bool, bool> func(UnfoldingEvent *it, std::list<UnfoldingEvent*>& EvtList1, EventSet &J, unsigned int n, unsigned long sizeD, int inter)
    {
        bool check = false;
        for (auto evt : EvtList1)
        {
            if (it->isConflict(it, evt))
            {
                check = true;
                break;
            }
        }
        if (check)
            return std::make_tuple(true, false, false);

        // if inter > 0 intS already has a presentative, pop the old one and add the new one

        if (inter > 0)
            EvtList1.pop_back();
        EvtList1.push_back(it);

        if ((n == sizeD - 1) && (EvtList1.size() == sizeD))
        {
            for (auto evt : EvtList1)
                EvtSetTools::pushBack(J, evt);
            return std::make_tuple(false, true, false);
        }
        return std::make_tuple(false, false, true);
    }

    /* this function select one event in every EventSet in a list to form a set,
    *  if they are not conflict with each other -> return the set J
    */
    void ksubset(unsigned long sizeD, std::list<UnfoldingEvent *> EvtList, std::list<EventSet> list, unsigned int n,
                 EventSet &J)
    {
        if (J.size() > 0)
            return;

        std::list<UnfoldingEvent *> EvtList1 = EvtList;
        std::list<UnfoldingEvent *> EvtList2;

        if (list.size() > 0)
        {
            EventSet intS = list.back();

            std::list<EventSet> list1 = list;
            list1.pop_back();

            auto inter = 0;
            for (auto it:intS)
            {
                auto tuple = func(it, EvtList1, J, n, sizeD, inter);
                auto check = std::get<0>(tuple);
                if(check)
                    continue;
                auto exit = std::get<1>(tuple);
                if(exit)
                    return;
                if (J.size() == 0)
                    ksubset(sizeD, EvtList1, list1, n + 1, J);
                inter++;
            }
        }
    }

    void check_event_to_remove(UnfoldingEvent *evt, EventSet& evtS, EventSet const &D, Configuration const& C)
    {
        EventSet subC;
        EventSet sub_history;
        EventSet history = evt->getHistory();
        EvtSetTools::pushBack(history, evt);
        bool chk = false;

        if (!EvtSetTools::isEmptyIntersection(history, D))
            chk = true;
        else
        {
            for (auto it1 : C.events_)
            {
                if (it1->isConflict(it1, evt))
                {
                    chk = true;
                    break;
                }
            }
        }
        if (chk)
            EvtSetTools::remove(evtS, evt);
    }

    bool remove_conflict_with_c(std::list<EventSet> const &kSet, std::list<EventSet> &kSet1, EventSet const &D, Configuration const& C)
    {
        for (auto it : kSet)
        {
            EventSet evtS = it;
            for (auto evt : it)
            {
                check_event_to_remove(evt, evtS, D, C);
            }

            if (evtS.empty())
            {
                return false;
            }

            kSet1.push_back(evtS);
        }
        return true;
    }

    EventSet UnfoldingChecker::KpartialAlt(EventSet D, Configuration C) const
    {
        EventSet J;
        EventSet J1;
        EventSet emptySet;
        EventSet result;
        std::list<EventSet> kSet;

        /*for each evt in D, add all evt1 in U that is conflict with evt to a set(spike) */

        for (auto evt : D)
        {
            EventSet evtSet;
            for (auto evt1 : g_var::U)
                if (evt->isConflict(evt, evt1))
                    EvtSetTools::pushBack(evtSet, evt1);

            kSet.push_back(evtSet);
        }

        if (kSet.size() < D.size())
            return emptySet;

        std::list<EventSet> kSet1;
        auto ret = remove_conflict_with_c(kSet, kSet1, D, C);
        if (!ret)
            return emptySet;

        // building J by chosing one event in each spike such that there are not 2 conflict events in J
        auto n = 0;
        std::list<UnfoldingEvent *> EvtList;

        ksubset(D.size(), EvtList, kSet1, n, J1);

        // J1.size() can be < D.size() since one event in J1 can be conflict with some events in D

        if (J1.size() == 0)
        {
            std::cout << "there is no optimal solution (no alternative) J1 size = " << J1.size() << " \n";
            return emptySet;
        }

        for (auto evt : J1)
        {
            EventSet history = evt->getHistory();
            J = EvtSetTools::makeUnion(J, history);
            EvtSetTools::pushBack(J, evt);
        }

        return J;
    }

    bool transition_is_IReceive(EventSet const &testedEvtHist, EventSet const& ancestorsHist, EventSet const &ancestors, const UnfoldingEvent *testedEvt)
    {
        auto nbReceive = 0;
        for (auto evt : testedEvtHist)
        {
            if (evt->transition.type == "Ireceive" && evt->transition.mailbox_id == testedEvt->transition.mailbox_id)
                nbReceive++;
        }

        auto nbSend1 = 0;
        for (auto evt : ancestorsHist)
        {
            if (evt->transition.type == "Isend" && evt->transition.mailbox_id == testedEvt->transition.mailbox_id)
                nbSend1++;
        }

        for (auto it : ancestors)
        {
            if (it->transition.type == "Isend" && it->transition.mailbox_id == testedEvt->transition.mailbox_id)
                nbSend1++;
        }

        if (nbReceive != nbSend1)
            return false;
        return true;
    }

    bool transition_is_ISend(EventSet const &testedEvtHist, EventSet const &ancestorsHist, EventSet const &ancestors, const UnfoldingEvent *testedEvt)
    {
        auto nbSend = 0;
        for (auto evt : testedEvtHist)
        {
            if (evt->transition.type == "Isend" && evt->transition.mailbox_id == testedEvt->transition.mailbox_id)
                nbSend++;
        }

        auto nbReceive1 = 0;
        for (auto evt : ancestorsHist)
        {
            if (evt->transition.type == "Ireceive" && evt->transition.mailbox_id == testedEvt->transition.mailbox_id)
                nbReceive1++;
        }

        for (auto it : ancestors)
        {
            if (it->transition.type == "Ireceive" && it->transition.mailbox_id == testedEvt->transition.mailbox_id)
                nbReceive1++;
        }
 
        if (nbSend != nbReceive1)
            return false;
        
        return true;
    }

    bool checkSdRcCreation(Transition const &trans, EventSet ancestors, Configuration C)
    {

        int nbTest = 0;//, nbSend = 0, nbReceive = 0;
        UnfoldingEvent *testEvt;
        for (auto evt : ancestors)
        {
            if (evt->transition.type == "Test" && evt->transition.actor_id != trans.actor_id)
            {
                nbTest++;
                testEvt = evt;
            }
        }

        // one send/receive can not concern more than one communication
        if (nbTest > 1)
            return false;
        if (nbTest == 0)
            return true;

        UnfoldingEvent *testedEvt = C.findTestedComm(testEvt);

        //  two sends or receives can not be in the communiation -> not depend

        if (testedEvt->transition.type == trans.type || testedEvt->transition.mailbox_id != trans.mailbox_id)
            return false;

        EventSet testedEvtHist = testedEvt->getHistory();
        EventSet ancestorsHist;

        for (auto evt : ancestors)
            ancestorsHist = EvtSetTools::makeUnion(ancestorsHist, evt->getHistory());

        // check nb send == nb receive ?, if yes they concern the same comm -> test and send/receive are dependent

        if (testedEvt->transition.type == "Isend")
        {
            auto ret = transition_is_ISend(testedEvtHist, ancestorsHist, ancestors, testedEvt);
            if(!ret)
                return ret;
        }

        if (testedEvt->transition.type == "Ireceive")
        {
            return transition_is_IReceive(testedEvtHist, ancestorsHist, ancestors, testedEvt);
        }

        return true;
    }

    /* this function creating new events from a transition and a set cadidate directed ancestors (ancestorSet)
    *  (the set includes events that can be a direct ancestor)
    *  by combination the transition and all subset (cause)  of the set ancestorSet
    */
    bool check_contains(bool chk, EventSet const& cause, const UnfoldingEvent *immPreEvt)
    {
        bool chk1 = false;
        // if immediate precede event is not in the causuality_events, ensure that it is in the history of one event in
        // cause
        if (!chk)
        {
            for (auto evt : cause)
            {
                if (EvtSetTools::contains(evt->getHistory(), immPreEvt))
                {
                    chk1 = true;
                    break;
                }
            }
        }
        return chk1;
    }

    void Configuration::createEvts(Configuration C, EventSet &result, const app::Transition &t, s_evset_in_t ev_sets, bool chk, UnfoldingEvent *immPreEvt)
    {
        auto causuality_events = ev_sets.causuality_events;
        auto cause = ev_sets.cause;
        auto ancestorSet = ev_sets.ancestorSet;

        if (ancestorSet.empty())
        {

            auto chk1 = check_contains(chk, cause, immPreEvt);

            /* create a new evt with directed ancestors are cause1,
            * if all conditions are passed (trans is enabled, any ancestors are not in the history of other ancestors)
            */
            if (chk || chk1)
            {
                bool send_receiveCheck = true;
                EventSet cause1;
                cause1 = EvtSetTools::makeUnion(cause, causuality_events);
                // check condition for send/receive action or check enabled for MutexWait action

                if (t.type == "Isend" || t.type == "Ireceive")
                {
                    send_receiveCheck = checkSdRcCreation(t, cause1, C);
                }

                if (send_receiveCheck)
                {
                    g_var::nb_events++;
                    UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, t, cause1);
                    EvtSetTools::pushBack(result, e);
                }
            }
            return;
        }

        else
        {
            UnfoldingEvent *a = *(ancestorSet.begin());
            EventSet evtSet1, evtSet2, evtSet3, evtSet4;
            evtSet1 = cause;
            evtSet3 = ancestorSet;
            EvtSetTools::pushBack(evtSet1, a);
            EvtSetTools::remove(evtSet3, a);
            s_evset_in_t evsets{causuality_events, evtSet1, evtSet3};
            createEvts(C, result, t, evsets, chk, immPreEvt);
            evtSet2 = cause;
            evtSet4 = ancestorSet;
            EvtSetTools::remove(evtSet4, a);
            evsets.causuality_events = causuality_events;
            evsets.cause = evtSet2;
            evsets.ancestorSet = evtSet4;
            createEvts(C, result, t, evsets, chk, immPreEvt);
        }
    }

    // gives a event in actorMaxEvent has the id = a given id
    UnfoldingEvent *Configuration::findActorMaxEvt(int id)
    {
        UnfoldingEvent *immPreEvt = nullptr;
        for (auto evt : this->actorMaxEvent)
            if (evt->transition.actor_id == id)
                immPreEvt = evt;
        return immPreEvt;
    }

    void create_events_from_trans_and_maxEvent(std::tuple<Configuration, bool, Transition const&> t_params,
                            EventSet const& causalityEvts, UnfoldingEvent *immPreEvt, EventSet const& ancestorSet, 
                            std::list<EventSet>& maxEvtHistory, EventSet &exC)
    {
        auto C = std::get<0>(t_params);
        auto chk = std::get<1>(t_params); 
        auto trans = std::get<2>(t_params); 

        EventSet cause;
        EventSet exC1;
        EventSet H;
        
        //EventSet causuality_events, EventSet cause, EventSet ancestorSet
        s_evset_in evsets = {causalityEvts, cause, ancestorSet};
        C.createEvts(C, exC1, trans, evsets, chk, immPreEvt);
        exC = EvtSetTools::makeUnion(exC, exC1);

        // remove last MaxEvt, sine we already used it in the above
        maxEvtHistory.pop_back();

        /*2. We now compute new evts by using MaxEvts in the past, but we have to ensure that
            * the ancestor events (used to generate history candidate) are not in history of the evts in the causalityEvts
            */
        std::vector<int> intS;
        // get all events in the history of the evts in causalityEvts
        for (auto evt : causalityEvts)
        {
            EventSet H1;
            H1 = evt->getHistory();
            H = EvtSetTools::makeUnion(H, H1);
        }

        // put id of evts in H in the the set intS
        for (auto evt : H)
            intS.push_back(evt->id);

        /* compute a set of evt that can generate history (all subset of it)
             by getting evts in the maximal evts but not in the history of causalityEvts*/

        for (auto evtSet : maxEvtHistory)
        {
            EventSet evtS;

            // put ids of events that are not in the history of evts in causalityEvts into a set intS1
            // if history candidate is not empty then try create new evts from its subset
            if (!evtSet.empty())
            {

                // retrieve  evts in congig from intS1 (intS1 store id of evts in C whose transitions are dependent with trans)
                EventSet evtSet1;
                for (auto evt : evtSet)
                    if ((!EvtSetTools::contains(H, evt)) && evt->transition.isDependent(trans))
                        EvtSetTools::pushBack(evtSet1, evt);

                EventSet exC1;
                EventSet cause;
                s_evset_in_t evsets = {causalityEvts, cause, evtSet1};
                C.createEvts(C, exC1, trans, evsets, chk, immPreEvt);
                exC = EvtSetTools::makeUnion(exC, exC1);
            }
        }
    }

    /* this function produces new events from a given transition (trans) and the maxEvtHistory*/

    EventSet computeExt(Configuration C, std::list<EventSet> maxEvtHistory, Transition trans)
    {
        bool chk = false;
        EventSet causalityEvts;
        EventSet exC, ancestorSet, H;
        UnfoldingEvent *immPreEvt = nullptr;

        // add causality evts to causalityEvts set, firstly add last event to causalityEvts
        EvtSetTools::pushBack(causalityEvts, C.lastEvent);

        /* add the immediate precede evt of transition trans to the causalityEvts
        *  used to make sure trans is enabled (Ti is enlabled if Ti-1 is aready fined)
        chk == true =>  causalityEvts contains immediate precede event of trans Or the immediate precede event is in history
        of lastEvt.
        */

        if (trans.id == 0)
            chk = true; // if trans.id ==0 => trans is always enabled do not need to check enable condition ?.

        else
        {
            // if immediate precede evt in maxEvent, add it to the causalityEvts
            for (auto evt : C.maxEvent)
                if (trans.actor_id == evt->transition.actor_id)
                {
                    EvtSetTools::pushBack(causalityEvts, evt);
                    chk = true;
                    break;
                }

            // else find it in actorMaxEvent to add it into causalityEvts
            immPreEvt = C.findActorMaxEvt(trans.actor_id);
            if (!chk)
                if (EvtSetTools::contains(C.lastEvent->getHistory(), immPreEvt))
                    chk = true;
        }

        for (auto evt : C.maxEvent)
            if (trans.isDependent(evt->transition) && (!EvtSetTools::contains(causalityEvts, evt)))
                EvtSetTools::pushBack(ancestorSet, evt);

        /*1. Create events from current (last) maximal event of C */
        // 1.1 if only last evt and immidiate precede event are dependent with trans -> only one evt is created
        if (causalityEvts.size() <= 2 && ancestorSet.size() == 0)
        {

            g_var::nb_events++;

            /* in this case only one event is created, since all MaxEvts are in the history of lastEvt*/
            UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, trans, causalityEvts);
            EvtSetTools::pushBack(exC, e);
        }

        else
        {
            // 1.2 else create events from trans and events in current maxEvent
            auto t_params = std::make_tuple(C, chk, trans);
            create_events_from_trans_and_maxEvent(t_params, causalityEvts, immPreEvt, ancestorSet, maxEvtHistory, exC);
        }
        return exC;
    }

    /* this function creates new events from a wait transition (trans),
this wait waits a communication (action send/receive) in the the parameter/event evt
The idea here is that, we try to march the communication with all possible communication to crete a complete
communication, making wait become enabled. When the wait enable, we can create new events
*/

    EventSet createWaitEvt(const UnfoldingEvent *evt, Configuration C, Transition const &trans)
    {
        EventSet evtS;
        int nbSdRc = 0;
        EventSet hist = evt->getHistory();
        int mbId = evt->transition.mailbox_id;
        std::string comType = evt->transition.type;

        if (comType == "Isend")
        {
            // if waited communication is  send, count the number of send request before the communication

            for (auto evt1 : hist)
                if (evt1->transition.type == "Isend" && evt1->transition.mailbox_id == mbId)
                    nbSdRc++;

            // try to march the communication with all possible receice request
            for (auto evt2 : C.events_)
                if (evt2->transition.type == "Ireceive" && evt2->transition.mailbox_id == mbId)
                {
                    // after find ount a receive request
                    EventSet hist1 = evt2->getHistory();
                    int nbRc = 0;

                    // count the number of receice requests before the receive that we found above

                    for (auto evt3 : hist1)
                        if (evt3->transition.type == "Ireceive" && evt3->transition.mailbox_id == mbId)
                            nbRc++;
                    if (nbSdRc == nbRc)
                    {

                        // if the number send = number receive, we can march the send communication  with the rececive
                        EventSet ancestors;
                        UnfoldingEvent *maxEvt = C.findActorMaxEvt(evt->transition.actor_id);
                        EventSet maxEvtHist = maxEvt->getHistory();

                        if (EvtSetTools::contains(maxEvtHist, evt2))
                            EvtSetTools::pushBack(ancestors, maxEvt);
                        else if (EvtSetTools::contains(hist1, maxEvt))
                            EvtSetTools::pushBack(ancestors, evt2);
                        else
                        {
                            EvtSetTools::pushBack(ancestors, maxEvt);
                            EvtSetTools::pushBack(ancestors, evt2);
                        }
                        g_var::nb_events++;
                        UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, trans, ancestors);
                        EvtSetTools::pushBack(evtS, e);
                    }
                }
        }
        // do the same for a receive
        else if (comType == "Ireceive")
        {
            // if waited communication is send, count the number of send request before the communication

            for (auto evt1 : hist)
                if (evt1->transition.type == "Ireceive" && evt1->transition.mailbox_id == mbId)
                    nbSdRc++;

            // try to march the communication with all possible receice request
            for (auto evt2 : C.events_)
                if (evt2->transition.type == "Isend" && evt2->transition.mailbox_id == mbId)
                {
                    // after find out a receive request
                    EventSet hist1 = evt2->getHistory();
                    int nbRc = 0;

                    // count the number of receice requests before the receive that we found above

                    for (auto evt3 : hist1)
                        if (evt3->transition.type == "Isend" && evt3->transition.mailbox_id == mbId)
                            nbRc++;
                    if (nbSdRc == nbRc)
                    {
                        // if the number send = number receive, we can march the send communication  with the rececive
                        EventSet ancestors;
                        UnfoldingEvent *maxEvt = C.findActorMaxEvt(evt->transition.actor_id);
                        EventSet maxEvtHist = maxEvt->getHistory();

                        if (EvtSetTools::contains(maxEvtHist, evt2))
                            EvtSetTools::pushBack(ancestors, maxEvt);
                        else if (EvtSetTools::contains(hist1, maxEvt))
                            EvtSetTools::pushBack(ancestors, evt2);
                        else
                        {
                            EvtSetTools::pushBack(ancestors, maxEvt);
                            EvtSetTools::pushBack(ancestors, evt2);
                        }
                        g_var::nb_events++;
                        UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, trans, ancestors);
                        EvtSetTools::pushBack(evtS, e);
                    }
                }
        }

        return evtS;
    }

    EventSet createTestEvt(EventSet exC, UnfoldingEvent *evt, Configuration C, Transition trans)
    {

        EventSet evtS;
        int numberSend = 0, numberReceive = 0;
        EventSet hist = evt->getHistory();
        EventSet lastEvtHist = C.lastEvent->getHistory();

        int mbId = evt->transition.mailbox_id;
        std::string comType = evt->transition.type;
        UnfoldingEvent *newEvt1, *newEvt2;
        EventSet ancestors;

        /* since action test is always enabled, create a new event although there is no pending Ireceive
       * to march with the send (create ewEvt1)
       *
                                                       /--------\
                                                      /          \
                 evt=<Isend,ancestors>     evt2=<IReceive,ancestors>
                                                / \           /
                                               /	 \  	   /
                                              /	  \       /
                                         /       \     /
      ewEvt1 =<Test1,(e1, e2)>     newEvt2 =<Test1, (e1, e2)>
       * */

        /*1. last evt = pre evt -> create at least one event
   *						   |-> pre evt is in the history of last evt
   *2. last evt != pre evt =>  |-> TRY TO create one evt
                               |-> pre evt is not in the history of last evt
   */

        // count the number of isend/ireceive befere evt
        if (comType == "Isend")
        {
            for (auto evt1 : hist)
                if (evt1->transition.type == "Isend" && evt1->transition.mailbox_id == mbId)
                    numberSend++;
        }
        else if (comType == "Ireceive")
        {
            for (auto evt1 : hist)
                if (evt1->transition.type == "Ireceive" && evt1->transition.mailbox_id == mbId)
                    numberReceive++;
        }

        // if last evt is the pre evt -> create at least one evt
        if (C.lastEvent->transition.actor_id == trans.actor_id)
        // maxEvt =  C.findActorMaxEvt(evt->transition.actor_id);
        {
            EventSet maxEvtHist = C.lastEvent->getHistory();

            EvtSetTools::pushBack(ancestors, C.lastEvent);
            g_var::nb_events++;
            newEvt1 = new UnfoldingEvent(g_var::nb_events, trans, ancestors);

            EvtSetTools::pushBack(evtS, newEvt1);

            /* Now try to create newEvt2 if there is a pending communication can be march
           with the communication tested by the Test action */

            if (comType == "Isend")
            {

                // try to march the communication with all possible receice request
                for (auto evt2 : C.events_)
                    if (evt2->transition.type == "Ireceive" && evt2->transition.mailbox_id == mbId and
                        (!EvtSetTools::contains(lastEvtHist, evt2)))
                    {
                        // after find out a receive request

                        EventSet hist2 = evt2->getHistory();
                        int nbReceive = 0;

                        // count the number of receice requests before the receive that we found above

                        for (auto evt3 : hist2)
                            if (evt3->transition.type == "Ireceive" && evt3->transition.mailbox_id == mbId)
                                nbReceive++;
                        if (numberSend == nbReceive)
                        {
                            /* if the number send = number receive, we can march the send communication with the rececive
             * -> create an new event ( newEvt2 in the figure) */

                            EvtSetTools::pushBack(ancestors, evt2);
                            g_var::nb_events++;
                            newEvt2 = new UnfoldingEvent(g_var::nb_events, trans, ancestors);

                            EvtSetTools::pushBack(evtS, newEvt2);
                        }
                    }
            }
            // do the same for a receive

            else if (comType == "Ireceive")
            {

                // try to march the communication with all possible receice request
                for (auto evt2 : C.events_)
                    if (evt2->transition.type == "Isend" && evt2->transition.mailbox_id == mbId and
                        (!EvtSetTools::contains(lastEvtHist, evt2)))
                    {
                        // after find out a receive request

                        EventSet hist2 = evt2->getHistory();
                        int nbSend = 0;

                        // count the number of receice requests heppen before the receive that we found above

                        for (auto evt3 : hist2)
                            if (evt3->transition.type == "Isend" && evt3->transition.mailbox_id == mbId)
                                nbSend++;
                        if (numberReceive == nbSend)
                        {

                            /* if the number send = number receive, we can march the send communication with the rececive
             * -> create an new event ( newEvt2 in the figure) */

                            EvtSetTools::pushBack(ancestors, evt2);
                            g_var::nb_events++;
                            newEvt2 = new UnfoldingEvent(g_var::nb_events, trans, ancestors);
                            EvtSetTools::pushBack(evtS, newEvt2);
                        }
                    }
            }
        }

        // If the last event is evt2 (in figure), try to march evt2 with evt (in figure)
        else
        {

            if (comType == "Isend")
            {
                int nbReceive = 0;

                for (auto evt2 : lastEvtHist)
                    if (evt2->transition.type == "Ireceive")
                        nbReceive++;
                if (numberSend == nbReceive)
                {

                    UnfoldingEvent *maxEvt = C.findActorMaxEvt(evt->transition.actor_id);
                    EvtSetTools::pushBack(ancestors, C.lastEvent);
                    if (!EvtSetTools::contains(lastEvtHist, maxEvt))
                        EvtSetTools::pushBack(ancestors, maxEvt);
                    g_var::nb_events++;
                    newEvt2 = new UnfoldingEvent(g_var::nb_events, trans, ancestors);
                    EvtSetTools::pushBack(evtS, newEvt2);
                }
            }
            else if (comType == "Ireceive")
            {

                int nbSend = 0;
                for (auto evt2 : lastEvtHist)
                    if (evt2->transition.type == "Isend")
                        nbSend++;

                if (numberReceive == nbSend)
                {
                    UnfoldingEvent *maxEvt = C.findActorMaxEvt(evt->transition.actor_id);

                    EvtSetTools::pushBack(ancestors, C.lastEvent);
                    if (!EvtSetTools::contains(lastEvtHist, maxEvt))
                        EvtSetTools::pushBack(ancestors, maxEvt);
                    g_var::nb_events++;
                    newEvt2 = new UnfoldingEvent(g_var::nb_events, trans, ancestors);
                    EvtSetTools::pushBack(evtS, newEvt2);
                }
            }
        }

        return evtS;
    }

    EventSet createIsendEvts(Transition trans, Configuration C)
    {

        bool enableChk = false;

        EventSet exC, EvtSet, causalityEvts, ancestorSet;
        UnfoldingEvent *testedEvt, *immPreEvt;

        /* if trans is not dependent with the last transition -> return */

        if (C.lastEvent->transition.type == "Test" && C.lastEvent->transition.actor_id != trans.actor_id)
        {
            // get the communication tested by Test
            testedEvt = C.findTestedComm(C.lastEvent);

            // two sends or two receives can not be in the same communication -> not denpendent
            if (trans.type == testedEvt->transition.type || trans.mailbox_id != testedEvt->transition.mailbox_id)
                return EvtSet;
        }

        // if trans.id ==0 => trans is always enabled do not need to check enable condition ?.
        if (trans.id == 0 || C.lastEvent->transition.actor_id == trans.actor_id)
            enableChk = true;
        /* enableChk = true -> trans is ensured enabled or his pre evt is in the causalityEvts  */
        else if (C.lastEvent->transition.actor_id != trans.actor_id && (!enableChk))
        {
            // else find it in actorMaxEvent and check where it is in the history of last Evt
            immPreEvt = C.findActorMaxEvt(trans.actor_id);
            if (EvtSetTools::contains(C.lastEvent->getHistory(), immPreEvt))
                enableChk = true;
        }

        immPreEvt = C.findActorMaxEvt(trans.actor_id);

        EventSet lastEvtHist = C.lastEvent->getHistory();

        EventSet immPreEvtHist;
        if (trans.id != 0)
            immPreEvtHist = immPreEvt->getHistory();

        EvtSetTools::pushBack(ancestorSet, C.lastEvent);

        // if last event is preEvt(trans) always create a new event.
        if (C.lastEvent->transition.actor_id == trans.actor_id)
        {
            g_var::nb_events++;
            UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, trans, ancestorSet);
            EvtSetTools::pushBack(exC, e);
        }
        // else if last event is Isend try to create a new event .
        else if (C.lastEvent->transition.type == "Isend")
        {

            if (enableChk)
            {
                g_var::nb_events++;
                UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, trans, ancestorSet);
                EvtSetTools::pushBack(exC, e);
            }
            else
            {
                EvtSetTools::pushBack(ancestorSet, immPreEvt);
                g_var::nb_events++;
                UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, trans, ancestorSet);
                EvtSetTools::pushBack(exC, e);
            }
        }

        // if last evt = preEvt(Isend) or last evt is a Isend, then try to combine with Test
        if (trans.isDependent(C.lastEvent->transition))
            for (auto it : C.events_)
                if (it->transition.type == "Test" && it->transition.mailbox_id == trans.mailbox_id)
                {
                    UnfoldingEvent *testedEvt;
                    // retrive the communication tested by the the
                    testedEvt = C.findTestedComm(it);

                    // tested action is Ireceive, create a new event if it can be marched with the Isend
                    // make sure no ancestor candidate event is in history of other ancestor candidate

                    if (testedEvt->transition.type == "Ireceive" && (!EvtSetTools::contains(lastEvtHist, it)))
                        if (!EvtSetTools::contains(immPreEvtHist, it))
                        {
                            EventSet ancestorSet1;
                            EvtSetTools::pushBack(ancestorSet1, C.lastEvent);
                            EvtSetTools::pushBack(ancestorSet1, it);
                            EventSet itHist = it->getHistory();
                            if ((!EvtSetTools::contains(itHist, immPreEvt)) && (!enableChk))
                                EvtSetTools::pushBack(ancestorSet1, immPreEvt);

                            if (checkSdRcCreation(trans, ancestorSet1, C))
                            {
                                g_var::nb_events++;
                                UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, trans, ancestorSet1);
                                EvtSetTools::pushBack(exC, e);
                            }
                        }
                }

        // else if last event is a Test
        if (C.lastEvent->transition.actor_id == trans.actor_id ||
            (C.lastEvent->transition.actor_id != trans.actor_id && C.lastEvent->transition.type == "Test"))
        {
            EventSet ansestors;
            EvtSetTools::pushBack(ansestors, C.lastEvent);
            if (!enableChk)
                EvtSetTools::pushBack(ansestors, immPreEvt);
            if (checkSdRcCreation(trans, ansestors, C))
            {
                g_var::nb_events++;
                UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, trans, ansestors);
                EvtSetTools::pushBack(exC, e);
            }

            for (auto it : C.events_)
                if (trans.isDependent(it->transition) && trans.actor_id != it->transition.actor_id)
                {
                    // make sure no ancestor candidate event is in history of other ancestor candidate
                    EventSet ansestors1;
                    EvtSetTools::pushBack(ansestors1, C.lastEvent);

                    if (!EvtSetTools::contains(lastEvtHist, it) && (!EvtSetTools::contains(immPreEvtHist, it)))
                    {
                        EvtSetTools::pushBack(ansestors1, it);
                        EventSet itHist = it->getHistory();
                        if ((!enableChk) && (!EvtSetTools::contains(itHist, immPreEvt)))
                            EvtSetTools::pushBack(ansestors1, immPreEvt);

                        if (checkSdRcCreation(trans, ansestors1, C))
                        {
                            g_var::nb_events++;
                            UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, trans, ansestors1);
                            EvtSetTools::pushBack(exC, e);
                        }
                    }
                }
        }

        // now combine send and test actions, forming a ancestors set including 3 events

        if (C.lastEvent->transition.actor_id == trans.actor_id)
            for (auto sEvt : C.events_)
                if (sEvt->transition.type == "Isend" && sEvt->transition.mailbox_id == trans.mailbox_id &&
                    sEvt->transition.actor_id != trans.actor_id && (!EvtSetTools::contains(lastEvtHist, sEvt)))
                {

                    EventSet rEvtHist = sEvt->getHistory();
                    for (auto tEvt : C.events_)
                        if (tEvt->transition.type == "Test" && tEvt->transition.actor_id != trans.actor_id &&
                            tEvt->transition.mailbox_id == trans.mailbox_id)
                        {
                            UnfoldingEvent *testedEvent = C.findTestedComm(tEvt);
                            if (testedEvent->transition.type == "Ireveive" && (!EvtSetTools::contains(lastEvtHist, tEvt)) &&
                                !(EvtSetTools::contains(rEvtHist, tEvt)) && (!EvtSetTools::contains(tEvt->getHistory(), sEvt)))
                            {
                                EventSet ancestorsSet;
                                EvtSetTools::pushBack(ancestorsSet, C.lastEvent);
                                EvtSetTools::pushBack(ancestorsSet, sEvt);
                                EvtSetTools::pushBack(ancestorsSet, tEvt);

                                if (checkSdRcCreation(trans, ancestorsSet, C))
                                {
                                    g_var::nb_events++;
                                    UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, trans, ancestorsSet);
                                    EvtSetTools::pushBack(exC, e);
                                }
                            }
                        }
                }

        return exC;
    }

    EventSet createIreceiveEvts(Transition trans, Configuration C)
    {
        bool enableChk = false;

        EventSet exC, EvtSet, causalityEvts, ancestorSet;
        UnfoldingEvent *testedEvt, *immPreEvt;

        /* if trans is not dependent with the last transition -> return */

        if (C.lastEvent->transition.type == "Test" && C.lastEvent->transition.actor_id != trans.actor_id)
        {
            // get the communication tested by Test
            testedEvt = C.findTestedComm(C.lastEvent);

            // two sends or two receives can not be in the same communication -> not denpendent
            if (trans.type == testedEvt->transition.type || trans.mailbox_id != testedEvt->transition.mailbox_id)
                return EvtSet;
        }

        // if trans.id ==0 => trans is always enabled do not need to check enable condition ?.
        if (trans.id == 0 || C.lastEvent->transition.actor_id == trans.actor_id)
            enableChk = true;
        /* enableChk = true -> trans is ensured enabled or his pre evt is in the causalityEvts  */
        else if (C.lastEvent->transition.actor_id != trans.actor_id && (!enableChk))
        {
            // else find it in actorMaxEvent and check where it is in the history of last Evt
            immPreEvt = C.findActorMaxEvt(trans.actor_id);
            if (EvtSetTools::contains(C.lastEvent->getHistory(), immPreEvt))
                enableChk = true;
        }

        immPreEvt = C.findActorMaxEvt(trans.actor_id);
        EventSet lastEvtHist = C.lastEvent->getHistory();
        EventSet immPreEvtHist;
        if (trans.id != 0)
            immPreEvtHist = immPreEvt->getHistory();

        EvtSetTools::pushBack(ancestorSet, C.lastEvent);

        // if last event is preEvt(trans) always create a new event.
        if (C.lastEvent->transition.actor_id == trans.actor_id)
        {
            g_var::nb_events++;
            UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, trans, ancestorSet);
            EvtSetTools::pushBack(exC, e);
        }
        // else if last event is Ireceive try to create a new event .
        else if (C.lastEvent->transition.type == "Ireceive")
        {
            if (enableChk)
            {
                g_var::nb_events++;
                UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, trans, ancestorSet);
                EvtSetTools::pushBack(exC, e);
            }
            else
            {
                EvtSetTools::pushBack(ancestorSet, immPreEvt);
                g_var::nb_events++;
                UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, trans, ancestorSet);
                EvtSetTools::pushBack(exC, e);
            }
        }

        // if last evt = preEvt(Ireceive) or last evt is a Ireceive, then try to combine with Test
        if (trans.isDependent(C.lastEvent->transition))
            for (auto it : C.events_)
                if (it->transition.type == "Test" && it->transition.mailbox_id == trans.mailbox_id)
                {
                    UnfoldingEvent *testedEvt;
                    // retrive the communication tested by the the
                    testedEvt = C.findTestedComm(it);

                    // tested action is Ireceive, create a new event if it can be marched with the Isend
                    // make sure no ancestor candidate event is in history of other ancestor candidate

                    if (testedEvt->transition.type == "Isend" && (!EvtSetTools::contains(lastEvtHist, it)))
                        if (!EvtSetTools::contains(immPreEvtHist, it))
                        {

                            EventSet ancestorSet1;
                            EvtSetTools::pushBack(ancestorSet1, C.lastEvent);
                            EvtSetTools::pushBack(ancestorSet1, it);
                            EventSet itHist = it->getHistory();
                            if ((!EvtSetTools::contains(itHist, immPreEvt)) && (!enableChk))
                                EvtSetTools::pushBack(ancestorSet1, immPreEvt);

                            if (checkSdRcCreation(trans, ancestorSet1, C))
                            {
                                g_var::nb_events++;
                                UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, trans, ancestorSet1);
                                EvtSetTools::pushBack(exC, e);
                            }
                        }
                }

        // else if last event is a Test
        if (C.lastEvent->transition.actor_id == trans.actor_id ||
            (C.lastEvent->transition.actor_id != trans.actor_id && C.lastEvent->transition.type == "Test"))
        {
            EventSet ansestors;
            EvtSetTools::pushBack(ansestors, C.lastEvent);
            if (!enableChk)
                EvtSetTools::pushBack(ansestors, immPreEvt);
            if (checkSdRcCreation(trans, ansestors, C))
            {
                g_var::nb_events++;
                UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, trans, ansestors);
                EvtSetTools::pushBack(exC, e);
            }

            for (auto it : C.events_)
                if (trans.isDependent(it->transition) && trans.actor_id != it->transition.actor_id)
                {
                    // make sure no ancestor candidate event is in history of other ancestor candidate
                    EventSet ansestors1;
                    EvtSetTools::pushBack(ansestors1, C.lastEvent);

                    if (!EvtSetTools::contains(lastEvtHist, it) && (!EvtSetTools::contains(immPreEvtHist, it)))
                    {
                        EvtSetTools::pushBack(ansestors1, it);
                        EventSet itHist = it->getHistory();
                        if ((!enableChk) && (!EvtSetTools::contains(itHist, immPreEvt)))
                            EvtSetTools::pushBack(ansestors1, immPreEvt);

                        if (checkSdRcCreation(trans, ansestors1, C))
                        {
                            g_var::nb_events++;
                            UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, trans, ansestors1);
                            EvtSetTools::pushBack(exC, e);
                        }
                    }
                }
        }

        // now combine send and test actions, forming a ancestors set including 3 events

        if (C.lastEvent->transition.actor_id == trans.actor_id)
            for (auto rEvt : C.events_)
                if (rEvt->transition.type == "Ireceive" && rEvt->transition.mailbox_id == trans.mailbox_id &&
                    rEvt->transition.actor_id != trans.actor_id && (!EvtSetTools::contains(lastEvtHist, rEvt)))
                {
                    EventSet rEvtHist = rEvt->getHistory();
                    for (auto tEvt : C.events_)
                        if (tEvt->transition.type == "Test" && tEvt->transition.actor_id != trans.actor_id &&
                            tEvt->transition.mailbox_id == trans.mailbox_id)
                        {
                            UnfoldingEvent *testedEvent = C.findTestedComm(tEvt);
                            if (testedEvent->transition.type == "Isend" && (!EvtSetTools::contains(lastEvtHist, tEvt)) &&
                                !(EvtSetTools::contains(rEvtHist, tEvt)) && (!EvtSetTools::contains(tEvt->getHistory(), rEvt)))
                            {
                                EventSet ancestorsSet;
                                EvtSetTools::pushBack(ancestorsSet, C.lastEvent);
                                EvtSetTools::pushBack(ancestorsSet, rEvt);
                                EvtSetTools::pushBack(ancestorsSet, tEvt);

                                if (checkSdRcCreation(trans, ancestorsSet, C))
                                {
                                    g_var::nb_events++;
                                    UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, trans, ancestorsSet);

                                    EvtSetTools::pushBack(exC, e);
                                }
                            }
                        }
                }
        return exC;
    }

    /* given a send/receive transition and a history candidate represented by maxmal event (ancestors), this function
   check whether we can create a event ?
*/

    EventSet createSendReceiveEvts(Transition trans, Configuration C, std::list<EventSet> maxEvtHistory)
    {
        EventSet exC, EvtSet, causalityEvts, ancestorSet, H;
        UnfoldingEvent *testedEvt = nullptr, *immPreEvt = nullptr;
        bool chk = false;

        /* if trans is not dependent with the last transition -> return
   if the last transition is a test -> check whether they concern the same communication ?*/

        if (C.lastEvent->transition.type == "Test" && C.lastEvent->transition.actor_id != trans.actor_id)
        {

            // get the communication tested by Test
            for (auto evt : C.events_)
                if (evt->transition.actor_id == C.lastEvent->transition.actor_id &&
                    evt->transition.commId == C.lastEvent->transition.commId && evt->transition.type != "Test")

                {
                    testedEvt = evt;
                    break;
                }
            // two sends or two receives can not be in the same communication -> not denpendent
            if (trans.type == testedEvt->transition.type || trans.mailbox_id != testedEvt->transition.mailbox_id)
                return EvtSet;
        }

        // add causality evts to causalityEvts set, firstly add last event to causalityEvts
        EvtSetTools::pushBack(causalityEvts, C.lastEvent);

        /* add the immediate precede evt of transition trans to the causalityEvts
   *  used to make sure trans is enabled (Ti is enlabled if Ti-1 is aready fined)
   chk == true =>  causalityEvts contains immediate precede event of trans Or the immediate precede event is in history
   of lastEvt.
   */

        if (trans.id == 0 || C.lastEvent->transition.actor_id == trans.actor_id)
            chk = true; // if trans.id ==0 => trans is always enabled do not need to check enable condition ?.

        /* chk = true -> trans is ensured enabled or his pre evt is in the causalityEvts  */

        else if (C.lastEvent->transition.actor_id != trans.actor_id && (!chk))
        {

            // if immediate precede evt in maxEvent, add it to the causalityEvts

            for (auto evt : C.maxEvent)
                if (trans.actor_id == evt->transition.actor_id)
                {
                    EvtSetTools::pushBack(causalityEvts, evt);
                    chk = true;
                    break;
                }

            // else find it in actorMaxEvent and check where it is in the history of last Evt

            if (!chk)
            {

                immPreEvt = C.findActorMaxEvt(trans.actor_id);
                if (EvtSetTools::contains(C.lastEvent->getHistory(), immPreEvt))
                    chk = true;
            }
        }

        // compute all events are dependent with the transition trans.

        for (auto evt : C.maxEvent)
            if ((trans.isDependent(evt->transition) ||
                 (evt->transition.type == "Test" && evt->transition.mailbox_id == trans.mailbox_id)) &&
                (!EvtSetTools::contains(causalityEvts, evt)))
                EvtSetTools::pushBack(ancestorSet, evt);

        if (checkSdRcCreation(trans, causalityEvts, C) && chk)
        {
            g_var::nb_events++;
            UnfoldingEvent *e = new UnfoldingEvent(g_var::nb_events, trans, causalityEvts);
            EvtSetTools::pushBack(exC, e);
        }

        EventSet cause;
        EventSet exC1;
        s_evset_in_t evsets = {causalityEvts, cause, ancestorSet};
        C.createEvts(C, exC1, trans, evsets, chk, immPreEvt);
        exC = EvtSetTools::makeUnion(exC, exC1);

        // remove last MaxEvt, sine we already used it before
        maxEvtHistory.pop_back();

        /*2. We now compute new evts by using MaxEvts in the past, but we have to ensure that
   * the ancestor events (used to generate history candidate) are not in history of the evts in the causalityEvts
   */
        /* get all events in the history of the evts in causalityEvts */
        for (auto evt : causalityEvts)
        {
            EventSet H1;
            H1 = evt->getHistory();
            H = EvtSetTools::makeUnion(H, H1);
        }

        for (auto evtSet : maxEvtHistory)
        {

            EventSet evtS;

            // if intS1 is not empty then retrive thier events, create new events from all subsets
            if (!evtSet.empty())
            {

                // retrieve  evts from intS1 (intS1 store id of evts in C whose transitions are dependent with trans)
                EventSet evtSet1;
                for (auto evt : evtSet)
                    if ((!EvtSetTools::contains(H, evt)) &&
                        (evt->transition.isDependent(trans) ||
                         (evt->transition.type == "Test" && trans.mailbox_id == evt->transition.mailbox_id)))

                        EvtSetTools::pushBack(evtSet1, evt);

                EventSet exC1, cause;
                s_evset_in_t evsets{causalityEvts, cause, evtSet1};
                C.createEvts(C, exC1, trans, evsets, chk, immPreEvt);

                exC = EvtSetTools::makeUnion(exC, exC1);
            }
        }

        return exC;
    }

    void UnfoldingChecker::extend(std::deque<Actor> actors, Configuration C, std::list<EventSet> maxEvtHistory, EventSet &exC,
                                  EventSet &enC) const
    {

        // in the initial state each actor creates one event
        EventSet causes;
        if (C.events_.empty())
        {
            // auto all_tr0 = app_side_->get_all_tr0_tags(0);
            // for(auto tag:all_tr0)
            // {
            //     g_var::nb_events++;
            //     UnfoldingEvent *newEvent = new UnfoldingEvent(g_var::nb_events, tag, causes);
            //     if (!EvtSetTools::contains(g_var::U, newEvent))
            //     {
            //         EvtSetTools::pushBack(g_var::U, newEvent);
            //         EvtSetTools::pushBack(enC, newEvent);
            //         EvtSetTools::pushBack(exC, newEvent);
            //     }
            //     else
            //     {
            //         auto evt1 = EvtSetTools::find(g_var::U, newEvent);
            //         EvtSetTools::pushBack(enC, evt1);
            //         auto evt2 = EvtSetTools::find(g_var::U, newEvent);
            //         EvtSetTools::pushBack(exC, evt2);
            //     }
            // }

            for (auto p : actors)
            {
                g_var::nb_events++;
                UnfoldingEvent *newEvent = new UnfoldingEvent(g_var::nb_events, p.trans[0], causes);
                if (!EvtSetTools::contains(g_var::U, newEvent))
                {
                    EvtSetTools::pushBack(g_var::U, newEvent);
                    EvtSetTools::pushBack(enC, newEvent);
                    EvtSetTools::pushBack(exC, newEvent);
                }
                else
                {
                    auto evt1 = EvtSetTools::find(g_var::U, newEvent);
                    EvtSetTools::pushBack(enC, evt1);
                    auto evt2 = EvtSetTools::find(g_var::U, newEvent);
                    EvtSetTools::pushBack(exC, evt2);
                }
            }
        }
        else
        {
            // TODO: implement get_enabled_transitions()
            // get all enabled transitions at current appState
            std::deque<Transition> enabledTransitions;
            enabledTransitions = C.lastEvent->appState.getEnabledTransition();

            auto state_id = C.lastEvent->get_state_id();
            std::deque<std::string> enabled_trans_tags = app_side_->get_enabled_transitions(state_id); 

            // try to create new events from a enabled transition and every maximal_Evt history in maxEvtHistory of C
            // TODO: TR
            std::vector<bool> v;
            auto tag = C.lastEvent->transition.get_tr_tag();
            std::vector<std::string> types = {"Wait", "Test", "Isend", "Ireceive", "localComp"};
            std::vector<bool> equality_vec;
            std::vector<std::string> enabled_tr_types;
            for (auto t : enabled_trans_tags)
            {
                auto equality = app_side_->check_transition_type(t, types);
                auto tr_type = app_side_->get_transition_type(t);
                equality_vec.push_back(!equality);
                enabled_tr_types.push_back(tr_type);
                v.push_back(app_side_->check_transition_dependency(t, tag));
            }

            for (auto trans : enabledTransitions)
            {
                // TODO: eliminate next 2 lines
                auto comp = trans.isDependent(C.lastEvent->transition);
                auto eq = trans.type != "Wait" && trans.type != "Test" &&
                    trans.type != "Isend" && trans.type != "Ireceive" && trans.type != "localComp";

                // if trans is not a wait action, and is dependent with the transition of last event
                if (trans.isDependent(C.lastEvent->transition) && trans.type != "Wait" && trans.type != "Test" &&
                    trans.type != "Isend" && trans.type != "Ireceive" && trans.type != "localComp")
                {

                    EventSet exC1, causalityEvts;

                    // in maxEvent of C, we only consider events having transitions that are dependent with trans
                    if (trans.read_write < 2)
                    {
                        std::list<EventSet> maxEvtHistory1 = maxEvtHistory;

                        exC1 = computeExt(C, maxEvtHistory1, trans);
                    }

                    for (auto newEvent : exC1)
                        if (!EvtSetTools::contains(g_var::U, newEvent))
                        {
                            EvtSetTools::pushBack(g_var::U, newEvent);
                            EvtSetTools::pushBack(exC, newEvent);
                        }
                        else
                        {
                            auto evt = EvtSetTools::find(g_var::U, newEvent);
                            EvtSetTools::pushBack(exC, evt);
                        }
                }

                // ELSE IF THE TRANSITION IS A isend or i receive

                else if (trans.type == "Isend" and
                         (trans.isDependent(C.lastEvent->transition) ||
                          (C.lastEvent->transition.type == "Test" && C.lastEvent->transition.mailbox_id == trans.mailbox_id)))
                {
                    EventSet exC1 = createIsendEvts(trans, C);
                    for (auto newEvent : exC1)
                        if (!EvtSetTools::contains(g_var::U, newEvent))
                        {
                            EvtSetTools::pushBack(g_var::U, newEvent);
                            EvtSetTools::pushBack(exC, newEvent);
                        }
                        else
                        {
                            auto evt = EvtSetTools::find(g_var::U, newEvent);
                            EvtSetTools::pushBack(exC, evt);
                        }
                }

                else if (trans.type == "Ireceive" &&
                         (trans.isDependent(C.lastEvent->transition) ||
                          (C.lastEvent->transition.type == "Test" && C.lastEvent->transition.mailbox_id == trans.mailbox_id)))
                {
                    EventSet exC1 = createIreceiveEvts(trans, C);
                    for (auto newEvent : exC1)
                        if (!EvtSetTools::contains(g_var::U, newEvent))
                        {
                            EvtSetTools::pushBack(g_var::U, newEvent);
                            EvtSetTools::pushBack(exC, newEvent);
                        }
                        else
                        {
                            auto evt = EvtSetTools::find(g_var::U, newEvent);
                            EvtSetTools::pushBack(exC, evt);
                        }
                }
                // ELSE IF THE TRANSITION IS A WAIT ACTION
                else if (trans.type == "Wait")
                {

                    // check which kind of communication (send/receive) waited by the wait?
                    UnfoldingEvent *evt = nullptr;
                    for (auto evt1 : C.events_)
                        if (evt1->transition.actor_id == trans.actor_id && evt1->transition.commId == trans.commId)
                        {
                            evt = evt1;
                            break;
                        }

                    /* we only call function createWaitEvt if the last action is send/ or receive
                    or dependent with the wait (transition in the same actor) */

                    std::string comType = C.lastEvent->transition.type;
                    std::string comType1 = evt->transition.type;

                    if (C.lastEvent->transition.actor_id == trans.actor_id || (comType == "Isend" && comType1 == "Ireceive") or
                        (comType == "Ireceive" && comType1 == "Isend"))
                    {
                        EventSet newEvts = createWaitEvt(evt, C, trans);

                        // EventSet unionSet = U.makeUnion(U,gD1);

                        for (auto newEvent : newEvts)
                            if (!EvtSetTools::contains(g_var::U, newEvent))
                            {
                                EvtSetTools::pushBack(g_var::U, newEvent);
                                EvtSetTools::pushBack(exC, newEvent);
                            }
                            else
                            {
                                auto evt = EvtSetTools::find(g_var::U, newEvent);
                                EvtSetTools::pushBack(exC, evt);
                            }
                    }
                }

                // ELSE IF THE TRANSITION IS A TEST ACTION

                else if (trans.type == "Test")
                {

                    // check which kind of communication (send/receive) tested by the test?

                    UnfoldingEvent *event = nullptr;
                    for (auto evt1 : C.events_)
                        if (evt1->transition.actor_id == trans.actor_id && evt1->transition.commId == trans.commId)
                        {
                            event = evt1;
                            break;
                        }

                    /* we only call function createTestEvt if the last action is send or receive
                    or dependent with the test (transition in the same actor) */

                    std::string comType = C.lastEvent->transition.type;
                    std::string comType1 = event->transition.type;

                    if (C.lastEvent->transition.actor_id == trans.actor_id || (comType == "Isend" && comType1 == "Ireceive") or
                        (comType == "Ireceive" && comType1 == "Isend"))

                    {
                        EventSet newEvts = createTestEvt(exC, event, C, trans);

                        for (auto newEvent : newEvts)
                            if (!EvtSetTools::contains(g_var::U, newEvent))
                            {

                                EvtSetTools::pushBack(g_var::U, newEvent);
                                EvtSetTools::pushBack(exC, newEvent);
                            }
                            else
                            {
                                auto evt = EvtSetTools::find(g_var::U, newEvent);
                                EvtSetTools::pushBack(exC, evt);
                            }
                    }
                }

                else if (trans.type == "localComp" && C.lastEvent->transition.actor_id == trans.actor_id)
                {
                    EventSet ancestors;
                    EvtSetTools::pushBack(ancestors, C.lastEvent);
                    g_var::nb_events++;
                    UnfoldingEvent *newEvent = new UnfoldingEvent(g_var::nb_events, trans, ancestors);
                    if (!EvtSetTools::contains(g_var::U, newEvent))
                    {
                        EvtSetTools::pushBack(g_var::U, newEvent);
                        EvtSetTools::pushBack(exC, newEvent);
                    }
                    else
                    {
                        auto evt = EvtSetTools::find(g_var::U, newEvent);
                        EvtSetTools::pushBack(exC, evt);
                    }
                }
            }

            for (auto evt : exC)
            {
                /* add new event evt to enC if evt's transition is not dependent with any transition of a event
                which is in C and is not in history of evt */
                bool chk = true;
                EventSet evtHisty = evt->getHistory();
                for (auto it : C.events_)
                    if (it->isConflict(it, evt))
                        chk = false;

                if (chk)
                    EvtSetTools::pushBack(enC, evt);
            }
        }
    }

    void UnfoldingChecker::explore(std::deque<Actor> actors, std::deque<Mailbox> mailboxes)
    {
        EventSet A, D;
        Configuration C;
        EventSet prev_exC;

        // TODO: develop and call a create_state() without input arguments
        auto state_actors = actors;
        auto state_mbs = mailboxes;
        auto state_id = app_side_->create_state(std::move(state_actors), std::move(state_mbs));
        app_side_->initialize(actors, mailboxes);

        auto initState = new State(actors.size(), actors, mailboxes);
        // auto *e = new UnfoldingEvent(initState);
        // TODO: call another constructor
        auto *e = new UnfoldingEvent(initState, state_id);

        explore(C, {EventSet()}, D, A, e, prev_exC, actors);
    }

    void UnfoldingChecker::explore(State *state)
    {
        EventSet A, D;
        Configuration C;
        EventSet prev_exC;

        auto local_actors = state->actors_;
        auto local_mbs = state->mailboxes_;
        app_side_->create_state(std::move(local_actors), std::move(local_mbs));

        explore(C, {EventSet()}, D, A, new UnfoldingEvent(state), prev_exC, state->actors_);
        std::cout.flush();
        if (g_var::nb_traces != confs_expected_.size())
        {
            std::cerr << "ERROR: " << confs_expected_.size() << " traces expected, but " << g_var::nb_traces << " observed.\n";
            error_++;
        }
        if (g_var::nb_events != expected_events_)
        {
            std::cerr << "ERROR: " << expected_events_ << " unique events expected, but " << g_var::nb_events << " observed.\n";
            error_++;
        }
    }

    void UnfoldingChecker::explore(Configuration C, std::list<EventSet> maxEvtHistory, EventSet D, EventSet A,
                                   UnfoldingEvent *currentEvt, EventSet prev_exC, std::deque<Actor> actors)
    {
        UnfoldingEvent *e = nullptr;
        EventSet enC, exC = prev_exC; // exC.erase(currentEvt);

        EvtSetTools::remove(exC, currentEvt);

        // exC = previous exC - currentEvt + new events

        extend(actors, C, maxEvtHistory, exC, enC);

        for (auto it : C.events_)
            EvtSetTools::remove(exC, it);

        // return when enC \subset of D

        bool chk = true;
        if (enC.size() > 0)
            for (auto evt : enC)
                if (!EvtSetTools::contains(D, evt))
                {
                    chk = false;
                    break;
                }

        if (chk)
        {

            if (C.events_.size() > 0)
            {

                g_var::nb_traces++;
                if (this->confs_check_)
                {
                    if (g_var::nb_traces > confs_expected_.size())
                    {
                        std::cerr << "ERROR: more trace than expected!!\n";
                        error_++;
                    }
                    else if (confs_expected_[g_var::nb_traces - 1] != C.events_.size())
                    {
                        std::cerr << "ERROR: trace " << g_var::nb_traces << " contains " << C.events_.size() << " events where "
                                  << confs_expected_[g_var::nb_traces - 1] << " were expected!!\n";
                        error_++;
                    }
                }

                std::cout << "\n Exploring executions: " << g_var::nb_traces << " : \n";
                std::cout << "\n-----------------------------------------------------------"
                             "-------------------------------------------------------------------"
                             "-------------------\n";
                for (auto evt : C.events_)
                {
                    std::cout << " --> ";
                    evt->print();
                }

                std::cout << "\n-----------------------------------------------------------"
                             "-------------------------------------------------------------------"
                             "-------------------\n";
                std::cout << "\n";
            }
            return;
        }

        if (A.empty())
        {
            e = *(enC.begin());
        }
        else
        {

            // if A is not empty, chose one event in the intersection of A and enC
            for (auto evt : A)
                if (EvtSetTools::contains(enC, evt))
                {
                    e = evt;
                    break;
                }
        }

        if (e == nullptr)
        {
            std::cerr << "\n\nSOMETHING WENT WRONG. Event is null."
                      << "\n";
            exit(EXIT_FAILURE);
        }

        std::cout << "exploring --------------------> :";

        e->print();

        std::cout << "\n";
        
        // TODO: eliminate nextState
        State nextState = currentEvt->appState.execute(e->transition);
        
        auto curEv_StateId = currentEvt->get_state_id();
        // auto nextState_id = app_side_->execute_transition(curEv_StateId, e->transition);
        auto nextState_id = app_side_->execute_transition(curEv_StateId, e->get_transition_tag());
        e->set_state_id(nextState_id);

        e->appState = nextState;

        Configuration C1 = C;
        EvtSetTools::pushBack(C1.events_, e);

        C1.updateMaxEvent(e);

        // update history of the maximal events by adding maximal events of the current Configuration (adding intSet)
        std::list<EventSet> maxEvtHistory1 = maxEvtHistory;

        maxEvtHistory1.push_back(C1.maxEvent);

        auto a_minus = EvtSetTools::minus(A, e);
        auto exc_minus = EvtSetTools::minus(exC, e);
        explore(C1, maxEvtHistory1, D, a_minus, e, exc_minus, actors);

        EventSet J, U1;
        EventSet Uc = g_var::U;

        EventSet D1 = D;
        EvtSetTools::pushBack(D1, e);
        EventSet D2 = D;
        EvtSetTools::pushBack(D2, e);

        EventSet J1;

        J = KpartialAlt(D1, C);

        if (!J.empty())
        {
            EvtSetTools::subtract(J, C.events_);

            explore(C, maxEvtHistory, D1, J, currentEvt, prev_exC, actors);
        }

        remove(e, C, D);
    }

    void UnfoldingChecker::remove(UnfoldingEvent *e, Configuration C, EventSet D)
    {

        EventSet unionSet, res, res1;
        unionSet = EvtSetTools::makeUnion(C.events_, D);

        // building Qcdu
        for (auto e1 : g_var::U)
        {
            for (auto e2 : unionSet)
                // add e1 which is immediate conflict with one event in C u D to res

                if (e1->isImmediateConflict1(e1, e2))
                {
                    EvtSetTools::pushBack(res, e1);
                    break;
                }
        }

        res1 = res;
        for (auto e1 : res1)
        {
            EventSet h = e1->getHistory();
            res = EvtSetTools::makeUnion(res, h);
        }

        res = EvtSetTools::makeUnion(res, unionSet);
        // move e from U to G if the condition is satisfied

        if (!EvtSetTools::contains(res, e))
        {
            EvtSetTools::remove(g_var::U, e);

            // G.insert(e);
        }

        // move history of ê from U to G
        EventSet U1 = g_var::U;
        for (auto evt : U1)

        {
            if (evt->isImmediateConflict1(evt, e))
            {

                EventSet h = evt->getHistory();
                EvtSetTools::pushBack(h, evt);

                for (auto e2 : h)
                    if (!EvtSetTools::contains(res, e2))
                    {
                        EvtSetTools::remove(g_var::U, e2);
                        //	G.insert(e2);
                    }
            }
        }
    }

} // namespace uc
