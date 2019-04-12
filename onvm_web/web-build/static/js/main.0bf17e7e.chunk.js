(window.webpackJsonp = window.webpackJsonp || []).push([
  [0],
  {
    22: function(e, t, n) {
      e.exports = n(39);
    },
    27: function(e, t, n) {},
    37: function(e, t, n) {},
    39: function(e, t, n) {
      "use strict";
      n.r(t);
      var a = n(0),
        o = n.n(a),
        r = n(16),
        l = n.n(r),
        c = (n(27), n(41)),
        i = n(45),
        s = n(32),
        u = n(42),
        p = n(4),
        f = n(5),
        m = n(7),
        h = n(6),
        v = n(8),
        b = n(43),
        d = n(44),
        g = n(2),
        E = n(10),
        y = {},
        L = {},
        O = [],
        j = 0,
        C = null,
        k = -1;
      function N(e, t, n) {
        var a = {
          name: e,
          callback: t,
          includePastEvents: n,
          hasBeenCalled: !1
        };
        if (n && C) {
          var o = C;
          console.log("Catching event subscriber up to all past events.");
          for (var r = 0; r !== k + 1; ) a.callback(o[r]), ++r;
          a.hasBeenCalled = !0;
        }
        O.push(a);
      }
      function w(e) {
        O = O.filter(function(t) {
          return t.name !== e;
        });
      }
      function P(e, t) {
        if (e.length < t) return e;
        var n = e[0];
        for (e = e.slice(2); e.length > t; ) e = e.slice(1);
        return [n].concat(Object(E.a)(e));
      }
      function R(e, t) {
        if ((e in L ? L[e].push(t) : (L[e] = [t]), e in y)) return y[e];
      }
      function x(e, t, n) {
        e in L &&
          (0 ===
            L[e].filter(function(e) {
              return e !== t;
            }).length && delete L[e],
          null != n && (y[e] = n));
      }
      var F = (function(e) {
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
            Object(v.a)(t, e),
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
                            if (e in L)
                              L[e].forEach(function(n) {
                                return n(t[e], j);
                              });
                            else if (
                              (console.log(
                                "No NF Subscriber callback for label: " + e
                              ),
                              e in y)
                            ) {
                              var n = t[e],
                                a = y[e];
                              a[0].push(j),
                                (a[0] = P(a[0], 40)),
                                a[1].push(n.TX),
                                (a[1] = P(a[1], 40)),
                                a[2].push(n.RX),
                                (a[2] = P(a[2], 40));
                            } else
                              console.error(
                                "No NF Restore state for label: " + e
                              );
                          };
                        for (var a in t) n(a);
                        var o = e.onvm_port_stats,
                          r = function(e) {
                            if (e in L)
                              L[e].forEach(function(t) {
                                return t(o[e], j);
                              });
                            else if (
                              (console.log(
                                "No NF Subscriber callback for label: " + e
                              ),
                              e in y)
                            ) {
                              var t = o[e],
                                n = y[e];
                              n[0].push(j),
                                (n[0] = P(n[0], 40)),
                                n[1].push(t.TX),
                                (n[1] = P(n[1], 40)),
                                n[2].push(t.RX),
                                (n[2] = P(n[2], 40));
                            } else
                              console.error(
                                "No NF Restore state for label: " + e
                              );
                          };
                        for (var l in o) r(l);
                        ++j;
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
                              var t = e[++k];
                              t &&
                                O.forEach(function(e) {
                                  return e.callback(t);
                                });
                            };
                            k < e.length - 1;

                          )
                            t();
                          C = e;
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
                          LinkComponent: Object(b.a)(d.a)
                        },
                        {
                          value: "Port Dashboard",
                          to: "/ports",
                          icon: "server",
                          LinkComponent: Object(b.a)(d.a)
                        },
                        {
                          value: "Core Mappings",
                          to: "/core-mappings",
                          icon: "cpu",
                          LinkComponent: Object(b.a)(d.a)
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
        S = n(12),
        D = n(20),
        A = n.n(D),
        I = {
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
                var a = Object(S.a)({}, n.state.graphData),
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
            Object(v.a)(t, e),
            Object(f.a)(t, [
              {
                key: "componentDidMount",
                value: function() {
                  console.log("Graph Mount: " + this.props.nfLabel);
                  var e = R(this.props.nfLabel, this.dataCallback);
                  if (e) {
                    console.log("Graph Restore: " + this.props.nfLabel);
                    var t = Object(S.a)({}, this.state.graphData);
                    (t.columns = e), this.setState({ graphData: t });
                  }
                }
              },
              {
                key: "componentWillUnmount",
                value: function() {
                  console.log("Graph Unmount: " + this.props.nfLabel),
                    x(
                      this.props.nfLabel,
                      this.dataCallback,
                      this.state.graphData.columns
                    );
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
                      a.createElement(A.a, {
                        data: this.state.graphData,
                        axis: I,
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
        _ = (function(e) {
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
                if ("NF Ready" === e.message) {
                  var t = "NF ".concat(e.source.instance_id);
                  n.setState(function(e) {
                    return {
                      nfLabelList: [t].concat(Object(E.a)(e.nfLabelList))
                    };
                  });
                }
                if ("NF Stopping" === e.message) {
                  var a = "NF ".concat(e.source.instance_id);
                  n.setState(function(e) {
                    return {
                      nfLabelList: Object(E.a)(
                        e.nfLabelList.filter(function(e) {
                          return e !== a;
                        })
                      )
                    };
                  });
                }
              }),
              n
            );
          }
          return (
            Object(v.a)(t, e),
            Object(f.a)(t, [
              {
                key: "componentDidMount",
                value: function() {
                  N("NF DASHBOARD PAGE", this.eventHandler, !0);
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
            Object(v.a)(t, e),
            Object(f.a)(t, [
              {
                key: "componentDidMount",
                value: function() {
                  N("PORT DASHBOARD PAGE", this.eventHandler, !0);
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
      var H = function(e) {
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
            Object(v.a)(t, e),
            Object(f.a)(t, [
              {
                key: "componentDidMount",
                value: function() {
                  console.log("NF Info Mount: " + this.props.nfLabel),
                    R(this.props.nfLabel, this.dataCallback);
                }
              },
              {
                key: "componentWillUnmount",
                value: function() {
                  console.log("NF Info Unmount: " + this.props.nfLabel),
                    x(this.props.nfLabel, this.dataCallback, null);
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
        T = (function(e) {
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
                var t = parseInt(n.state.nfLabel.replace("NF ", ""));
                console.log(e),
                  "NF" === e.source.type &&
                    e.source.instance_id === t &&
                    n.setState(function(t) {
                      return {
                        eventList: Object(E.a)(t.eventList).concat([e])
                      };
                    });
              }),
              n
            );
          }
          return (
            Object(v.a)(t, e),
            Object(f.a)(t, [
              {
                key: "componentDidMount",
                value: function() {
                  N(
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
                          extraContent: a.createElement(H, {
                            events: this.state.eventList
                          })
                        })
                      ),
                      a.createElement(
                        g.d.Col,
                        { sm: 4, lg: 4 },
                        a.createElement(G, { nfLabel: this.state.nfLabel })
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
          var t = "";
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
              e.sources.map(function(n) {
                return (
                  (t =
                    "NF" === n.type ? "NF ".concat(n.instance_id) : "Manager"),
                  a.createElement(
                    g.g.Row,
                    null,
                    a.createElement(g.g.Col, null, t),
                    a.createElement(
                      g.b,
                      {
                        RootComponent: "a",
                        color: "secondary",
                        size: "sm",
                        className: "ml-2",
                        onClick: function() {
                          var t = e.history;
                          t
                            ? t.push("/nfs/NF ".concat(n.instance_id))
                            : console.error("Failed to go to single NF page");
                        }
                      },
                      "View More Info"
                    )
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
                "NF Ready" === e.message &&
                  n.setState(function(e) {
                    var n = Object(S.a)({}, e.coreList),
                      a = t.core;
                    return (
                      a in n && n[a].length ? n[a].push(t) : (n[a] = [t]),
                      { coreList: n }
                    );
                  }),
                  "NF Stopping" === e.message &&
                    n.setState(function(e) {
                      var n = Object(S.a)({}, e.coreList),
                        a = t.core;
                      return a in n
                        ? ((n[a] = n[a].filter(function(e) {
                            return e.instance_id !== t.instance_id;
                          })),
                          0 == n[a].length && delete n[a],
                          { coreList: n })
                        : (console.error("Error with coreMap"),
                          { coreList: n });
                    });
              }),
              n
            );
          }
          return (
            Object(v.a)(t, e),
            Object(f.a)(t, [
              {
                key: "componentDidMount",
                value: function() {
                  N("CORE MAPPINGS PAGE", this.eventHandler, !0);
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
      };
      n(35), n(37);
      var V = function(e) {
        return a.createElement(
          c.a,
          null,
          a.createElement(
            F,
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
                a.createElement(s.a, { exact: !0, path: "/nfs", component: _ }),
                a.createElement(s.a, {
                  exact: !0,
                  path: "/nfs/:nfLabel",
                  component: T
                }),
                a.createElement(s.a, {
                  exact: !0,
                  path: "/ports",
                  component: B
                }),
                a.createElement(s.a, {
                  exact: !0,
                  path: "/core-mappings",
                  component: X
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
        o.a.createElement(o.a.StrictMode, null, o.a.createElement(V, null)),
        document.getElementById("root")
      ),
        "serviceWorker" in navigator &&
          navigator.serviceWorker.ready.then(function(e) {
            e.unregister();
          });
    }
  },
  [[22, 2, 1]]
]);
