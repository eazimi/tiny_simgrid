#include "StateManager.h"

namespace app
{
    int StateManager::state_id_ = 0;

    int StateManager::create_state(std::deque<Actor> &&actors, std::deque<Mailbox> &&mailboxes)
    {
        auto state = new AppState(std::forward<std::deque<Actor>>(actors), 
                    std::forward<std::deque<Mailbox>>(mailboxes));
        auto id = add_state(std::move(*state));
        return id;
    }

    int StateManager::execute_transition(int state_id, Transition const &tr)
    {
        auto state = find_state(state_id);
        if (state == nullptr)
            return -1;
        auto next_state = state->execute_transition(tr);
        auto id = add_state(std::move(next_state));
        return id;
    }

    AppState *StateManager::find_state(int state_id)
    {
        auto pos = states_.find(state_id);
        if (pos == states_.end())
            return nullptr;
        return &(pos->second);
    }

    int StateManager::add_state(AppState &&state)
    {
        states_.insert({state_id_, state});
        auto id = state_id_++;
        return id;
    }

} // namespace app