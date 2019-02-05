// @flow

import * as React from "react";

import { Page, Grid } from "tabler-react";
import NFGraph from "../components/NFGraph.react";

import { registerEventSubscriber, unregisterEventSubscriber } from "../pubsub";

import type { OnvmEvent } from "../pubsub";

type Props = {|
  +history: Object
|};

type State = {|
  portList: Array<string>
|};

class PortDashboardPage extends React.PureComponent<Props, State> {
  state = {
    portList: []
  };

  eventHandler = (event: OnvmEvent): void => {
    if (
      event.message.includes("Port ") &&
      event.message.includes(" initialized")
    ) {
      const portLabel = event.message.replace(" initialized", "");
      this.setState(prevState => {
        return { portList: [portLabel, ...prevState.portList] };
      });
    }
  };

  componentDidMount(): void {
    registerEventSubscriber("PORT DASHBOARD PAGE", this.eventHandler, true);
  }

  componentWillUnmount(): void {
    unregisterEventSubscriber("PORT DASHBOARD PAGE");
  }

  render(): React.Node {
    const { portList } = this.state;
    const { history } = this.props;
    return (
      <Page.Content>
        <Grid.Row>
          {portList.map(port => (
            <Grid.Col md={6} xl={4} key={port}>
              <NFGraph
                nfLabel={port}
                history={history}
                showMoreInfoButton={false}
              />
            </Grid.Col>
          ))}
          {portList.length === 0 && "No ports in use!"}
        </Grid.Row>
      </Page.Content>
    );
  }
}

export default PortDashboardPage;
