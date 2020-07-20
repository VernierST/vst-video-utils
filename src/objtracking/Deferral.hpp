//
//  Deferral.hpp
//  Video Physics
//
//  Created by Joseph Kelly on 6/28/20.
//  Copyright © 2020 Vernier Software & Technology. All rights reserved.
//

#ifndef Deferral_hpp
#define Deferral_hpp

#include <functional>

/// Class that defers an action until the instance goes out of scope.
/// Why bother? Because I like making silly little code widgets.
class Deferral {
public:
    Deferral(std::function<void()> action) :_action(action) {}
    ~Deferral() { _action(); }
private:
    std::function<void()> _action;
};

#define DEFER(x) auto __finalize = Deferral(x)

#endif /* Deferral_hpp */
