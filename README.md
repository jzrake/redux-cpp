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

Middleware is how the redux pattern constrains the app's influence on the outside world. Many actions dispatched to the store are pure - they simply invoke `state = reducer(state, action)`. Others need to do things that transform the action before it reaches the reducer, or dispatch further actions to the store (so-called "side-effects"). Middleware is thus a subroutine, which reads from, and operates on the action, the store, and outside services.

The agreed-upon pattern is that middlewares are composed with one another - with each (optionally) invoking the next. The final (innermost) middleware invokes the reducer, replaces the state, and notifies subscribers.

In this implementation, I gave the store a method called `apply_middleware` that mutates the store itself by replacing its `next` function object with a new one, which can (optionally) call it. To log every action in the above example, you would do this:

```C++
store.apply_middleware([] (auto& store, auto next, auto& action)
{
    std::cout << "Dispatching " << action << std::endl;
    next(action);
    std::cout << "Finished! New state = " << store.get_state() << std::endl;
};
```

`Store::apply_middleware` returns a non-const reference to the store, so you can chain the middlewares together. They will be called in the reverse order to how they were applied.
