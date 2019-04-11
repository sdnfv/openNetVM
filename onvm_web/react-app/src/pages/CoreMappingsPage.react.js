// @flow

import * as React from "react";

import { Page, Grid } from "tabler-react";
import CoreCard from "../components/CoreCard.react";
import NFCoreList from "../components/NFCoreList.react";

import { registerEventSubscriber, unregisterEventSubscriber } from "../pubsub";

import type { OnvmEvent } from "../pubsub";

type Props = {|
  +history: Object // passed in from React Router
|};

type State = {|
  coreList: Array<Array<Object>>,
|};

class CoreMappingsPage extends React.PureComponent<Props, State> {
  state = {
    coreList: []
  };

  coreMap = {

  };

  eventHandler = (event: OnvmEvent): void => {
    const source = event.source;

    if (event.message === "NF Ready") {
      this.setState(prevState => {
        var arr = [ ...prevState.coreList ];
        var mapNum = this.coreMap[source.core];

        if(source.core in this.coreMap && arr.length > 0){
          // we already have the core in the list
          arr[mapNum].push(source);
        } else{
          // put our core at end of coreList
          this.coreMap[source.core] = arr.length;
          arr.push([source]);
        }

        return { coreList: arr };
      });
    }
    
    if (event.message === "NF Stopping") {
      this.setState(prevState => {
        var arr = [ ...prevState.coreList ];
        var mapNum = this.coreMap[source.core];

        if(!(source.core in this.coreMap)){
          console.error("Error with coreMap");
          return { coreList: arr };
        }

        console.log("\n-----------------\n");
        console.log("MapNum: ");
        console.log(mapNum);
        console.log("Source: ");
        console.log(source);
        console.log("Array before: ");
        console.log(arr);

        arr[mapNum] = arr[mapNum].filter(elem => elem.instance_id !== source.instance_id);

        console.log("Arr now: ");
        console.log(arr);

        return { coreList: arr };
      });
    }
  };

  componentDidMount(): void {
    registerEventSubscriber("CORE MAPPINGS PAGE", this.eventHandler, true);
  }

  componentWillUnmount(): void {
    unregisterEventSubscriber("CORE MAPPINGS PAGE");
  }

  render(): React.Node {
    const { history } = this.props;
    const { coreList } = this.state;

    return (
      <Page.Content>
        <Grid.Row>
          {coreList.map((core, i) => {
            if(core.length > 0)
              return <Grid.Col md={6} xl={4} key={core[0].core}>
                <CoreCard
                    coreLabel={core[0].core}
                    extraContent={ <NFCoreList history={history} sources={core}/> }
                />
              </Grid.Col>
            else return '';
          })}
          {coreList.length === 0 && "No Cores have Running NFs!"}
        </Grid.Row>
      </Page.Content>
    );
  }
}

export default CoreMappingsPage;
