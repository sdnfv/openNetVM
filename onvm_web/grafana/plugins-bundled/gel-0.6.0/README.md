# GEL (Grafana Expressions)

GEL runs as a new type of Transform Plugin and works with Grafana master if the `expressions` flag under `feature_toggles` is set. This is only a backend plugin (front-end transform (expression) component as it is part of Grafana).

**Warning: This is an alpha/preview feature. It is currently subject breaking and major design changes (including the project/feature name, and code location). Enabling this feature currently allows one to bypass datasource permissions and security.**

GEL preforms data transformations on backend and built-in datasources such as reduction, time series resampling, and math.

## Instructions

Last Updated: Nov 1, 2019

Currently Grafana `master` will run GEL as a backend transform plugin. When you have built it, Grafana should run the GEL transform plugin.

### Building

Build the Go backend (this plugin has no front-end component as it is part of Grafana).

1. `make vendor`
2. `make build` (on Linux), (or `make build-darwin` for mac) - which should output an executable to `dist/`.

### Running

Your Grafana configuration needs the following to enable use of the GEL plugin:

```ini
[feature_toggles]
enable = expressions
```

The plugin will need to be added to grafana's `data/plugins` through your preferred method. One way is to create a symlink, in the `data/plugins` directory something like `ln -s /home/kbrandt/src/github.com/grafana/gel-app/ gel-app`.

If you enable debug logging in Grafana's ini you should see the plugin executed (or errors/problems). You should also see a gel process running after grafana starts `ps aux | grep gel | grep -v grep` or task manager or something :-).

### Using GEL

When using a datasource, select "Add Expression". This will cause the queries to go through the Transform plugin (GEL).

To reference a datasource query or another expression use the refId as a variable. For example `$A + 5` in the math text field, or `$A` in the field of reduction.

#### Caveats

Currently the Mixed query editor has an issue if you use the "default" datasource. However, you can use whatever datasource default points to without issue.
