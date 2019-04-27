// @flow

import * as React from "react";

import { Card, Table, Badge } from "tabler-react";

import { registerNFSubscriber, unregisterNFSubscriber } from "../pubsub";

import type { OnvmNfData } from "../pubsub";

type Props = {|
  nfLabel: string
|};

type State = {|
  instanceId: ?number,
  serviceId: ?number,
  core: ?number
|};

class NFInfo extends React.PureComponent<Props, State> {
  state = {
    instanceId: null,
    serviceId: null,
    core: null
  };

  dataCallback = (data: OnvmNfData, _: number): void => {
    this.setState({
      instanceId: data.instance_id,
      serviceId: data.service_id,
      core: data.core
    });
  };

  componentDidMount(): void {
    console.log("NF Info Mount: " + this.props.nfLabel);
    registerNFSubscriber(`NF ${this.props.nfLabel.split(" - ")[1]}`, this.dataCallback);
  }

  componentWillUnmount(): void {
    console.log("NF Info Unmount: " + this.props.nfLabel);
    unregisterNFSubscriber(`NF ${this.props.nfLabel.split(" - ")[1]}`, this.dataCallback, null);
  }

  render(): React.Node {
    const { serviceId, instanceId, core } = this.state;
    return (
      <Card title="NF Info">
        <Table cards>
          <Table.Row>
            <Table.Col>Service ID</Table.Col>
            <Table.Col alignContent="right">
              <Badge color="default">{serviceId}</Badge>
            </Table.Col>
          </Table.Row>

          <Table.Row>
            <Table.Col>Instance ID</Table.Col>
            <Table.Col alignContent="right">
              <Badge color="default">{instanceId}</Badge>
            </Table.Col>
          </Table.Row>

          <Table.Row>
            <Table.Col>Core</Table.Col>
            <Table.Col alignContent="right">
              <Badge color="default">{core}</Badge>
            </Table.Col>
          </Table.Row>
        </Table>
      </Card>
    );
  }
}

export default NFInfo;
