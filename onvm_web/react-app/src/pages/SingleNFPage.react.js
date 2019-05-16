// @flow

import * as React from "react";

import { Page, Grid } from "tabler-react";
import NFGraph from "../components/NFGraph.react";
import NFEventList from "../components/NFEventList.react";
import NFInfo from "../components/NFInfo.react";

import { registerEventSubscriber, unregisterEventSubscriber } from "../pubsub";

import type { OnvmEvent } from "../pubsub";

type Props = {|
  +match: Object
|};

type State = {|
  nfLabel: string,
  eventList: Array<OnvmEvent>
|};

class SingleNFPage extends React.PureComponent<Props, State> {
  state = {
    nfLabel: this.props.match.params.nfLabel,
    eventList: []
  };

  eventHandler = (event: OnvmEvent): void => {
    const instanceID = parseInt(this.state.nfLabel.split(" - ")[1]);
    const source = event.source;
    if (source.type && source.type !== "MGR" && event.source.instance_id === instanceID) {
      this.setState(prevState => {
        return { eventList: [...prevState.eventList, event] };
      });
    }
  };

  componentDidMount(): void {
    registerEventSubscriber(
      `SINGLE NF PAGE ${this.state.nfLabel}`,
      this.eventHandler,
      true
    );
  }

  componentWillUnmount(): void {
    unregisterEventSubscriber(`SINGLE NF PAGE ${this.state.nfLabel}`);
  }

  render(): React.Node {
    return (
      <Page.Content>
        <Grid.Row>
          <Grid.Col md={8} xl={8}>
            <NFGraph
              nfLabel={this.state.nfLabel}
              showMoreInfoButton={false}
              extraContent={<NFEventList events={this.state.eventList} />}
            />
          </Grid.Col>
          <Grid.Col sm={4} lg={4}>
            <NFInfo nfLabel={this.state.nfLabel} />
          </Grid.Col>
        </Grid.Row>
      </Page.Content>
    );
  }
}

export default SingleNFPage;
