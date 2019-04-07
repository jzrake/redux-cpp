#include "redux-undoable.hpp"




//=============================================================================
int main()
{
    auto state = redux::undoable<int>();

    state = state.apply([] (auto i) { return i + 1; });
    state = state.undo();

    return 0;
}
