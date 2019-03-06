// @flow

import * as React from "react";
import { NavLink, withRouter } from "react-router-dom";
import { Site } from "tabler-react";

import { handleNFData, handleEventData } from "./pubsub";

type Props = {|
  +children: React.Node
|};

type State = {|
  interval: ?IntervalID
|};

class AppWrapper extends React.PureComponent<Props, State> {
  state = {
    interval: null
  };

  dataFetcher(): void {
    const hostName = window.location.hostname;
    fetch(`http://${hostName}:8000/onvm_json_stats.json`)
      .then(res => res.json())
      .then(res => handleNFData(res))
      .catch(error => console.error(error));

    fetch(`http://${hostName}:8000/onvm_json_events.json`)
      .then(res => res.json())
      .then(res => handleEventData(res))
      .catch(error => console.error(error));
  }

  componentDidMount(): void {
    const interval = setInterval(this.dataFetcher, 3000);
    this.setState({ interval: interval });
  }

  componentWillUnmount(): void {
    const { interval } = this.state;
    if (interval) {
      clearInterval(interval);
    }
  }

  render(): React.Node {
    const { children } = this.props;

    const headerProps = {
      href: "/",
      alt: "openNetVM",
      imageURL: "/onvm-logo.png"
    };

    const navProps = {
      itemsObjects: [
        {
          value: "NF Dashboard",
          to: "/nfs",
          icon: "home",
          LinkComponent: withRouter(NavLink)
        },
        {
          value: "Port Dashboard",
          to: "/ports",
          icon: "server",
          LinkComponent: withRouter(NavLink)
        },
        {
          value: "Core Mappings",
          to: "/core-mappings",
          icon: "cpu",
          LinkComponent: withRouter(NavLink)
        }
      ]
    };

    return (
      <Site.Wrapper
        headerProps={headerProps}
        navProps={navProps}
        footerProps={{}}
      >
        {children}
      </Site.Wrapper>
    );
  }
}

export default AppWrapper;
