// @flow

import type { OnvmNfData, ColumnRestoreData } from "../pubsub";

import * as React from "react";

import { Card, Button } from "tabler-react";
import C3Chart from "react-c3js";

import { registerNFSubscriber, unregisterNFSubscriber } from "../pubsub";

type Props = {|
  nfLabel: string,
  showMoreInfoButton?: boolean,
  history?: Object, // react router history object
  extraContent?: React.Node
|};

type State = {|
  graphData: {
    columns: ColumnRestoreData
  }
|};

const axisData = {
  x: {
    label: {
      text: "X Axis",
      position: "outer-center"
    }
  },
  y: {
    label: {
      text: "Y Axis",
      position: "outer-middle"
    }
  }
};

class NFGraph extends React.PureComponent<Props, State> {
  state = {
    graphData: {
      xs: {
        tx_pps: `${this.props.nfLabel}x1`,
        rx_pps: `${this.props.nfLabel}x1`
      },
      names: {
        tx_pps: "TX PPS",
        rx_pps: "RX PPS"
      },
      empty: {
        label: {
          text: "No Data to Display"
        }
      },
      columns: [[`${this.props.nfLabel}x1`], ["tx_pps"], ["rx_pps"]]
    }
  };

  dataCallback = (data: OnvmNfData, counter: number): void => {
    const arrMaxSize = 40;
    const graphDataCopy = { ...this.state.graphData };
    const { columns } = graphDataCopy;

    columns[0].push(counter);
    columns[0] = this.trimToSize(columns[0], arrMaxSize);

    columns[1].push(data.TX);
    columns[1] = this.trimToSize(columns[1], arrMaxSize);

    columns[2].push(data.RX);
    columns[2] = this.trimToSize(columns[2], arrMaxSize);

    this.setState({ graphData: graphDataCopy });
  };

  trimToSize = (arr: Array<*>, size: number): Array<*> => {
    if (arr.length < size) return arr;
    const firstElem = arr[0];
    arr = arr.slice(2);
    while (arr.length > size) {
      arr = arr.slice(1);
    }
    return [firstElem, ...arr];
  };

  componentDidMount(): void {
    console.log("Graph Mount: " + this.props.nfLabel);
    var nfLabel = `NF ${this.props.nfLabel.split(" - ")[1]}`;
    if(this.props.nfLabel.substring(0, 4) === "Port")
      nfLabel = this.props.nfLabel;
    const columnStateToRestore = registerNFSubscriber(
      nfLabel,
      this.dataCallback
    );
    if (columnStateToRestore) {
      console.log("Graph Restore: " + this.props.nfLabel);
      const graphDataCopy = { ...this.state.graphData };
      graphDataCopy.columns = columnStateToRestore;
      this.setState({ graphData: graphDataCopy });
    }
  }

  componentWillUnmount(): void {
    console.log("Graph Unmount: " + this.props.nfLabel);
    var nfLabel = `NF ${this.props.nfLabel.split(" - ")[1]}`;
    if(this.props.nfLabel.substring(0, 4) === "Port")
      nfLabel = this.props.nfLabel;
    unregisterNFSubscriber(
      nfLabel,
      this.dataCallback,
      this.state.graphData.columns
    );
  }

  render(): React.Node {
    const showMoreInfoButton =
      this.props.showMoreInfoButton === null ||
      this.props.showMoreInfoButton === undefined
        ? true
        : this.props.showMoreInfoButton;
    const { extraContent } = this.props;
    return (
      <Card>
        <Card.Header>
          <Card.Title>{this.props.nfLabel}</Card.Title>
          {showMoreInfoButton && (
            <Card.Options>
              <Button
                RootComponent="a"
                color="secondary"
                size="sm"
                className="ml-2"
                onClick={() => {
                  const { history } = this.props;
                  if (history) {
                    history.push(`/nfs/${this.props.nfLabel}`);
                  } else {
                    console.error("Failed to go to single NF page");
                  }
                }}
              >
                View More Info
              </Button>
            </Card.Options>
          )}
        </Card.Header>
        <Card.Body>
          <C3Chart
            data={this.state.graphData}
            axis={axisData}
            legend={{
              show: true
            }}
            padding={{
              bottom: 0,
              top: 0
            }}
          />
          {extraContent}
        </Card.Body>
      </Card>
    );
  }
}

export default NFGraph;
