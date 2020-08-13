# Grafana usage guide

## Basic usage

Grafana is running at `localhost:3000` by default. The default username and password are both `admin`. Upon first login, you can change the default password.

Grafana has been set to listen to Prometheus by default. Prometheus is currently set up to listen to node_exporter. The default dashboards in Grafana is showing the data collected by Prometheus.

There are two default dashboards, one is downloaded from grafana website, the other is a demo on how to setup a new dashboard. In the dashboard page you can select time interval, auto refresh rate on the top right corner.

## Add new data souce

To add new data source, select `configuration` then select `data sources`. Select the type of data source then enter the URL for that data source. There are other specific settings under that.

## Add new dashboard

You can add new dashboards by either make a new one or import one from the [grafana_lab][grafana website]. If you want to make one by yourself, you can select `create` then `dashboard` and make a new dashboard. Then you can add panels to the dashboard. In the panel you can add queries to select data to display.

Alternatively you can select dashboards from grafana website. There are many useful dashboards in the website to use. To import a dashboard, first copy the dashboard ID, then select `create` then `import` and paste the ID.

## Edit grafana provisioning

The grafana provisioning files are in the [grafana_provision][grafana provision] folder. Grafana can add provsions for dashboard, data source and notifiers. You only need to modify the respective yaml files in each folder.

To add new data source, you need to keep adding more entires after `prometheus`. In it you must include name of the source, type of the source (e.g. grafana, influxdb, grafite etc.), type of access (direct or proxy), and the URL of the data source. For the URL, if you are using a data souce that is localhost, regardless of using your own machine, nimbus, or cloudlab, you can write it in the format of `http://HOSTIP:PORT_NUMBER`, you only need to replace PORT_NUMBER with the actual port number, but you can leave HOSTIP as it is, the [start_web_sh][start_web_console.sh] will automatically fill in that host IP for you.

There are other optional field you can include in your settings. They are all listed in the yaml file with some explanation.

To add new dashboard, you do not need to change the yaml file. You only need to copy the json dashboard file into [grafana_dashboard][grafana dashboards] folder. Grafana will automatically read the dashboards from that folder.

To add notifiers, just follow the examples in the yaml file and fill in the required field.

## Edit grafana dashboard

The grafana dashboard can be created by click the `create` button on the left and then create a new dashboard. Because some of the data collected are specific to each nf, it would be better to create new panels as you start up more nfs. Click add new panel on the dashboard page, and on the bottom left conor you can select specific metrics you want to display. Click that and you should see all the data metrics collected by grafana.

You can select any of them to display them. To learn more about advanced query for grafana and prometheus, visit this page [prometheus_query][prometheus query].

## Save dashboard to local

The simplest way to save dashboards is click `setting` button on the top right cornor of your dashboard page. Then find JSON Model on the left, and you should see the JSON form data of your dashboard. Copy paste that to your local device or save it in [grafana_dashboard][grafana dashboards] folder.

# About Prometheus

## How to push data to Prometheus

Edit [pushgateway_script][pushgateway_monitor.py] to push data to prometheus pushgateway api. To do that, you can either make a new collector registry or use the same one. You need to push new data to the pushgateway URL with a description. For more information on how to use pushgateway client please visit [pushgateway_website][pushgateway python client] page.

[grafana_lab]: https://grafana.com/grafana/plugins
[grafana_provision]: ./provisioning
[start_web_sh]: ./start_web_console.sh
[grafana_dashboard]: ./provisioning/dashboards
[prometheus_query]: https://prometheus.io/docs/prometheus/latest/querying/basics/
[pushgateway_script]: ./pushgateway_monitor.py
[pushgateway_website]: https://github.com/prometheus/client_python