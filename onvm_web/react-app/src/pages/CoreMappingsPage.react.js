// @flow

import * as React from "react";

import { Page, Grid } from "tabler-react";

import { registerEventSubscriber, unregisterEventSubscriber } from "../pubsub";

type Props = {||};

type State = {|
  coreMappings: Array<Object>
|};

class CoreMappingsPage extends React.PureComponent<Props, State> {
  state = {
    coreMappings: []
  };

  eventHandler = (event: OnvmEvent): void => {
    // TODO: implement when shared CPU is completed
  };

  componentDidMount(): void {
    registerEventSubscriber("CORE MAPPINGS PAGE", this.eventHandler, true);
  }

  componentWillUnmount(): void {
    unregisterEventSubscriber("CORE MAPPINGS PAGE");
  }

  render(): React.Node {
    const { coreMappings } = this.state;
    return (
      <Page.Content>
        <Grid.Row>
          {coreMappings.length === 0 &&
            "Core Mapping Info coming soon, once shared CPU implementation is completed."}
        </Grid.Row>
      </Page.Content>
    );
  }
}

export default CoreMappingsPage;
