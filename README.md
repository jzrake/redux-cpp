# Redux in C++
_Tiny implementation of the redux pattern for C++14_

Redux is a pattern for state management. Its [canonical implementation is Javascript](https://redux.js.org), and the documentation uses Javascript for all of its examples. I wrote this little piece of C++ code to help C++ programmers (like myself) understand what the pattern does.


## Basic example
```C++
/* Define types for your state and action. */
using State = int;
using Action = std::string;

/* Write a reducer: (state, action) -> state. */
auto reducer = [] (State state, Action action)
{
    if (action == "Increment")
        return state + 1;
    if (action == "Decrement")
        return state - 1;
    return state;
};

/* Create a store from the reducer. */
auto store = redux::create_store(reducer);

/* Subscribe to the store to be notified when its state changes. */
store.subscribe([] (auto state)
{
    std::cout << "state: " << state << std::endl;
});

/* Dispatch actions to the store. */
store.dispatch("Increment");
```


## Middleware
Middleware is how the redux pattern constrains an application's influence on the "outside world." Some actions dispatched to the store are pure, in the sense that they only trigger the creation of a new state via `state = reducer(state, action)`. However certain actions need to be transformed before reaching the reducer, or should trigger a cascade of new actions. They might also need to launch asynchronous tasks, interact with the filesystem, or send commands to the view hierarchy. Middleware is how redux controls where and how these "side-effects" are allowed to take place.

If you visit the [official documentation](https://redux.js.org/advanced/middleware#middleware), you'll see an article that walks you through the steps that led to the current formulation of middleware. But I think for C++ programmers it's easier to say outright what the pattern actually is. Middleware is a chain of operators that wrap the store's `dispatch` function. Each link in the chain can operate on the store, on the action, or on external services, and can then optionally invoke the next link in the chain. The final link is the store's original middleware, which is defined as follows:

```C++
return [this, reducer] (auto action)
{
    state = reducer(state, action);

    for (auto subscriber : subscribers)
    {
        subscriber(state);
    }
};
```

The store has a `next` member variable which is initialized with this definition, and `store_t::dispatch` just invokes `next`. Each time `apply_middleware` is invoked, `next` is replaced with a new `next` that wraps the old one:

```C++
next = [this, old_next=next, middleware] (auto action)
{
    middleware(*this, old_next, action);
};
return *this;
```

That's all middleware is (though reading the official docs, you might think it was more complicated). As an example, suppose you want to log each action, and the new state after that action was applied. You'd do this:

```C++
store.apply_middleware([] (auto&& store, auto next, auto action)
{
    std::cout << "Dispatching " << action << std::endl;
    next(action);
    std::cout << "Finished! New state = " << store.get_state() << std::endl;
};
```

`store_t::apply_middleware` returns a non-const reference to the store, so you can chain the middlewares together. They will be called in the reverse order to how they were applied. Note also that if a middleware dispatches new actions to the store, those actions are filtered through the middleware chain from the beginning.


## Limitations
The store implementation in `redux.hpp` is limited in several ways:

- __no move or copy__: The store class is not move or copy constructible. This is because the `this` pointer is captured in the lambda closures used. A work-around is to use the `redux::create_store_prt` function to create a move-constructible version.

- __no unsubscribe__: Subscriptions to the store are permanent.

- __no thread safety__: Your middleware cannot call `next` or `store.dispatch` from a background thread. This makes it a pain to do asynchronous work in the middleware. You'd need to launch work (`(store, action) -> action`) on an external service (e.g. a thread pool), then poll that service on the main thread for the resulting actions, and then dispatch them to the store. You can implement this by putting a promise-resolving middleware downstream of the one that launched the async work. But, what if you were perfectly happy (or even wanted) to dispatch actions, and evaluate the reducer a background thread? Come to think of it, you might even stay on the background thread to compute `state -> view`...

Consider the requirements to overcome these limitations. You'll first need to implement the observer pattern (meaning that `store.subscribe` needs to have the signature `observer -> subscription`, where `subscription` provides an `unsubscribe` method). And, you'd be coordinating calls to `next` and `dispatch` from multiple threads.

These problems are non-trivial, but they are also already-solved, by the [Reactive Extensions](http://reactivex.io)! And, since we have [`RxCpp`](https://github.com/ReactiveX/RxCpp), I thought it would be nice to provide a more production-ready redux implementation by taking a dependency on `Rx`.


## Rx-Redux in C++
`rx-redux.hpp` is a drop-in replacement for the single-thread version in `redux.hpp`. It leverages `RxCpp`'s implementation of the observer pattern to manage subscription lifetimes. It defines a proper `observable<state_t>`, as a `scan` operation applied to the action stream via the reducer (so, feel free to map it to an `observable<view_t>`). The store subscribes itself to the stream of states, and makes the most recent one accessible to middlewares in a thread-safe manner.

The `rx-redux` store is copy and move constructible, having the same semantics as the underlying `RxCpp` structs. It's also safer than the first implementation. Middleware functions are invoked with a limited "proxy" store which only has `dispatch` and `get_state` methods (your middleware should not be able to use the other two store methods - applying more middleware or creating new subscriptions). Middleware that does need to access the state calls `store.get_state` on the proxy, and pays for this access by locking a mutex.


## One step further: asynchronous middleware
What do you do when your middlware needs to dispatch an action asynchronously to the store? You could start up a thread to do the work, and then invoke the `next` function from that thread as needed. However, this misses much of the elegance afforded by `Rx`. It would be much nicer to leverage the existing features of `Rx`, and dispatch observable streams of actions to the store.

To do this, first give your action struct an `observable<action_t>` data member (and a predicate `is_observable` for you to check later). Then, assign to that data member an observable, scheduled to run on a background thread. Now, dispatch that action to the store.

Your action (with its observable data member) will be propagated through the action stream and middlewares just like the others. To get its emissions, you'll need someone to subscribe to the inner observable. This is done with an additional concept I've added to the store: a _bottomware_.

The bottomware is a mapping of `action_stream_t -> action_stream_t`, which transforms the stream of post-middleware actions. That is, it goes in between the middleware and the reducer. To get the actions from the inner observable you dispatched in your middleware, you would define your bottomware to convert the action stream first to a higher-order stream (a stream of action streams), and then back to a stream of actions:

```C++
auto bottomware = [] (auto action_stream)
{
    return action_stream.flat_map([] (auto a)
    {
        return a.is_observable() ? a.get_observable() : observable<>::just(a);
    });
};
```
Now your asynchronous actions are subscribed to and will begin reaching the store.

There's a caveat to be aware of with this pattern: the actions emitted by your inner observable entered the stream _after_ the middleware. Basically, they snuck through (prenatally - before they were even actions) inside their parent action. In some cases, you might not care about this, because maybe none of your middlewares are interested in the types of actions resulting from asynchronous work.


## Diverting stowaways
However, for cases in which a middleware might be interested, I have added one additional concept, called `runoff`. It's a predicate `(action_t -> boolean)` which you can optionally supply to `create_store`, and which will divert actions you deem are not ready to reach the reducer. The store subscribes itself to this stream of runoff actions, and dispatches them on the 'main' thread (the one your store was created on). The default runoff predictate returns false for all actions. However, if you supply your own, then it should return true for any actions you know have snuck through your middleware by way of an inner observable, and should thus be re-dispatched.
