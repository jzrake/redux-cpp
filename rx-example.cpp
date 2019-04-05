#include <iostream>
#include <rxcpp/rx.hpp>




//==============================================================================
using namespace rxcpp;
using State = int;
using Action = std::string;




//=============================================================================
class redux_t
{
public:


    //=========================================================================
    class proxy_t;
    class store_t;
    class dispatcher_t;

    using state_t        = State;
    using action_t       = Action;
    using reducer_t      = std::function<state_t(state_t, action_t)>;
    using next_t         = std::function<void(action_t)>;
    using middleware_t   = std::function<void(proxy_t, action_t, next_t)>;
    using subscriber_t   = std::function<void(state_t)>;
    using action_bus_t   = rxcpp::subjects::subject<action_t>;
    using state_stream_t = rxcpp::observable<state_t>;


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
        friend class dispatcher_t;

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
    class dispatcher_t
    {
    public:
        dispatcher_t(next_t next, state_t state=state_t())
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

        void apply_middleware(middleware_t middleware)
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
        }

    private:
        std::shared_ptr<next_t>             shared_next;
        std::shared_ptr<state_t>            shared_state;
        std::shared_ptr<std::mutex> mutable shared_state_mutex;
        proxy_t proxy;
    };


    //=========================================================================
    class store_t
    {
    public:
        store_t(reducer_t reducer, state_t state=state_t()) : dispatcher(make_next(), state)
        {
            state_stream = action_bus.get_observable().scan(state, reducer);
            state_stream.subscribe([this] (auto state)
            {
                dispatcher.set_state(state);
            });
        }

        void dispatch(action_t action)
        {
            dispatcher.dispatch(action);
        }

        const state_t& get_state() const
        {
            return dispatcher.get_state();
        }

        store_t& apply_middleware(middleware_t middleware)
        {
            dispatcher.apply_middleware(middleware);
            return *this;
        }

        auto subscribe(subscriber_t subscriber)
        {
            return state_stream.subscribe(subscriber);
        }

    private:
        next_t make_next()
        {
            return [s=action_bus.get_subscriber()] (action_t action) { s.on_next(action); };
        }
        action_bus_t action_bus;
        dispatcher_t dispatcher;
        state_stream_t state_stream;
    };
};




//=============================================================================
int main()
{


    // Reducer
    //=========================================================================
    auto reducer = [] (State state, Action action)
    {
        if (action == "Increment")
            return state + 1;
        if (action == "Decrement")
            return state - 1;
        return state;
    };


    // Middleware functions
    //=========================================================================
    auto log_state = [] (auto store, auto action, auto next)
    {
        if (action == "Log")
        {
            std::cout << "The state is " << store.get_state() << std::endl;
        }
        else
        {
            next(action);
        }
    };
    auto cancel_if_empty = [] (auto store, auto action, auto next)
    {
        if (! action.empty())
        {
            next(action);
        }
        else
        {
            std::cout << "That was an empty action!" << std::endl;
        }
    };
    auto dispatch_more = [] (auto store, auto action, auto next)
    {
        if (action == "Dispatch")
        {
            store.dispatch("Increment");
            store.dispatch("Log");
            store.dispatch("Increment");
            store.dispatch("Log");
        }
        else
        {
            next(action);
        }
    };


    // Store creation
    //=========================================================================
    auto store = redux_t::store_t(reducer)
    .apply_middleware(dispatch_more)
    .apply_middleware(log_state)
    .apply_middleware(cancel_if_empty);


    // Subscribe, and run it!
    //=========================================================================
    store.subscribe([] (State state) { std::cout << state << std::endl; });
    store.dispatch("Increment");
    store.dispatch("Log");
    store.dispatch("Increment");
    store.dispatch("Log");
    store.dispatch("Decrement");
    store.dispatch("Log");
    store.dispatch("Decrement");
    store.dispatch("Log");
    store.dispatch("Dispatch");
    store.dispatch(std::string());


    return 0;
}
