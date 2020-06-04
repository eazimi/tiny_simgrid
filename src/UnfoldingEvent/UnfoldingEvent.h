#ifndef UNFOLDINGEVENT_H
#define UNFOLDINGEVENT_H

#include "../transition/transition.h"
#include "../state/state.h"
//#include "../EventSet/EventSet.h"
//#include "../configuration/configuration.h"

class EventSet;
class Configuration;

//namespace event {

class UnfoldingEvent
{
public:
    UnfoldingEvent(State* s) : appState(*s) {}
    UnfoldingEvent(unsigned int nb_events, Transition t, EventSet* causes) : id(nb_events), transition(t), causes(causes) {}
//    UnfoldingEvent(unsigned int nb_events, Transition t) : id(nb_events), transition(t) {}

    int id {-1};
    State appState;
    Transition transition; // The last transition made to reach that state
    EventSet* causes;       // used to store directed ancestors of event e
    EventSet* conflictEvts; // used for Test and Send/Receive events, storing conflicts events with me (concern the same
                           // communication)

    EventSet getHistory() const;

    bool isConflict(UnfoldingEvent* event, UnfoldingEvent* otherEvent);
    bool concernSameComm(UnfoldingEvent* event, UnfoldingEvent* otherEvent);

    // check otherEvent is in my history ?
    bool inHistory(UnfoldingEvent* otherEvent);

    bool isImmediateConflict(UnfoldingEvent* evt, UnfoldingEvent* otherEvt);
    bool isImmediateConflict1(UnfoldingEvent* evt, UnfoldingEvent* otherEvt);

    bool conflictWithConfig(UnfoldingEvent* event, Configuration* config);
    bool operator<(const UnfoldingEvent& other) const;
    bool operator==(const UnfoldingEvent& other) const;
    void print();
};

//}

#endif // UNFOLDINGEVENT_H