#pragma once
#include <vector>
#include <functional>




//=============================================================================
namespace redux
{

//=============================================================================
template<
    typename StateType,
    typename ActionType,
    typename ReducerType>
class Store
{
public:

    using Subscriber = std::function<void(const Store&)>;
    using NextFunc   = std::function<void(ActionType&)>;
    using Middleware = std::function<void(Store& store, NextFunc next, ActionType& action)>;

    Store(const Store& other) = delete;

    Store(ReducerType reducer, StateType state)
    : state(state)
    , next(next_func(reducer))
    {
    }

    void dispatch(ActionType action)
    {
        next(action);
    }

    const StateType& get_state() const
    {
        return state;
    }

    void subscribe(Subscriber subscriber)
    {
        subscribers.push_back(subscriber);
    }

    Store& apply_middleware(Middleware middleware)
    {
        next = [this, old_next=next, middleware] (auto& action)
        {
            middleware(*this, old_next, action);
        };
        return *this;
    }

private:
    //=========================================================================
    NextFunc next_func(ReducerType reducer)
    {
        return [this, reducer] (ActionType& action)
        {
            state = reducer(state, action);

            for (auto subscriber : subscribers)
            {
                subscriber(*this);
            }
        };
    }

    NextFunc next;
    StateType state;
    std::vector<Subscriber> subscribers;
};




//=============================================================================
namespace detail
{
    template <typename T>
    struct function_traits : public function_traits<decltype(&T::operator())> {};

    template <typename ClassType, typename ReturnType, typename... Args>
    struct function_traits<ReturnType(ClassType::*)(Args...) const>
    {
        typedef ReturnType result_type;
        enum { arity = sizeof...(Args) };
        template <size_t i> struct arg
        {
            typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
        };
    };
}




//=============================================================================
template<
    typename ReducerType,
    typename StateType = typename detail::function_traits<ReducerType>::template arg<0>::type,
    typename ActionType = typename detail::function_traits<ReducerType>::template arg<1>::type>
auto create_store(ReducerType reducer, StateType state=StateType())
{
    return Store<StateType, ActionType, ReducerType>(reducer, state);
}




//=============================================================================
template<
    typename ReducerType,
    typename StateType = typename detail::function_traits<ReducerType>::template arg<0>::type,
    typename ActionType = typename detail::function_traits<ReducerType>::template arg<1>::type>
auto create_store_ptr(ReducerType reducer, StateType state=StateType())
{
    return std::make_unique<Store<StateType, ActionType, ReducerType>>(reducer, state);
}

} // namespace redux
