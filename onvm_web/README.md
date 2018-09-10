openNetVM Web Console for Statistics
==

About
--

Resources to view openNetVM Manager statistics through a web console.

The [start web console][start_web] script will run [Python SimpleHTTPServer][simplehttp] on port 8080 by default, or on a port of your specification using the `-p` flag.  See more details for this in the **Usage** section below.

Configuration
--

You may provide a config.json file in this directory containing the following variables:

* `y_min`: minimum Y value shown on the graphs (default = 0)
* `y_max`: maximum Y value shown on the graphs (default = 30M)
* `x_range`: maximum number of seconds shown on the graph on the X axis at once (default = 30)
* `refresh_rate`: Number of **milliseconds** to update the graphs (default = 3000)

**NOTE:** The refresh rate in the config.json file can be any value, but please know that the manager only updates stats every **1 second** ([main.c:95][sleep_file]).  As such, _please refrain from using refresh rates faster than 1 second_.

If no config.json file is provided, or if invalid options are provided, the system will use the defaults.

An example `config.json` setup is provided below using the default parameters:

```
{
        "y_min": 0,
        "y_max": 30000000,
        "x_range": 30,
        "refresh_rate": 3000
}
```                                                                

CSV Download
--

The web view also provides the user with the ability to download a CSV containing all data points recorded **by the current browser session**.  Please note that refreshing the page will remove these data points and reset the data the browser has recorded.  Using the [csv analysis script][csv_script], you can obtain meaningful, human readable information from this CSV.

Usage
--

Start the openNetVM Manager following steps in either the [install
doc][install] or the [example usage guide][examples] and be sure to use
the `-s` flag supplying the argument `web`.  If the `-s` flag is supplied,
the web console will automatically be launched for you using the port 
number you have specified on the manager (using the manager's `-p` flag).
If no port is specified, it will run on port 8080.

The following example runs openNetVM with four cores (1 for stats
display, 1 for NIC Rx, 1 for NIC Tx, and 1 for NF Tx), a portmask of 1
to use 1 NIC port, and sets the stats output to the web console.
```sh
cd onvm
./go.sh 0,1,2,3 1 -s web
```

The following example runs openNetVM with four cores (1 for stats
display, 1 for NIC Rx, 1 for NIC Tx, and 1 for NF Tx), a portmask of 1
to use 1 NIC port, and sets the stats output to the web console.  It also runs
the web dashhboard on port 9000.
```sh
cd onvm
./go.sh 0,1,2,3 1 -s web -p 9000
```

Doing that will launch openNetVM and use the console to display log
messages.  It will print out a message stating that the web stats
console has been setup and information to access it.

If something were to go wrong in the launching of the web stats, we could
manually start it ourselves by doing the following:
```sh
cd onvm_web
./start_web_console.sh
```

We could also specify a port argument using the `-p` flag:
```sh
cd onvm_web
./start_web_console.sh -p 9000
```


In your web browser, you will see statistics regarding openNetVM NIC
performance, each NF's Rx and Tx performance, and the raw stats output.


[install]: ../docs/Install.md
[examples]: ../docs/Examples.md
[start_web]: ./start_web_console.sh
[chartjs]: http://www.chartjs.org/
[simplehttp]: https://docs.python.org/2/library/simplehttpserver.html
[csv_script]: ../scripts/csv-analysis.py
[sleep_file]: https://github.com/sdnfv/openNetVM-dev/blob/master/onvm/onvm_mgr/main.c#L95
