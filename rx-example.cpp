#include <iostream>
#include "../RxCpp/Rx/v2/src/rxcpp/rx.hpp"



using namespace rxcpp;
using State = int;
using Action = std::string;




//=============================================================================
class Store
{
public:


    //=========================================================================
    class proxy_t;
    using state_t      = State;
    using action_t     = Action;
    using next_t       = std::function<void(action_t)>;
    using middleware_t = std::function<void(proxy_t, action_t, next_t)>;


    //=========================================================================
    class proxy_t
    {
    public:

        const state_t& get_state() const
        {
            std::lock_guard<std::mutex> lock(*shared_state_mutex);
            return *shared_state;
        }

        void dispatch(action_t action)
        {
            shared_next->operator()(action);
        }

    private:
        friend class Store;

        proxy_t(
            std::shared_ptr<next_t> shared_next,
            std::shared_ptr<state_t> shared_state,
            std::shared_ptr<std::mutex> shared_state_mutex)
        : shared_next(shared_next)
        , shared_state(shared_state)
        , shared_state_mutex(shared_state_mutex)
        {
        }

        std::shared_ptr<next_t>             shared_next;
        std::shared_ptr<state_t>            shared_state;
        std::shared_ptr<std::mutex> mutable shared_state_mutex;
    };


    //=========================================================================
    Store(next_t next, state_t state=state_t())
    : shared_next       (std::make_shared<next_t>(next))
    , shared_state      (std::make_shared<state_t>(state))
    , shared_state_mutex(std::make_shared<std::mutex>())
    , proxy(shared_next, shared_state, shared_state_mutex)
    {
    }

    void dispatch(action_t action)
    {
        shared_next->operator()(action);
    }

    void set_state(const state_t& next_state)
    {
        std::lock_guard<std::mutex> lock(*shared_state_mutex);
        *shared_state = next_state;
    }

    const state_t& get_state() const
    {
        std::lock_guard<std::mutex> lock(*shared_state_mutex);
        return *shared_state;
    }

    Store& apply_middleware(middleware_t middleware)
    {
        *shared_next = [
            middleware,
            proxy=proxy,
            shared_state=shared_state,
            shared_next=shared_next,
            next=*shared_next] (action_t action)
        {
            middleware(proxy, action, next);
        };
        return *this;
    }

private:
    std::shared_ptr<next_t>             shared_next;
    std::shared_ptr<state_t>            shared_state;
    std::shared_ptr<std::mutex> mutable shared_state_mutex;
    proxy_t proxy;
};




//=============================================================================
int main()
{
    auto action_subject = subjects::subject<Action>();
    auto action_bus = action_subject.get_subscriber();
    auto action_stream = action_subject.get_observable();
    auto reducer = [] (State state, Action action)
    {
        if (action == "Increment")
            return state + 1;
        if (action == "Decrement")
            return state - 1;
        return state;
    };
    auto state_stream = action_stream.scan(State(), reducer);

    auto next = [action_bus] (Action action)
    {
        action_bus.on_next(action);
    };

    auto store = Store(next)
    .apply_middleware([] (auto store, auto action, auto next)
    {
        if (action == "Log")
        {
            std::cout << "The state is " << store.get_state() << std::endl;
        }
        else
        {
            next(action);
        }
    });

    state_stream.subscribe([] (State state) { std::cout << state << std::endl; });
    state_stream.subscribe([&store] (State state) { store.set_state(state); });

    store.dispatch("Increment");
    store.dispatch("Log");
    store.dispatch("Increment");
    store.dispatch("Log");
    store.dispatch("Decrement");
    store.dispatch("Log");
    store.dispatch("Decrement");
    store.dispatch("Log");

    return 0;
}
