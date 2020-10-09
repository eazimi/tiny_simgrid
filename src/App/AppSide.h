#ifndef APPSIDE_H
#define APPSIDE_H

#include <iostream>
#include <vector>
#include <memory>
#include "StateManager.h"

namespace app
{
    typedef struct TRANSITION_PARAMS_2
    {
        int read_write;
        int access_var;
    } S_TRANSITION_PARAMS_2;

    typedef struct TRANSITION_PARAMS_3
    {
        int mailbox_id;
        int communication_id;
        const std::string &type;
    } S_TRANSITION_PARAMS_3;

    namespace
    {
        typedef struct TRANSITION_ATTRS
        {
            int actor_id {0};
            unsigned int id {0};
            int read_write {0};    // default value is read (= 0), write =1, if mutex lock = 2, unlock = 3
            int access_var {0};    // default this transition access variable 0
            int mailbox_id {-1};   // used to present mailbox id for send and receive transition
            int comm_id {-1};       // the communication that i concern (used for only send, receive, wait and test operations)
            std::string type {""}; // Isend, Ireceive, test, wait
            int mutex_iD {-1};      // the mutex i touch
            int lock_id {-1};       // used for Mtest and Mwait
        } S_TRANSITION_ATTRS;
    } // namespace

    class AppSide
    {
    public:
        explicit AppSide();
        AppSide(const AppSide &) = default;
        AppSide &operator=(AppSide const &) = default;
        AppSide(AppSide &&) = default;
        ~AppSide() = default;

        /* ACTOR */
        void create_actor(int actor_id, std::vector<S_TRANSITION_PARAMS_2> tr_params);
        void create_actor(int actor_id, std::vector<S_TRANSITION_PARAMS_3> tr_params);

        /* TRANSITION */
        std::deque<std::string> get_trans0_tags() const;
        bool partial_check_transition_equality(std::string const& tr_tag0, std::string const& tr_tag1);
        bool check_transition_dependency(std::string const &tr_tag0, std::string const &tr_tag1) const;
        bool check_transition_type(std::string const& tr_tag, std::vector<std::string> const& type_names);
        std::string get_transition_type(std::string const& tr_tag);
        int get_transition_read_write(std::string const& tr_tag);
        int get_transition_id(std::string const& tr_tag);
        int get_transition_actor_id(std::string const& tr_tag);
        int get_transition_mb_id(std::string const& tr_tag);
        int get_transition_comm_id(std::string const& tr_tag);
        std::pair<std::string, std::tuple<int, int, int>> get_transition_attrs(std::string const& tr_tag);

        /* MAILBOX */
        void create_mailbox(std::vector<int> mb_ids);

        /* STATE */
        int create_state(std::deque<Actor> &&actors, std::deque<Mailbox> &&mailboxes, bool is_state0 = false);
        int execute_transition(int state_id, std::string const &transition_tag) const;
        std::deque<std::string> get_enabled_transitions(int sid) const;
        void delete_state(int state_id);

    private:
        std::shared_ptr<std::deque<Actor>> actors_;
        std::map<std::string, S_TRANSITION_ATTRS> transitions_;
        std::shared_ptr<std::deque<Mailbox>> mailboxes_;
        std::shared_ptr<StateManager> state_manager_;

        void add_transition(std::vector<Transition> const& transitions);
        S_TRANSITION_ATTRS* find_transition_attrs(std::string const& tr_tag);
        
    };

} // namespace app

#endif // APPSIDE_H
