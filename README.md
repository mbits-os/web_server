# Web Server

An asio-based, Acorn-inspired library for simple web service with routes.

## Dependencies

1. A modern C++ compiler with proper `<regex>` library inside standard library. This disqualifies GCC, anything below 4.9 and some anscient, but still used VC++, like 2013.
2. Boost for the ASIO. In the future, probably dropped for the Networking TS.

## Model

Just like with Acorn, or anything out there written in Node.js, you setup your `routes` based on [express paths](https://github.com/pillarjs/path-to-regexp/blob/master/Readme.md) and middlewares, create a `web::server`, setup port (and/or interface, _TBD_), hand over the router and `server.run()`.

### Simple static server

    #include <web/server.h>
    #include <files/files.h>

	int main(int argc, char* argv[])
	{
        unsigned short port = 80;

        auto root = web::router::make();
        root->filter<web::middleware::files>(argv[0]);
        web::server server;
        server.set_routes(*root);
        if (!server.listen(port)) {
            fprintf(stderr, "server: cannot listen on port %u", port);
            return 2;
        }

        server.run();
	}

### Adding a route

    root->add("/api/:id(\d+)/link.json", [links](const web::request& req, web::response& resp) {

        auto id_param = *req.param("id");
        auto it = links->find(id_param);
        if (it == links->end()) {
            resp.set(web::header::Content_Type, "text/json");
            resp.status(web::status::bad_request);
            resp.print(
                R"({"error": "Could not find link )" + id_param + R"("})"
            );
            return;
        }

        it->details(resp);
    });
## Credits

The code contains `delegate`s from [Code Review Stack Exchange](http://codereview.stackexchange.com/questions/14730/impossibly-fast-delegate-in-c11), discovered by the InsideOS people. The code is attributed to [user1095108](http://codereview.stackexchange.com/users/15768/user1095108).

The `path_compiler.h`contains C++ implementation of [pathToRegexp](https://github.com/pillarjs/path-to-regexp/blob/master/index.js) of [pillarjs](https://pillarjs.github.io/) (current maintainer: [Blake Embrey](https://github.com/blakeembrey)).
