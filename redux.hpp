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
class store_t
{
public:

    using state_t      = StateType;
    using action_t     = ActionType;
    using reducer_t    = ReducerType;
    using next_t       = std::function<void(action_t)>;
    using middleware_t = std::function<void(store_t& store, next_t next, action_t action)>;
    using subscriber_t = std::function<void(state_t)>;

    store_t(const store_t& other) = delete;

    store_t(ReducerType reducer, state_t state)
    : state(state)
    , next(next_func(reducer))
    {
    }

    void dispatch(action_t action)
    {
        next(action);
    }

    const state_t& get_state() const
    {
        return state;
    }

    void subscribe(subscriber_t subscriber)
    {
        subscribers.push_back(subscriber);
    }

    store_t& apply_middleware(middleware_t middleware)
    {
        next = [this, old_next=next, middleware] (auto action)
        {
            middleware(*this, old_next, action);
        };
        return *this;
    }

private:
    //=========================================================================
    next_t next_func(reducer_t reducer)
    {
        return [this, reducer] (auto action)
        {
            state = reducer(state, action);

            for (auto subscriber : subscribers)
            {
                subscriber(state);
            }
        };
    }

    next_t next;
    state_t state;
    std::vector<subscriber_t> subscribers;
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
    return store_t<StateType, ActionType, ReducerType>(reducer, state);
}




//=============================================================================
template<
    typename ReducerType,
    typename StateType = typename detail::function_traits<ReducerType>::template arg<0>::type,
    typename ActionType = typename detail::function_traits<ReducerType>::template arg<1>::type>
auto create_store_ptr(ReducerType reducer, StateType state=StateType())
{
    return std::make_unique<store_t<StateType, ActionType, ReducerType>>(reducer, state);
}

} // namespace redux
