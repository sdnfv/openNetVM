/*! For license information please see module.js.LICENSE.txt */
define(["react", "@grafana/data", "@grafana/ui"], function(e, t, r) {
  return (function(e) {
    var t = {};
    function r(n) {
      if (t[n]) return t[n].exports;
      var a = (t[n] = { i: n, l: !1, exports: {} });
      return e[n].call(a.exports, a, a.exports, r), (a.l = !0), a.exports;
    }
    return (
      (r.m = e),
      (r.c = t),
      (r.d = function(e, t, n) {
        r.o(e, t) || Object.defineProperty(e, t, { enumerable: !0, get: n });
      }),
      (r.r = function(e) {
        "undefined" != typeof Symbol &&
          Symbol.toStringTag &&
          Object.defineProperty(e, Symbol.toStringTag, { value: "Module" }),
          Object.defineProperty(e, "__esModule", { value: !0 });
      }),
      (r.t = function(e, t) {
        if ((1 & t && (e = r(e)), 8 & t)) return e;
        if (4 & t && "object" == typeof e && e && e.__esModule) return e;
        var n = Object.create(null);
        if (
          (r.r(n),
          Object.defineProperty(n, "default", { enumerable: !0, value: e }),
          2 & t && "string" != typeof e)
        )
          for (var a in e)
            r.d(
              n,
              a,
              function(t) {
                return e[t];
              }.bind(null, a)
            );
        return n;
      }),
      (r.n = function(e) {
        var t =
          e && e.__esModule
            ? function() {
                return e.default;
              }
            : function() {
                return e;
              };
        return r.d(t, "a", t), t;
      }),
      (r.o = function(e, t) {
        return Object.prototype.hasOwnProperty.call(e, t);
      }),
      (r.p = "/"),
      r((r.s = 3))
    );
  })([
    function(t, r) {
      t.exports = e;
    },
    function(e, r) {
      e.exports = t;
    },
    function(e, t) {
      e.exports = r;
    },
    function(e, t, r) {
      "use strict";
      r.r(t);
      var n = r(1),
        a = function(e, t) {
          return (a =
            Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array &&
              function(e, t) {
                e.__proto__ = t;
              }) ||
            function(e, t) {
              for (var r in t) t.hasOwnProperty(r) && (e[r] = t[r]);
            })(e, t);
        };
      function o(e, t) {
        function r() {
          this.constructor = e;
        }
        a(e, t),
          (e.prototype =
            null === t
              ? Object.create(t)
              : ((r.prototype = t.prototype), new r()));
      }
      var i = function() {
        return (i =
          Object.assign ||
          function(e) {
            for (var t, r = 1, n = arguments.length; r < n; r++)
              for (var a in (t = arguments[r]))
                Object.prototype.hasOwnProperty.call(t, a) && (e[a] = t[a]);
            return e;
          }).apply(this, arguments);
      };
      function u(e) {
        var t = "function" == typeof Symbol && e[Symbol.iterator],
          r = 0;
        return t
          ? t.call(e)
          : {
              next: function() {
                return (
                  e && r >= e.length && (e = void 0),
                  { value: e && e[r++], done: !e }
                );
              }
            };
      }
      function l(e, t) {
        var r = "function" == typeof Symbol && e[Symbol.iterator];
        if (!r) return e;
        var n,
          a,
          o = r.call(e),
          i = [];
        try {
          for (; (void 0 === t || t-- > 0) && !(n = o.next()).done; )
            i.push(n.value);
        } catch (e) {
          a = { error: e };
        } finally {
          try {
            n && !n.done && (r = o.return) && r.call(o);
          } finally {
            if (a) throw a.error;
          }
        }
        return i;
      }
      function s() {
        for (var e = [], t = 0; t < arguments.length; t++)
          e = e.concat(l(arguments[t]));
        return e;
      }
      var c = (function(e) {
        function t(t) {
          var r = e.call(this, t) || this;
          return (
            (r.data = []),
            t.jsonData.data &&
              (r.data = t.jsonData.data.map(function(e) {
                return Object(n.toDataFrame)(e);
              })),
            r
          );
        }
        return (
          o(t, e),
          (t.prototype.getQueryDisplayText = function(e) {
            return e.data
              ? "Panel Data: " + d(e.data)
              : "Shared Data From: " + this.name + " (" + d(this.data) + ")";
          }),
          (t.prototype.metricFindQuery = function(e, t) {
            var r = this;
            return new Promise(function(e, t) {
              var n,
                a,
                o,
                i,
                l = [];
              try {
                for (var s = u(r.data), c = s.next(); !c.done; c = s.next()) {
                  var f = c.value;
                  try {
                    for (
                      var d = ((o = void 0), u(f.fields)), p = d.next();
                      !p.done;
                      p = d.next()
                    ) {
                      var h = p.value;
                      l.push({ text: h.name });
                    }
                  } catch (e) {
                    o = { error: e };
                  } finally {
                    try {
                      p && !p.done && (i = d.return) && i.call(d);
                    } finally {
                      if (o) throw o.error;
                    }
                  }
                }
              } catch (e) {
                n = { error: e };
              } finally {
                try {
                  c && !c.done && (a = s.return) && a.call(s);
                } finally {
                  if (n) throw n.error;
                }
              }
              e(l);
            });
          }),
          (t.prototype.query = function(e) {
            var t,
              r,
              a = [];
            try {
              for (var o = u(e.targets), l = o.next(); !l.done; l = o.next()) {
                var s = l.value;
                if (!s.hide) {
                  var c = this.data;
                  s.data &&
                    (c = s.data.map(function(e) {
                      return Object(n.toDataFrame)(e);
                    }));
                  for (var f = 0; f < c.length; f++)
                    a.push(i(i({}, c[f]), { refId: s.refId }));
                }
              }
            } catch (e) {
              t = { error: e };
            } finally {
              try {
                l && !l.done && (r = o.return) && r.call(o);
              } finally {
                if (t) throw t.error;
              }
            }
            return Promise.resolve({ data: a });
          }),
          (t.prototype.testDatasource = function() {
            var e = this;
            return new Promise(function(t, r) {
              var n,
                a,
                o = 0,
                i = e.data.length + " Series:";
              try {
                for (var l = u(e.data), s = l.next(); !s.done; s = l.next()) {
                  var c = s.value,
                    f = c.length;
                  (i += " [" + c.fields.length + " Fields, " + f + " Rows]"),
                    (o += f);
                }
              } catch (e) {
                n = { error: e };
              } finally {
                try {
                  s && !s.done && (a = l.return) && a.call(l);
                } finally {
                  if (n) throw n.error;
                }
              }
              o > 0 && t({ status: "success", message: i }),
                r({ status: "error", message: "No Data Entered" });
            });
          }),
          t
        );
      })(n.DataSourceApi);
      function f(e) {
        return e && e.fields && e.fields.length
          ? e.hasOwnProperty("length")
            ? e.length
            : e.fields[0].values.length
          : 0;
      }
      function d(e) {
        if (!e || !e.length) return "";
        if (e.length > 1) {
          var t = e.reduce(function(e, t) {
            return e + f(t);
          }, 0);
          return e.length + " Series, " + t + " Rows";
        }
        var r = e[0];
        if (!r.fields) return "Missing Fields";
        var n = f(r);
        return r.fields.length + " Fields, " + n + " Rows";
      }
      var p = r(0),
        h = r.n(p),
        v = r(2);
      function y(e) {
        return e && e.length
          ? Object(n.toCSV)(
              e.map(function(e) {
                return Object(n.toDataFrame)(e);
              })
            )
          : "";
      }
      var m = v.LegacyForms.Select,
        g = [
          {
            value: "panel",
            label: "Panel",
            description: "Save data in the panel configuration."
          },
          {
            value: "shared",
            label: "Shared",
            description: "Save data in the shared datasource object."
          }
        ],
        b = (function(e) {
          function t() {
            var t = (null !== e && e.apply(this, arguments)) || this;
            return (
              (t.state = { text: "" }),
              (t.onSourceChange = function(e) {
                var r = t.props,
                  a = r.datasource,
                  o = r.query,
                  u = r.onChange,
                  l = r.onRunQuery,
                  c = void 0;
                if ("panel" === e.value) {
                  if (o.data) return;
                  (c = s(a.data)) || (c = [new n.MutableDataFrame()]),
                    t.setState({ text: Object(n.toCSV)(c) });
                }
                u(i(i({}, o), { data: c })), l();
              }),
              (t.onSeriesParsed = function(e, r) {
                var a = t.props,
                  o = a.query,
                  u = a.onChange,
                  l = a.onRunQuery;
                t.setState({ text: r }),
                  e || (e = [new n.MutableDataFrame()]),
                  u(i(i({}, o), { data: e })),
                  l();
              }),
              t
            );
          }
          return (
            o(t, e),
            (t.prototype.onComponentDidMount = function() {
              var e = y(this.props.query.data);
              this.setState({ text: e });
            }),
            (t.prototype.render = function() {
              var e = this.props,
                t = e.datasource,
                r = e.query,
                n = t.id,
                a = t.name,
                o = this.state.text,
                i = r.data ? g[0] : g[1];
              return h.a.createElement(
                "div",
                null,
                h.a.createElement(
                  "div",
                  { className: "gf-form" },
                  h.a.createElement(v.InlineFormLabel, { width: 4 }, "Data"),
                  h.a.createElement(m, {
                    width: 6,
                    options: g,
                    value: i,
                    onChange: this.onSourceChange
                  }),
                  h.a.createElement(
                    "div",
                    { className: "btn btn-link" },
                    r.data
                      ? d(r.data)
                      : h.a.createElement(
                          "a",
                          { href: "datasources/edit/" + n + "/" },
                          a,
                          ": ",
                          d(t.data),
                          "   ",
                          h.a.createElement(v.Icon, { name: "pen" })
                        )
                  )
                ),
                r.data &&
                  h.a.createElement(v.TableInputCSV, {
                    text: o,
                    onSeriesParsed: this.onSeriesParsed,
                    width: "100%",
                    height: 200
                  })
              );
            }),
            t
          );
        })(p.PureComponent),
        S = (function(e) {
          function t() {
            var t = (null !== e && e.apply(this, arguments)) || this;
            return (
              (t.state = { text: "" }),
              (t.onSeriesParsed = function(e, r) {
                var a = t.props,
                  o = a.options,
                  u = a.onOptionsChange;
                e || (e = [new n.MutableDataFrame()]);
                var l = i(i({}, o.jsonData), { data: e });
                u(i(i({}, o), { jsonData: l })), t.setState({ text: r });
              }),
              t
            );
          }
          return (
            o(t, e),
            (t.prototype.componentDidMount = function() {
              var e = this.props.options;
              if (e.jsonData.data) {
                var t = y(e.jsonData.data);
                this.setState({ text: t });
              }
            }),
            (t.prototype.render = function() {
              var e = this.state.text;
              return h.a.createElement(
                "div",
                null,
                h.a.createElement(
                  "div",
                  { className: "gf-form-group" },
                  h.a.createElement("h4", null, "Shared Data:"),
                  h.a.createElement("span", null, "Enter CSV"),
                  h.a.createElement(v.TableInputCSV, {
                    text: e,
                    onSeriesParsed: this.onSeriesParsed,
                    width: "100%",
                    height: 200
                  })
                ),
                h.a.createElement(
                  "div",
                  { className: "grafana-info-box" },
                  "This data is stored in the datasource json and is returned to every user in the initial request for any datasource. This is an appropriate place to enter a few values. Large datasets will perform better in other datasources.",
                  h.a.createElement("br", null),
                  h.a.createElement("br", null),
                  h.a.createElement("b", null, "NOTE:"),
                  " Changes to this data will only be reflected after a browser refresh."
                )
              );
            }),
            t
          );
        })(p.PureComponent);
      r.d(t, "plugin", function() {
        return x;
      });
      var x = new n.DataSourcePlugin(c).setConfigEditor(S).setQueryEditor(b);
    }
  ]);
});
//# sourceMappingURL=module.js.map
