openNetVM Web Console for Statistics
==

About
--

Resources to view openNetVM Manager statistics through a web based console.

The [start web console][start_web] script will run [Python SimpleHTTPServer][simplehttp] on port 8080.

Configuration
--

You may provide a config.json file in this directory containing the following variables:

* `y_min`: minimum Y value shown on the graphs (default = 0)
* `y_max`: maximum Y value shown on the graphs (default = 30M)
* `x_range`: maximum number of seconds shown on the graph on the X axis at once (default = 30)
* `refresh_rate`: Number of **milliseconds** to update the graphs (default = 3000)

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

The web view also provides the user with the ability to download a CSV containing all data points recorded **by the current browser session**.  Please note that refreshing the page will remove these data points and reset the data the browser has recorded.

Usage
--

Start the openNetVM Manager following steps in either the [install
doc][install] or the [example usage guide][examples] and be sure to use
the `-s` flag supplying the argument `web`.

The following example runs openNetVM with four cores (1 for stats
display, 1 for NIC Rx, 1 for NIC Tx, and 1 for NF Tx), a portmask of 1
to use 1 NIC port, and sets the stats output to the web console.
```sh
cd onvm
./go.sh 0,1,2,3 1 -s web
```

Doing that will launch openNetVM and use the console to display log
messages.  It will print out a message stating that the web stats
console has been setup and information to access it.

Next, we run the [start web console][start_web] script and point a
browser to `http://<Address of ONVM host>:8080`
```sh
cd onvm_web
./start_web_console.sh
```

In your web browser, you will see statistics regarding openNetVM NIC
performance, each NF's Rx and Tx performance, and the raw stats output.


[install]: ../docs/Install.md
[examples]: ../docs/Examples.md
[start_web]: ./start_web_console.sh
[chartjs]: http://www.chartjs.org/
[simplehttp]: https://docs.python.org/2/library/simplehttpserver.html
