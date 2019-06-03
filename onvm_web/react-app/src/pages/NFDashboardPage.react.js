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
    var nfLabel;
    console.log(event);
    if (event.message === "NF Ready") {
      nfLabel = ` - ${event.source.instance_id}`;
      
      if(event.source.type === "NF")
        nfLabel = 'NF' + nfLabel;
      else
        nfLabel = event.source.type + nfLabel;
      
      this.setState(prevState => {
        return { nfLabelList: [nfLabel, ...prevState.nfLabelList] };
      });
    }
    if (event.message === "NF Stopping") {
      nfLabel = ` - ${event.source.instance_id}`;

      if(event.source.type === "NF")
        nfLabel = 'NF' + nfLabel;
      else
        nfLabel = event.source.type + nfLabel;
      console.log(nfLabel);
      this.setState(prevState => {
        console.log(prevState.nfLabelList);
        const arr = [
          ...prevState.nfLabelList.filter(label => label.split(" - ")[1] !== nfLabel.split(" - ")[1])
        ];
        console.log("end: " + prevState.nfLabelList);
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
