#include <iostream>
#include "redux.hpp"




//=============================================================================
struct State
{
    int count = 0;
};




//=============================================================================
struct Action
{
    enum class Type
    {
        Increment,
        Decrement,
        LogMessage,
    };

    Action() {}
    Action(Type type) : type(type) {}

    Type type;
};




//=============================================================================
int main()
{
    auto reducer = std::function<State(State, Action)>([] (State state, Action action)
    {
        switch (action.type)
        {
            case Action::Type::Increment: return State {state.count + 1};
            case Action::Type::Decrement: return State {state.count - 1};
            case Action::Type::LogMessage: return state;
        }
    });
    auto store = redux::create_store(reducer);



    auto middleware1 = [] (auto& store, auto next, auto& action)
    {
        std::cout << "Log a first message!" << std::endl;
        next(action);
    };

    auto middleware2 = [] (auto& store, auto next, auto& action)
    {
        std::cout << "Log another message!" << std::endl;
        next(action);
    };


    store
    .apply_middleware(middleware1)
    .apply_middleware(middleware2);


    store.subscribe([] (auto& store)
    {
        std::cout << "should be 1: " << store.get_state().count << std::endl;
    });
    store.dispatch(Action::Type::Increment);

    return 0;
}
