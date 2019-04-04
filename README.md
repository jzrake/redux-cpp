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
store.subscribe([] (auto&& store)
{
    std::cout << "state: " << store.get_state() << std::endl;
});

/* Dispatch actions to the store. */
store.dispatch("Increment");
```


## Middleware
Middleware is how the redux pattern constrains an application's influence on the "outside world." Some actions dispatched to the store are pure, in the sense that they only trigger the creation of a new state via `state = reducer(state, action)`. However certain actions need to be transformed before reaching the reducer, or should trigger a cascade of new actions. They might also need to launch asynchronous tasks, interact with the filesystem, or send commands to the view hierarchy. Middleware is how redux controls where and how these "side-effects" are allowed to take place.

If you visit the [official documentation](https://redux.js.org/advanced/middleware#middleware), you'll see an article that walks you through the steps that led to the current formulation of middleware. But I think for C++ programmers it's easier to say outright what the pattern actually is. Middleware is a chain of operators that wrap the store's `dispatch` function. Each link in the chain can operate on the store, on the action, or on external services, and can then optionally invoke the next link in the chain. The final link is the store's original middleware, which is defined as follows:

```C++
next = [this, reducer] (auto& action)
{
    state = reducer(state, action);

    for (auto subscriber : subscribers)
    {
        subscriber(*this);
    }
};
```

The store has a `next` member variable which is initialized with this definition, and `Store::dispatch` just invokes `next`. Each time `apple_middleware` is invoked, `next` is replaced with a new `next` that wraps the old one:

```C++
next = [this, old_next=next, middleware] (auto& action)
{
    middleware(*this, old_next, action);
};
return *this;
```

That's all middleware is (though reading the official docs, you might think it was more complicated). As an example, suppose you want to log each action, and the new state after that action was applied. You'd do this:

```C++
store.apply_middleware([] (auto& store, auto next, auto& action)
{
    std::cout << "Dispatching " << action << std::endl;
    next(action);
    std::cout << "Finished! New state = " << store.get_state() << std::endl;
};
```

`Store::apply_middleware` returns a non-const reference to the store, so you can chain the middlewares together. They will be called in the reverse order to how they were applied. Note also that if a middleware dispatches new actions to the store, those actions are filtered through the middleware chain from the beginning.
