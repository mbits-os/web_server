# Web Server

An asio-based, Acorn-inspired library for simple web service with routes.

## Dependencies

1. A modern C++ compiler with proper `<regex>` library inside standard library. This disqualifies GCC, anything below 4.9 and some anscient, but still used VC++, like 2013.
2. Boost for the ASIO. In the future, probably dropped for the Networking TS.

## Model

Just like with Acorn, or anything out there written in Node.js, you setup your `routes` based on [express paths](https://github.com/pillarjs/path-to-regexp/blob/master/Readme.md), create a `web::server`, setup port (and/or interface, _TBD_), hand over the router and `server.run()`.

_TODO_: Support `web::middleware` and attributes on `web::request`.

## Credits

The code contains `delegate`s from [Code Review Stack Exchange](http://codereview.stackexchange.com/questions/14730/impossibly-fast-delegate-in-c11), discovered by the InsideOS people. The code is attributed to [user1095108](http://codereview.stackexchange.com/users/15768/user1095108).

The `path_compiler.h`contains C++ implementation of [pathToRegexp](https://github.com/pillarjs/path-to-regexp/blob/master/index.js) of [pillarjs](https://pillarjs.github.io/) (current maintainer: [Blake Embrey](https://github.com/blakeembrey)).
