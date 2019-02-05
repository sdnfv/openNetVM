// @flow

import * as React from "react";
import { Page, Grid } from "tabler-react";

import NFGraph from "../components/NFGraph.react";

import { registerEventSubscriber, unregisterEventSubscriber } from "../pubsub";

import type { OnvmEvent } from "../pubsub";

type Props = {|
  +history: Object // passed in from React Router
|};

type State = {|
  nfLabelList: Array<string>
|};

class NFDashboardPage extends React.PureComponent<Props, State> {
  state = {
    nfLabelList: []
  };

  eventHandler = (event: OnvmEvent): void => {
    if (event.message === "NF Ready") {
      const nfLabel = `NF ${event.source.instance_id}`;
      this.setState(prevState => {
        return { nfLabelList: [nfLabel, ...prevState.nfLabelList] };
      });
    }
    if (event.message === "NF Stopping") {
      const nfLabel = `NF ${event.source.instance_id}`;

      this.setState(prevState => {
        const arr = [
          ...prevState.nfLabelList.filter(label => label !== nfLabel)
        ];
        return { nfLabelList: arr };
      });
    }
  };

  componentDidMount(): void {
    registerEventSubscriber("NF DASHBOARD PAGE", this.eventHandler, true);
  }

  componentWillUnmount(): void {
    unregisterEventSubscriber("NF DASHBOARD PAGE");
  }

  render(): React.Node {
    const { history } = this.props;
    const { nfLabelList } = this.state;
    return (
      <Page.Content>
        <Grid.Row>
          {nfLabelList.map(label => (
            <Grid.Col md={6} xl={4} key={label}>
              <NFGraph nfLabel={label} history={history} />
            </Grid.Col>
          ))}
          {nfLabelList.length === 0 && "No Running NFS to Display!"}
        </Grid.Row>
      </Page.Content>
    );
  }
}

export default NFDashboardPage;
