# openNetVM Web Console for Statistics

## About

Resources to view openNetVM Manager statistics through a web console.

The [start web console][start_web] script will run cors_server.py (which is an extension of [Python's SimpleHTTPServer][simplehttp] which enables CORS) on port 8080 by default, or on a port of your specification using the `-p` flag. See more details for this in the **Usage** section below.

## Usage

Start the openNetVM Manager following steps in either the [install
doc][install] or the [example usage guide][examples] and be sure to use
the `-s` flag supplying the argument `web`. If the `-s` flag is supplied,
the web console will automatically be launched for you using the port
number you have specified on the manager (using the manager's `-p` flag).
If no port is specified, it will run on port 8080.

The following example runs openNetVM with four cores, a portmask of 1
to use 1 NIC port, and sets the stats output to the web console.

```sh
cd onvm
./go.sh 0,1,2,3 1 -s web
```

The following example runs openNetVM with four cores, a portmask of 1
to use 1 NIC port, and sets the stats output to the web console. It also runs
the web dashboard on port 9000.

```sh
cd onvm
./go.sh 0,1,2,3 1 -s web -p 9000
```

Doing that will launch openNetVM and use the console to display log
messages. It will print out a message stating that the web stats
console has been setup and information to access it.

If something were to go wrong in the launching of the web stats, you could
manually start it by doing the following:

```sh
cd onvm_web
./start_web_console.sh
```

You could also specify a port argument using the `-p` flag:

```sh
cd onvm_web
./start_web_console.sh -p 9000
```

## Design and Implementation

Within OpenNetVM's [main.c file][onvm_main_c], the master thread initializes the system, starts other threads, then runs the stats in an infinite loop until the user kills or interrupts the process. The stats thread sleeps for a set amount of time, then updates the statistics. When run in web mode, it does this by outputting stats about ports and NFs into `onvm_json_stats.json` and outputting an event list into `onvm_json_events.json`, truncating the existing version of the file and overwriting with each iteration of the loop.

The events system is new and currently events are created for port initialization and NF starting, ready, and stopping. In the future, this should be used for more complex events such as service chain based events, and for core mappings once shared CPU is completed.

The existing code for the web frontend (which primarily used jquery) has been rewritten and expanded upon in React, using Flow for type checking rather than a full TypeScript implementation. This allows us to maintain application state across pages and to restore graphs to the fully updated state when returning to a graph from a different page. The [AppWrapper.react.js][app_wrapper_react_js] makes periodic web requests and updates global state.

Global state is managed via a small publisher/subscriber library located in [pubsub.js][pubsub_js]. This was chosen over Redux as our use case does not require the full overhead of Redux. Our publisher/subscriber library allows React components to easily subscribe to new NF data or new event data upon component mount, and unsubscribe upon component unmount. The NFGraph components enable restoration of previously encountered data onto the graph upon component remount. To maintain performance, [pubsub.js][pubsub_js] only retains the last 40 data points per NF and Port.

**Please note:** If you restart the manager then you will need to refresh the web page to reset the application state.

## Build Installs

The ReactJS app is ready for execution, but follow this process if you need to edit `onvm_web/react-app`
Assuming you're using an ubuntu system
Remember to run commands in sudo for nodejs and npm

Install latest stable Node.js version
```sh
curl -sL https://deb.nodesource.com/setup_6.x | sudo -E bash -
# Install Node.js from the Debian-based distributions repository
sudo apt-get install -y nodejs
# Use package manager
sudo npm cache clean -f
sudo npm install -g n
sudo n stable
```

Install yarn with npm
```sh
sudo npm install -g yarn
```

Install programs to minify web files
```sh
sudo npm install uglify-js -g
sudo npm install uglifycss -g
```

[install]: ../docs/Install.md
[examples]: ../docs/Examples.md
[start_web]: ./start_web_console.sh
[simplehttp]: https://docs.python.org/2/library/simplehttpserver.html
[onvm_main_c]: ../onvm/onvm_mgr/main.c
[app_wrapper_react_js]: ./react-app/src/AppWrapper.react.js
[pubsub_js]: ./react-app/src/pubsub.js
