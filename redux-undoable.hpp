#pragma once
#include <immer/flex_vector.hpp>




//=============================================================================
namespace redux
{


//=============================================================================
template<class T>
struct undoable
{
    using value_type = T;

    undoable(T present=T()) : present(present)
    {
    }

    const T& get() const
    {
        return present;
    }

    template<typename Function, typename... Args>
    undoable advance(Function&& fn, Args&&... args) const
    {
        return {
            fn(present, std::forward<Args...>(args)...),
            past.push_back(present),
            {},
        };
    }

    template<typename Function, typename... Args>
    undoable replace(Function&& fn, Args&&... args) const
    {
        return {
            fn(present, std::forward<Args...>(args)...),
            past,
            {},
        };
    }

    undoable undo() const
    {
        if (past.empty())
        {
            throw std::logic_error("cannot undo");
        }
        return {
            past.back(),
            past.erase(past.size() - 1),
            future.push_front(present),
        };
    }

    undoable redo() const
    {
        if (future.empty())
        {
            throw std::logic_error("cannot redo");
        }
        return {
            future.front(),
            past.push_back(present),
            future.erase(0),
        };
    }

    bool can_undo() const
    {
        return ! past.empty();
    }

    bool can_redo() const
    {
        return ! future.empty();
    }

private:
    //=========================================================================
    undoable(T present, immer::flex_vector<T> past, immer::flex_vector<T> future)
    : present(present)
    , past(past)
    , future(future)
    {
    }

    T present;
    immer::flex_vector<T> past;
    immer::flex_vector<T> future;
};

} // namespace undoable
