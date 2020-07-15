(window.webpackJsonp = window.webpackJsonp || []).push([
  [0],
  {
    33: function(e, t, n) {
      e.exports = n(68);
    },
    38: function(e, t, n) {},
    66: function(e, t, n) {},
    68: function(e, t, n) {
      "use strict";
      n.r(t);
      var a = n(0),
        o = n.n(a),
        r = n(19),
        l = n.n(r),
        c = (n(38), n(69)),
        i = n(73),
        s = n(44),
        u = n(70),
        p = n(3),
        f = n(4),
        m = n(6),
        h = n(5),
        d = n(7),
        b = n(71),
        v = n(72),
        g = n(2),
        E = n(12),
        y = {},
        O = {},
        C = [],
        L = 0,
        j = null,
        N = -1;
      function k(e, t, n) {
        var a = {
          name: e,
          callback: t,
          includePastEvents: n,
          hasBeenCalled: !1
        };
        if (n && j) {
          var o = j;
          console.log("Catching event subscriber up to all past events.");
          for (var r = 0; r !== N + 1; ) a.callback(o[r]), ++r;
          a.hasBeenCalled = !0;
        }
        C.push(a);
      }
      function w(e) {
        C = C.filter(function(t) {
          return t.name !== e;
        });
      }
      function F(e, t) {
        if (e.length < t) return e;
        var n = e[0];
        for (e = e.slice(2); e.length > t; ) e = e.slice(1);
        return [n].concat(Object(E.a)(e));
      }
      function x(e, t) {
        if ((e in O ? O[e].push(t) : (O[e] = [t]), e in y)) return y[e];
      }
      function S(e, t, n) {
        e in O &&
          (0 ===
            O[e].filter(function(e) {
              return e !== t;
            }).length && delete O[e],
          null != n && (y[e] = n));
      }
      var P = (function(e) {
          function t() {
            var e, n;
            Object(p.a)(this, t);
            for (var a = arguments.length, o = new Array(a), r = 0; r < a; r++)
              o[r] = arguments[r];
            return (
              ((n = Object(m.a)(
                this,
                (e = Object(h.a)(t)).call.apply(e, [this].concat(o))
              )).state = { interval: null }),
              n
            );
          }
          return (
            Object(d.a)(t, e),
            Object(f.a)(t, [
              {
                key: "dataFetcher",
                value: function() {
                  var e = window.location.hostname;
                  fetch("http://".concat(e, ":8000/onvm_json_stats.json"))
                    .then(function(e) {
                      return e.json();
                    })
                    .then(function(e) {
                      return (function(e) {
                        var t = e.onvm_nf_stats,
                          n = function(e) {
                            if (e in O)
                              O[e].forEach(function(n) {
                                return n(t[e], L);
                              });
                            else if (
                              (console.log(
                                "No NF Subscriber callback for label: " + e
                              ),
                              e in y)
                            ) {
                              var n = t[e],
                                a = y[e];
                              a[0].push(L),
                                (a[0] = F(a[0], 40)),
                                a[1].push(n.TX),
                                (a[1] = F(a[1], 40)),
                                a[2].push(n.RX),
                                (a[2] = F(a[2], 40));
                            } else
                              console.error(
                                "No NF Restore state for label: " + e
                              );
                          };
                        for (var a in t) n(a);
                        var o = e.onvm_port_stats,
                          r = function(e) {
                            if (e in O)
                              O[e].forEach(function(t) {
                                return t(o[e], L);
                              });
                            else if (
                              (console.log(
                                "No NF Subscriber callback for label: " + e
                              ),
                              e in y)
                            ) {
                              var t = o[e],
                                n = y[e];
                              n[0].push(L),
                                (n[0] = F(n[0], 40)),
                                n[1].push(t.TX),
                                (n[1] = F(n[1], 40)),
                                n[2].push(t.RX),
                                (n[2] = F(n[2], 40));
                            } else
                              console.error(
                                "No NF Restore state for label: " + e
                              );
                          };
                        for (var l in o) r(l);
                        ++L;
                      })(e);
                    })
                    .catch(function(e) {
                      return console.error(e);
                    }),
                    fetch("http://".concat(e, ":8000/onvm_json_events.json"))
                      .then(function(e) {
                        return e.json();
                      })
                      .then(function(e) {
                        return (function(e) {
                          for (
                            var t = function() {
                              var t = e[++N];
                              t &&
                                C.forEach(function(e) {
                                  return e.callback(t);
                                });
                            };
                            N < e.length - 1;

                          )
                            t();
                          j = e;
                        })(e);
                      })
                      .catch(function(e) {
                        return console.error(e);
                      });
                }
              },
              {
                key: "componentDidMount",
                value: function() {
                  var e = setInterval(this.dataFetcher, 3e3);
                  this.setState({ interval: e });
                }
              },
              {
                key: "componentWillUnmount",
                value: function() {
                  var e = this.state.interval;
                  e && clearInterval(e);
                }
              },
              {
                key: "render",
                value: function() {
                  var e = this.props.children,
                    t = {
                      itemsObjects: [
                        {
                          value: "NF Dashboard",
                          to: "/nfs",
                          icon: "home",
                          LinkComponent: Object(b.a)(v.a)
                        },
                        {
                          value: "Port Dashboard",
                          to: "/ports",
                          icon: "server",
                          LinkComponent: Object(b.a)(v.a)
                        },
                        {
                          value: "Core Mappings",
                          to: "/core-mappings",
                          icon: "cpu",
                          LinkComponent: Object(b.a)(v.a)
                        },
                        {
                          value: "Grafana Page",
                          to: "/grafana",
                          icon: "home",
                          LinkComponent: Object(b.a)(v.a)
                        },
                        {
                          value: "NF Chain Launch",
                          to: "/nf-chain",
                          icon: "ports",
                          LinkComponent: Object(b.a)(v.a)
                        }
                      ]
                    };
                  return a.createElement(
                    g.f.Wrapper,
                    {
                      headerProps: {
                        href: "/",
                        alt: "openNetVM",
                        imageURL: "/onvm-logo.png"
                      },
                      navProps: t,
                      footerProps: {}
                    },
                    e
                  );
                }
              }
            ]),
            t
          );
        })(a.PureComponent),
        R = n(14),
        D = n(30),
        _ = n.n(D),
        A = {
          x: { label: { text: "X Axis", position: "outer-center" } },
          y: { label: { text: "Y Axis", position: "outer-middle" } }
        },
        M = (function(e) {
          function t() {
            var e, n;
            Object(p.a)(this, t);
            for (var a = arguments.length, o = new Array(a), r = 0; r < a; r++)
              o[r] = arguments[r];
            return (
              ((n = Object(m.a)(
                this,
                (e = Object(h.a)(t)).call.apply(e, [this].concat(o))
              )).state = {
                graphData: {
                  xs: {
                    tx_pps: "".concat(n.props.nfLabel, "x1"),
                    rx_pps: "".concat(n.props.nfLabel, "x1")
                  },
                  names: { tx_pps: "TX PPS", rx_pps: "RX PPS" },
                  empty: { label: { text: "No Data to Display" } },
                  columns: [
                    ["".concat(n.props.nfLabel, "x1")],
                    ["tx_pps"],
                    ["rx_pps"]
                  ]
                }
              }),
              (n.dataCallback = function(e, t) {
                var a = Object(R.a)({}, n.state.graphData),
                  o = a.columns;
                o[0].push(t),
                  (o[0] = n.trimToSize(o[0], 40)),
                  o[1].push(e.TX),
                  (o[1] = n.trimToSize(o[1], 40)),
                  o[2].push(e.RX),
                  (o[2] = n.trimToSize(o[2], 40)),
                  n.setState({ graphData: a });
              }),
              (n.trimToSize = function(e, t) {
                if (e.length < t) return e;
                var n = e[0];
                for (e = e.slice(2); e.length > t; ) e = e.slice(1);
                return [n].concat(Object(E.a)(e));
              }),
              n
            );
          }
          return (
            Object(d.a)(t, e),
            Object(f.a)(t, [
              {
                key: "componentDidMount",
                value: function() {
                  console.log("Graph Mount: " + this.props.nfLabel);
                  var e = "NF ".concat(this.props.nfLabel.split(" - ")[1]);
                  "Port" === this.props.nfLabel.substring(0, 4) &&
                    (e = this.props.nfLabel);
                  var t = x(e, this.dataCallback);
                  if (t) {
                    console.log("Graph Restore: " + this.props.nfLabel);
                    var n = Object(R.a)({}, this.state.graphData);
                    (n.columns = t), this.setState({ graphData: n });
                  }
                }
              },
              {
                key: "componentWillUnmount",
                value: function() {
                  console.log("Graph Unmount: " + this.props.nfLabel);
                  var e = "NF ".concat(this.props.nfLabel.split(" - ")[1]);
                  "Port" === this.props.nfLabel.substring(0, 4) &&
                    (e = this.props.nfLabel),
                    S(e, this.dataCallback, this.state.graphData.columns);
                }
              },
              {
                key: "render",
                value: function() {
                  var e = this,
                    t =
                      null === this.props.showMoreInfoButton ||
                      void 0 === this.props.showMoreInfoButton ||
                      this.props.showMoreInfoButton,
                    n = this.props.extraContent;
                  return a.createElement(
                    g.c,
                    null,
                    a.createElement(
                      g.c.Header,
                      null,
                      a.createElement(g.c.Title, null, this.props.nfLabel),
                      t &&
                        a.createElement(
                          g.c.Options,
                          null,
                          a.createElement(
                            g.b,
                            {
                              RootComponent: "a",
                              color: "secondary",
                              size: "sm",
                              className: "ml-2",
                              onClick: function() {
                                var t = e.props.history;
                                t
                                  ? t.push("/nfs/".concat(e.props.nfLabel))
                                  : console.error(
                                      "Failed to go to single NF page"
                                    );
                              }
                            },
                            "View More Info"
                          )
                        )
                    ),
                    a.createElement(
                      g.c.Body,
                      null,
                      a.createElement(_.a, {
                        data: this.state.graphData,
                        axis: A,
                        legend: { show: !0 },
                        padding: { bottom: 0, top: 0 }
                      }),
                      n
                    )
                  );
                }
              }
            ]),
            t
          );
        })(a.PureComponent),
        I = (function(e) {
          function t() {
            var e, n;
            Object(p.a)(this, t);
            for (var a = arguments.length, o = new Array(a), r = 0; r < a; r++)
              o[r] = arguments[r];
            return (
              ((n = Object(m.a)(
                this,
                (e = Object(h.a)(t)).call.apply(e, [this].concat(o))
              )).state = { nfLabelList: [] }),
              (n.eventHandler = function(e) {
                var t;
                console.log(e),
                  "NF Ready" === e.message &&
                    ((t = " - ".concat(e.source.instance_id)),
                    (t = "NF" === e.source.type ? "NF" + t : e.source.type + t),
                    n.setState(function(e) {
                      return {
                        nfLabelList: [t].concat(Object(E.a)(e.nfLabelList))
                      };
                    })),
                  "NF Stopping" === e.message &&
                    ((t = " - ".concat(e.source.instance_id)),
                    (t = "NF" === e.source.type ? "NF" + t : e.source.type + t),
                    console.log(t),
                    n.setState(function(e) {
                      console.log(e.nfLabelList);
                      var n = Object(E.a)(
                        e.nfLabelList.filter(function(e) {
                          return e.split(" - ")[1] !== t.split(" - ")[1];
                        })
                      );
                      return (
                        console.log("end: " + e.nfLabelList), { nfLabelList: n }
                      );
                    }));
              }),
              n
            );
          }
          return (
            Object(d.a)(t, e),
            Object(f.a)(t, [
              {
                key: "componentDidMount",
                value: function() {
                  k("NF DASHBOARD PAGE", this.eventHandler, !0);
                }
              },
              {
                key: "componentWillUnmount",
                value: function() {
                  w("NF DASHBOARD PAGE");
                }
              },
              {
                key: "render",
                value: function() {
                  var e = this.props.history,
                    t = this.state.nfLabelList;
                  return a.createElement(
                    g.e.Content,
                    null,
                    a.createElement(
                      g.d.Row,
                      null,
                      t.map(function(t) {
                        return a.createElement(
                          g.d.Col,
                          { md: 6, xl: 4, key: t },
                          a.createElement(M, { nfLabel: t, history: e })
                        );
                      }),
                      0 === t.length && "No Running NFS to Display!"
                    )
                  );
                }
              }
            ]),
            t
          );
        })(a.PureComponent),
        H = (function(e) {
          function t() {
            var e, n;
            Object(p.a)(this, t);
            for (var a = arguments.length, o = new Array(a), r = 0; r < a; r++)
              o[r] = arguments[r];
            return (
              ((n = Object(m.a)(
                this,
                (e = Object(h.a)(t)).call.apply(e, [this].concat(o))
              )).state = { portList: [] }),
              (n.eventHandler = function(e) {
                if (
                  e.message.includes("Port ") &&
                  e.message.includes(" initialized")
                ) {
                  var t = e.message.replace(" initialized", "");
                  n.setState(function(e) {
                    return { portList: [t].concat(Object(E.a)(e.portList)) };
                  });
                }
              }),
              n
            );
          }
          return (
            Object(d.a)(t, e),
            Object(f.a)(t, [
              {
                key: "componentDidMount",
                value: function() {
                  k("PORT DASHBOARD PAGE", this.eventHandler, !0);
                }
              },
              {
                key: "componentWillUnmount",
                value: function() {
                  w("PORT DASHBOARD PAGE");
                }
              },
              {
                key: "render",
                value: function() {
                  var e = this.state.portList,
                    t = this.props.history;
                  return a.createElement(
                    g.e.Content,
                    null,
                    a.createElement(
                      g.d.Row,
                      null,
                      e.map(function(e) {
                        return a.createElement(
                          g.d.Col,
                          { md: 6, xl: 4, key: e },
                          a.createElement(M, {
                            nfLabel: e,
                            history: t,
                            showMoreInfoButton: !1
                          })
                        );
                      }),
                      0 === e.length && "No ports in use!"
                    )
                  );
                }
              }
            ]),
            t
          );
        })(a.PureComponent);
      var T = function(e) {
          return a.createElement(
            g.g,
            {
              cards: !0,
              striped: !0,
              responsive: !0,
              className: "table-vcenter"
            },
            a.createElement(
              g.g.Header,
              null,
              a.createElement(
                g.g.Row,
                null,
                a.createElement(g.g.ColHeader, null, "Event"),
                a.createElement(g.g.ColHeader, null, "Timestamp")
              )
            ),
            a.createElement(
              g.g.Body,
              null,
              e.events.map(function(e) {
                return a.createElement(
                  g.g.Row,
                  null,
                  a.createElement(g.g.Col, null, e.message),
                  a.createElement(g.g.Col, null, e.timestamp)
                );
              })
            )
          );
        },
        B = (function(e) {
          function t() {
            var e, n;
            Object(p.a)(this, t);
            for (var a = arguments.length, o = new Array(a), r = 0; r < a; r++)
              o[r] = arguments[r];
            return (
              ((n = Object(m.a)(
                this,
                (e = Object(h.a)(t)).call.apply(e, [this].concat(o))
              )).state = { instanceId: null, serviceId: null, core: null }),
              (n.dataCallback = function(e, t) {
                n.setState({
                  instanceId: e.instance_id,
                  serviceId: e.service_id,
                  core: e.core
                });
              }),
              n
            );
          }
          return (
            Object(d.a)(t, e),
            Object(f.a)(t, [
              {
                key: "componentDidMount",
                value: function() {
                  console.log("NF Info Mount: " + this.props.nfLabel),
                    x(
                      "NF ".concat(this.props.nfLabel.split(" - ")[1]),
                      this.dataCallback
                    );
                }
              },
              {
                key: "componentWillUnmount",
                value: function() {
                  console.log("NF Info Unmount: " + this.props.nfLabel),
                    S(
                      "NF ".concat(this.props.nfLabel.split(" - ")[1]),
                      this.dataCallback,
                      null
                    );
                }
              },
              {
                key: "render",
                value: function() {
                  var e = this.state,
                    t = e.serviceId,
                    n = e.instanceId,
                    o = e.core;
                  return a.createElement(
                    g.c,
                    { title: "NF Info" },
                    a.createElement(
                      g.g,
                      { cards: !0 },
                      a.createElement(
                        g.g.Row,
                        null,
                        a.createElement(g.g.Col, null, "Service ID"),
                        a.createElement(
                          g.g.Col,
                          { alignContent: "right" },
                          a.createElement(g.a, { color: "default" }, t)
                        )
                      ),
                      a.createElement(
                        g.g.Row,
                        null,
                        a.createElement(g.g.Col, null, "Instance ID"),
                        a.createElement(
                          g.g.Col,
                          { alignContent: "right" },
                          a.createElement(g.a, { color: "default" }, n)
                        )
                      ),
                      a.createElement(
                        g.g.Row,
                        null,
                        a.createElement(g.g.Col, null, "Core"),
                        a.createElement(
                          g.g.Col,
                          { alignContent: "right" },
                          a.createElement(g.a, { color: "default" }, o)
                        )
                      )
                    )
                  );
                }
              }
            ]),
            t
          );
        })(a.PureComponent),
        G = (function(e) {
          function t() {
            var e, n;
            Object(p.a)(this, t);
            for (var a = arguments.length, o = new Array(a), r = 0; r < a; r++)
              o[r] = arguments[r];
            return (
              ((n = Object(m.a)(
                this,
                (e = Object(h.a)(t)).call.apply(e, [this].concat(o))
              )).state = {
                nfLabel: n.props.match.params.nfLabel,
                eventList: []
              }),
              (n.eventHandler = function(e) {
                var t = parseInt(n.state.nfLabel.split(" - ")[1]),
                  a = e.source;
                a.type &&
                  "MGR" !== a.type &&
                  e.source.instance_id === t &&
                  n.setState(function(t) {
                    return { eventList: Object(E.a)(t.eventList).concat([e]) };
                  });
              }),
              n
            );
          }
          return (
            Object(d.a)(t, e),
            Object(f.a)(t, [
              {
                key: "componentDidMount",
                value: function() {
                  k(
                    "SINGLE NF PAGE ".concat(this.state.nfLabel),
                    this.eventHandler,
                    !0
                  );
                }
              },
              {
                key: "componentWillUnmount",
                value: function() {
                  w("SINGLE NF PAGE ".concat(this.state.nfLabel));
                }
              },
              {
                key: "render",
                value: function() {
                  return a.createElement(
                    g.e.Content,
                    null,
                    a.createElement(
                      g.d.Row,
                      null,
                      a.createElement(
                        g.d.Col,
                        { md: 8, xl: 8 },
                        a.createElement(M, {
                          nfLabel: this.state.nfLabel,
                          showMoreInfoButton: !1,
                          extraContent: a.createElement(T, {
                            events: this.state.eventList
                          })
                        })
                      ),
                      a.createElement(
                        g.d.Col,
                        { sm: 4, lg: 4 },
                        a.createElement(B, { nfLabel: this.state.nfLabel })
                      )
                    )
                  );
                }
              }
            ]),
            t
          );
        })(a.PureComponent);
      var U = function(e) {
        return a.createElement(
          g.c,
          null,
          a.createElement(
            g.c.Header,
            null,
            a.createElement(g.c.Title, null, "Core ", e.coreLabel)
          ),
          a.createElement(g.c.Body, null, e.extraContent)
        );
      };
      var W = function(e) {
          return a.createElement(
            g.g,
            {
              cards: !0,
              striped: !0,
              responsive: !0,
              className: "table-vcenter"
            },
            a.createElement(
              g.g.Body,
              null,
              e.sources.map(function(t) {
                return t.type
                  ? a.createElement(
                      g.g.Row,
                      null,
                      a.createElement(
                        g.g.Col,
                        null,
                        "".concat(t.type, " - ").concat(t.instance_id)
                      ),
                      a.createElement(
                        g.b,
                        {
                          RootComponent: "a",
                          color: "secondary",
                          size: "sm",
                          className: "ml-2",
                          onClick: function() {
                            var n = e.history;
                            n
                              ? n.push(
                                  "/nfs/"
                                    .concat(t.type, " - ")
                                    .concat(t.instance_id)
                                )
                              : console.error("Failed to go to single NF page");
                          }
                        },
                        "View More Info"
                      )
                    )
                  : a.createElement(
                      g.g.Row,
                      null,
                      a.createElement(
                        g.g.Col,
                        null,
                        "".concat(t.msg.split(" ")[0], " Thread")
                      )
                    );
              })
            )
          );
        },
        X = (function(e) {
          function t() {
            var e, n;
            Object(p.a)(this, t);
            for (var a = arguments.length, o = new Array(a), r = 0; r < a; r++)
              o[r] = arguments[r];
            return (
              ((n = Object(m.a)(
                this,
                (e = Object(h.a)(t)).call.apply(e, [this].concat(o))
              )).state = { coreList: {} }),
              (n.eventHandler = function(e) {
                var t = e.source;
                ("NF Ready" === e.message || e.message.includes("Start")) &&
                  n.setState(function(n) {
                    var a = Object(R.a)({}, n.coreList),
                      o = t.core;
                    if (((t.msg = e.message), o in a && a[o].length)) {
                      var r = t.instance_id;
                      r && r !== a[o][0].instance_id && a[o].unshift(t);
                    } else a[o] = [t];
                    return { coreList: a };
                  }),
                  ("NF Stopping" === e.message || e.message.includes("End")) &&
                    n.setState(function(t) {
                      var n,
                        a = Object(R.a)({}, t.coreList),
                        o = e.source.instance_id,
                        r = !1;
                      for (n in a)
                        if (
                          a.hasOwnProperty(n) &&
                          ((a[n] = a[n].filter(function(e) {
                            return !(r = e.instance_id === o);
                          })),
                          r)
                        )
                          break;
                      return (
                        r || console.log("Nothing removed from list!"),
                        0 === a[n].length && delete a[n],
                        { coreList: a }
                      );
                    });
              }),
              n
            );
          }
          return (
            Object(d.a)(t, e),
            Object(f.a)(t, [
              {
                key: "componentDidMount",
                value: function() {
                  k("CORE MAPPINGS PAGE", this.eventHandler, !0);
                }
              },
              {
                key: "componentWillUnmount",
                value: function() {
                  w("CORE MAPPINGS PAGE");
                }
              },
              {
                key: "render",
                value: function() {
                  var e = this.props.history,
                    t = this.state.coreList;
                  return a.createElement(
                    g.e.Content,
                    null,
                    a.createElement(
                      g.d.Row,
                      null,
                      Object.keys(t).map(function(n) {
                        var o = t[n];
                        return o && o.length > 0
                          ? a.createElement(
                              g.d.Col,
                              { md: 6, xl: 4, key: o[0].core },
                              a.createElement(U, {
                                coreLabel: o[0].core,
                                extraContent: a.createElement(W, {
                                  history: e,
                                  sources: o
                                })
                              })
                            )
                          : "";
                      }),
                      (function(e) {
                        for (var t in e) if (e.hasOwnProperty(t)) return !1;
                        return !0;
                      })(t) && "No Cores have Running NFs!"
                    )
                  );
                }
              }
            ]),
            t
          );
        })(a.PureComponent);
      var z = function(e) {
          return "Error404";
        },
        V = window.location.hostname,
        q = (function(e) {
          function t(e) {
            var n;
            return (
              Object(p.a)(this, t),
              ((n = Object(m.a)(
                this,
                Object(h.a)(t).call(this)
              )).state = Object(R.a)({}, e)),
              n
            );
          }
          return (
            Object(d.a)(t, e),
            Object(f.a)(t, [
              {
                key: "componentWillMount",
                value: function() {
                  window.open("http://".concat(V, ":3000"));
                }
              },
              {
                key: "render",
                value: function() {
                  return a.createElement("section", null, "...");
                }
              }
            ]),
            t
          );
        })(a.Component),
        J = n(18),
        Y = n.n(J),
        $ = window.location.hostname,
        K = (function(e) {
          function t() {
            var e, n;
            Object(p.a)(this, t);
            for (var a = arguments.length, o = new Array(a), r = 0; r < a; r++)
              o[r] = arguments[r];
            return (
              ((n = Object(m.a)(
                this,
                (e = Object(h.a)(t)).call.apply(e, [this].concat(o))
              )).state = { selectedFile: null }),
              (n.onFileChange = function(e) {
                n.setState({ selectedFile: e.target.files[0], launch: 0 });
              }),
              (n.OnStopHandler = function(e) {
                (n.state = { request_type: "stop" }),
                  Y.a
                    .post("http://".concat($, ":8000"), n.state)
                    .then(function(e) {
                      console.log(e),
                        alert(
                          "Post request succeeded. Status: " + e.statusText
                        );
                    })
                    .catch(function(e) {
                      console.log(e), alert(e);
                    }),
                  n.setState({ launch: 0 });
              }),
              (n.onFileUpload = function() {
                var e = new FormData();
                e.append(
                  "configFile",
                  n.state.selectedFile,
                  n.state.selectedFile.name
                );
                console.log(n.state.selectedFile),
                  Y.a
                    .post("http://".concat($, ":8000"), e, {
                      headers: { "Content-Type": "multipart/form-data" }
                    })
                    .then(function(e) {
                      console.log(e),
                        alert(
                          "Post request succeeded. Status: " + e.statusText
                        );
                    })
                    .catch(function(e) {
                      console.log(e), alert(e);
                    });
              }),
              (n.onLaunchChain = function() {
                (n.state = { request_type: "start" }),
                  Y.a
                    .post("http://".concat($, ":8000"), n.state)
                    .then(function(e) {
                      console.log(e),
                        alert(
                          "Post request succeeded. Status: " + e.statusText
                        );
                    })
                    .catch(function(e) {
                      console.log(e), alert(e);
                    }),
                  n.setState({ launch: 1 });
              }),
              n
            );
          }
          return (
            Object(d.a)(t, e),
            Object(f.a)(t, [
              {
                key: "render",
                value: function() {
                  return o.a.createElement(
                    "div",
                    { style: { marginLeft: "50px" } },
                    o.a.createElement("br", null),
                    o.a.createElement(
                      "h1",
                      null,
                      "Network Function Chain Deployment"
                    ),
                    o.a.createElement(
                      "h4",
                      null,
                      "Upload a JSON Configuration File to Launch a Chain of NFs"
                    ),
                    o.a.createElement(
                      "p",
                      null,
                      "Ensure your ONVM manager is running before uploading and launching your chain of NFs. Navigate to the various dashboards on ONVM web to observe NF behavior.",
                      o.a.createElement("br", null),
                      "Output from each NF will be written to the directory specified in your file or to a default timestamped directory."
                    ),
                    o.a.createElement(
                      "p",
                      null,
                      "Follow the",
                      " ",
                      o.a.createElement(
                        "a",
                        {
                          href:
                            "https://github.com/catherinemeadows/openNetVM/blob/configScript/docs/NF_Dev.md"
                        },
                        "documentation"
                      ),
                      " ",
                      "in the ONVM repository to learn more about proper formatting for your config file. See an",
                      " ",
                      o.a.createElement(
                        "a",
                        {
                          href:
                            "https://github.com/catherinemeadows/openNetVM/blob/configScript/examples/example_chain.json"
                        },
                        "example"
                      ),
                      " ",
                      "config file."
                    ),
                    o.a.createElement(
                      "div",
                      null,
                      o.a.createElement("input", {
                        type: "file",
                        onChange: this.onFileChange
                      }),
                      o.a.createElement(
                        "button",
                        { onClick: this.onFileUpload },
                        "Upload"
                      ),
                      o.a.createElement("br", null),
                      o.a.createElement("br", null),
                      o.a.createElement(
                        "button",
                        {
                          onClick: this.onLaunchChain,
                          style: {
                            backgroundColor: "#48cf7c",
                            borderRadius: "4px",
                            border: "none",
                            padding: "14px 28px"
                          }
                        },
                        "Launch NF Chain"
                      ),
                      o.a.createElement(
                        "button",
                        {
                          onClick: this.OnStopHandler,
                          style: {
                            margin: "5px",
                            backgroundColor: "#db4d5b",
                            borderRadius: "4px",
                            border: "none",
                            padding: "14px 28px"
                          }
                        },
                        "Terminate"
                      )
                    )
                  );
                }
              }
            ]),
            t
          );
        })(a.Component);
      n(64), n(66);
      var Q = function(e) {
        return a.createElement(
          c.a,
          null,
          a.createElement(
            P,
            null,
            a.createElement(
              c.a,
              null,
              a.createElement(
                i.a,
                null,
                a.createElement(s.a, {
                  exact: !0,
                  path: "/",
                  render: function() {
                    return a.createElement(u.a, { to: { pathname: "/nfs" } });
                  }
                }),
                a.createElement(s.a, { exact: !0, path: "/nfs", component: I }),
                a.createElement(s.a, {
                  exact: !0,
                  path: "/nfs/:nfLabel",
                  component: G
                }),
                a.createElement(s.a, {
                  exact: !0,
                  path: "/ports",
                  component: H
                }),
                a.createElement(s.a, {
                  exact: !0,
                  path: "/core-mappings",
                  component: X
                }),
                a.createElement(s.a, {
                  exact: !0,
                  path: "/grafana",
                  component: q
                }),
                a.createElement(s.a, {
                  exact: !0,
                  path: "/nf-chain",
                  component: K
                }),
                a.createElement(s.a, { component: z })
              )
            )
          )
        );
      };
      Boolean(
        "localhost" === window.location.hostname ||
          "[::1]" === window.location.hostname ||
          window.location.hostname.match(
            /^127(?:\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)){3}$/
          )
      );
      l.a.render(
        o.a.createElement(o.a.StrictMode, null, o.a.createElement(Q, null)),
        document.getElementById("root")
      ),
        "serviceWorker" in navigator &&
          navigator.serviceWorker.ready.then(function(e) {
            e.unregister();
          });
    }
  },
  [[33, 2, 1]]
]);
