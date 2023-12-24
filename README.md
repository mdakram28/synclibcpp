# DiffLib CPP

A C++ library to diff and patch json objects.

## Build (UNIX)
```
$ mkdir build && cd build
$ cmake ../
$ cmake --build .
```


## Includes
```CPP
// For diffing states
#include "diff.hpp"
// For state variable and state transport
#include "syncserver.cpp"
```

## Usage
```CPP

// Create and start server used for syncing states
auto const address = net::ip::make_address("127.0.0.1");
auto const port = static_cast<unsigned short>(8000);
std::shared_ptr<WsStateServer> server = std::make_shared<WsStateServer>(1, tcp::endpoint{address, port});
server->start_server();

// Create StateVar
std::shared_ptr<StateVar> state_var = std::make_shared<StateVar>();
std::thread sync_thread([&] {
    while (true) {
        std::cout << "Syncing ...." << std::endl;
        state_var->sync();
        sleep(5);
    }
});

// Attach state update listeners
state_var->on_update([](std::shared_ptr<StateValue> state) {
    std::cout << "State Updated :: " << state->value << std::endl;
});

// Connect
state_var->add_transport(server);
```