#include <iostream>
#ifdef HAVE_RXCPP
#include "rx-redux.hpp"
#else
#include "redux.hpp"
#endif




//=============================================================================
int main()
{
    using State = int;
    using Action = std::string;


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
    auto log_state = [] (auto&& store, auto next, auto action)
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
    auto cancel_if_empty = [] (auto&& store, auto next, auto action)
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
    auto dispatch_more = [] (auto&& store, auto next, auto action)
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
    auto store = redux::create_store(reducer);

    store
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
