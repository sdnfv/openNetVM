# openNetVM Web Console for Statistics

## About

Resources to view openNetVM Manager statistics through a web console.

The [start web console][start_web] script will run cors_server.py (which is an extension of [Python's SimpleHTTPServer][simplehttp] which enables CORS) on port 8080 by default, or on a port of your specification using the `-p` flag. See more details for this in the **Usage** section below.

OpenNetVM web now supports using Grafana with Prometheus to extract more data metrics. Grafana server will run on its default port 3000, but the Grafana page can be accessed using the current web console.

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
You could use the browser to monitor the stats of openNetVM manager.

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

This would only start the web consle and display the web status, it would not start the onvm manager!

## Install Guide

Because Prometheus, Grafana and pushgateway api are running on their official docker image, please make sure you have `docker.io` installed on your machine.
If not, please run following command to install it first, otherwise prometheus server may not be able to properly started.

```sh
sudo apt install docker.io
```

You also need to install pushgateway python client in order to run pushgateway
```sh
sudo pip install prometheus_client
```

The python server had been updated to flask server. In order to use flask server you need to install flask and flaks CORS in order to run the server
```sh
sudo pip install flask
sudo pip install -U flask_cors
```

## Design and Implementation

Within OpenNetVM's [main.c file][onvm_main_c], the master thread initializes the system, starts other threads, then runs the stats in an infinite loop until the user kills or interrupts the process. The stats thread sleeps for a set amount of time, then updates the statistics. When run in web mode, it does this by outputting stats about ports and NFs into `onvm_json_stats.json` and outputting an event list into `onvm_json_events.json`, truncating the existing version of the file and overwriting with each iteration of the loop.

The events system is new and currently events are created for port initialization and NF starting, ready, and stopping. In the future, this should be used for more complex events such as service chain based events, and for core mappings once shared CPU is completed.

The existing code for the web frontend (which primarily used jquery) has been rewritten and expanded upon in React, using Flow for type checking rather than a full TypeScript implementation. This allows us to maintain application state across pages and to restore graphs to the fully updated state when returning to a graph from a different page. The [AppWrapper.react.js][app_wrapper_react_js] makes periodic web requests and updates global state.

Global state is managed via a small publisher/subscriber library located in [pubsub.js][pubsub_js]. This was chosen over Redux as our use case does not require the full overhead of Redux. Our publisher/subscriber library allows React components to easily subscribe to new NF data or new event data upon component mount, and unsubscribe upon component unmount. The NFGraph components enable restoration of previously encountered data onto the graph upon component remount. To maintain performance, [pubsub.js][pubsub_js] only retains the last 40 data points per NF and Port.

**Please note:** If you restart the manager then you will need to refresh the web page to reset the application state.

## Build Installs

A bug had been reported that net-tools does not come with base minimal install of Ubuntu 18.04, which could cause a run-time error.
The following install command should solve that if you do not have net-tools installed.

```sh
sudo apt-get install net-tools
```

The ReactJS app is ready for execution, but follow this process if you need to edit `onvm_web/react-app`
Assuming you're using an ubuntu system
Remember to run commands in sudo for nodejs and npm

Install latest stable Node.js version
```sh
curl -sL https://deb.nodesource.com/setup_13.x | sudo -E bash -
# Install Node.js from the Debian-based distributions repository
sudo apt-get install -y nodejs
# Use package manager
sudo npm cache clean -f
sudo npm install -g n
# Install the current stable version of node
sudo n stable
```

npm should come automatically with Node.js, but on the rare occasion you do not have npm, you need to install it yourself.
```sh
sudo apt-get install npm
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

## About Grafana

OpenNetVM uses Grafana to display more data metrics collected by Prometheus. Currently the Grafana provisioning is set up to use prometheus as its default data source. The default dashboard will display some information about current CPU usage.

To create new dashboards, you can simply create it in the Grafana server and then save the json format config file, and save it in grafana/conf/provisioning/dashboards folder. The provisioning file will automatically upload any new json format dashboards dynamically.

To set up a new data source, you need to modify [datasource.yaml][datasource]. You can add additional data source below the current one, for more information on how to set up provision file, you can check [grafana_provision][Grafana Provision] for detailed information. The [datasource.yaml][datasource] also contains explainations on how to set up new data source.

### About Prometheus

Prometheus is a tool for collecting metrics. It also provides an internal query language PromQL that can search through the metrics and selectively represent them. The Prometheus server is running from the official docker image. The [Prometheus.yml][prometheus] set up file sets up the Prometheus to listen on the node exporter. To modify it, check out [prometheus_setup][prometheus setup page] for more information.

Prometheus works with different exporters and collects information from different exporters. The node exporter can mainly extract information on your current machine. But there are other exporters that can be used to extract more information.

[install]: ../docs/Install.md
[examples]: ../docs/Examples.md
[start_web]: ./start_web_console.sh
[simplehttp]: https://docs.python.org/2/library/simplehttpserver.html
[onvm_main_c]: ../onvm/onvm_mgr/main.c
[app_wrapper_react_js]: ./react-app/src/AppWrapper.react.js
[pubsub_js]: ./react-app/src/pubsub.js
[datasource.yaml]: ./conf/provisioning/datasources/datasources.yaml
[grafana_provision]: https://grafana.com/docs/grafana/latest/administration/provisioning/#provisioning-grafana
[Prometheus.yml]: ./Prometheus.yml
[prometheus_setup]: https://prometheus.io/docs/prometheus/latest/configuration/configuration/