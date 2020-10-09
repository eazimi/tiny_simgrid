#ifndef UNFOLDINGCHECKER_H
#define UNFOLDINGCHECKER_H

#include "list"
#include "Global.hpp"

namespace uc
{

    class UnfoldingChecker
    {
        unsigned long expandedStatesCount_ = 0;
        // int Mode = 1; // Mode = 1 is a mutexed model
        std::vector<unsigned int> confs_expected_;
        bool confs_check_ = false;
        int error_ = 0;
        unsigned int expected_events_ = 0;

    public:
        UnfoldingChecker() = default;
        UnfoldingChecker(std::vector<unsigned int> confs, unsigned int expected_events) : confs_expected_(confs), confs_check_(true), expected_events_(expected_events) {}
        UnfoldingChecker(const UnfoldingChecker &) = default;
        UnfoldingChecker &operator=(UnfoldingChecker const &) = default;
        UnfoldingChecker(UnfoldingChecker &&) = default;
        ~UnfoldingChecker() = default;

        // Recursive function
        void explore(Configuration C, std::list<EventSet> maxEvtHistory, EventSet D, EventSet A, UnfoldingEvent *currentEvt,
                     EventSet prev_enC);

        void explore(std::shared_ptr<AppSide> app_side);
        void extend(Configuration C, std::list<EventSet> maxEvtHistory, EventSet &ExC, EventSet &enC) const;
        void remove(UnfoldingEvent *e, Configuration C, EventSet D);
        EventSet KpartialAlt(EventSet D, Configuration C) const;

        int error_count() { return error_; }
    };

} // namespace uc

#endif // UNFOLDINGCHECKER_H
