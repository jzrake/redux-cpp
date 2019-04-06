#pragma once
#include <queue>
#include <rxcpp/rx.hpp>




//=============================================================================
namespace redux {




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

    template<typename Fn>
    Fn negate(Fn pred) { return [pred] (auto arg) { return ! pred(arg); }; }
}




//=============================================================================
template<typename StateType, typename ActionType>
class redux_t
{
public:


    //=========================================================================
    class proxy_t;
    class store_t;
    class dispatcher_t;

    using state_t         = StateType;
    using action_t        = ActionType;
    using action_bus_t    = rxcpp::subjects::subject<action_t>;
    using action_stream_t = rxcpp::observable<action_t>;
    using state_stream_t  = rxcpp::observable<state_t>;
    using reducer_t       = std::function<state_t(state_t, action_t)>;
    using runoff_pred_t   = std::function<bool(action_t)>;
    using next_t          = std::function<void(action_t)>;
    using subscriber_t    = std::function<void(state_t)>;
    using middleware_t    = std::function<void(proxy_t, next_t, action_t)>;
    using bottomware_t    = std::function<action_stream_t(action_stream_t)>;


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

        std::shared_ptr<next_t>     shared_next;
        std::shared_ptr<state_t>    shared_state;
        std::shared_ptr<std::mutex> shared_state_mutex;
    };


    //=========================================================================
    class dispatcher_t
    {
    public:
        dispatcher_t(next_t next, state_t state=state_t())
        : shared_next       (std::make_shared<next_t>(next))
        , shared_state      (std::make_shared<state_t>(state))
        , shared_state_mutex(std::make_shared<std::mutex>())
        , dispatch_mutex    (std::make_shared<std::mutex>())
        , dispatch_thread_id(std::this_thread::get_id())
        , proxy(shared_next, shared_state, shared_state_mutex)
        {
        }

        void dispatch(action_t action)
        {
            std::lock_guard<std::mutex> lock(*dispatch_mutex);

            if (std::this_thread::get_id() != dispatch_thread_id)
            {
                dispatch_queue.push(action);
            }
            else
            {
                shared_next->operator()(action);

                while (! dispatch_queue.empty())
                {
                    shared_next->operator()(dispatch_queue.front());
                    dispatch_queue.pop();
                }
            }
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
                shared_next=shared_next,
                next=*shared_next] (auto action)
            {
                middleware(proxy, next, action);
            };
        }

    private:
        std::shared_ptr<next_t>     shared_next;
        std::shared_ptr<state_t>    shared_state;
        std::shared_ptr<std::mutex> shared_state_mutex;
        std::shared_ptr<std::mutex> dispatch_mutex;
        std::thread::id             dispatch_thread_id;
        std::queue<action_t>        dispatch_queue;
        proxy_t                     proxy;
    };


    //=========================================================================
    class store_t
    {
    public:
        store_t(
            reducer_t reducer,
            bottomware_t bottomware,
            runoff_pred_t runoff_pred,
            state_t state)

        : dispatcher(innermost_next(), state)
        , action_stream(bottomware(action_bus.get_observable()))
        , action_runoff(action_stream.filter(runoff_pred))
        , state_stream(action_stream.filter(detail::negate(runoff_pred)).scan(state, reducer))

        {
            action_runoff.subscribe([this] (auto action)
            {
                dispatcher.dispatch(action);
            });
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

        auto get_state_stream() const
        {
            return state_stream;
        }

    private:
        next_t innermost_next()
        {
            return [s=action_bus.get_subscriber()] (action_t action) { s.on_next(action); };
        }
        action_bus_t    action_bus;
        dispatcher_t    dispatcher;
        action_stream_t action_stream;
        action_stream_t action_runoff;
        state_stream_t  state_stream;
    };
};




//=============================================================================
template<
    typename ReducerType,
    typename StateType  = typename detail::function_traits<ReducerType>::template arg<0>::type,
    typename ActionType = typename detail::function_traits<ReducerType>::template arg<1>::type,
    typename RunoffPredType = typename redux_t<StateType, ActionType>::runoff_pred_t,
    typename BottomwareType = typename redux_t<StateType, ActionType>::bottomware_t>
auto create_store(
    ReducerType reducer,
    BottomwareType bottomware=[] (auto o) { return o; },
    RunoffPredType runoff_pred=[] (auto) { return false; },
    StateType state=StateType())
{
    return typename redux_t<StateType, ActionType>::store_t(reducer, bottomware, runoff_pred, state);
}

} // namespace redux
