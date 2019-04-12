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
  coreList: Object
|};

class CoreMappingsPage extends React.PureComponent<Props, State> {
  state = {
    coreList: {}
  };

  eventHandler = (event: OnvmEvent): void => {
    const source = event.source;

    if (event.message === "NF Ready") {
      this.setState(prevState => {
        var cores = { ...prevState.coreList };
        var core = source.core;

        if (core in cores && cores[core].length)
          /* we already have the core */
          cores[core].push(source);
        /* create new map entry for core */ else cores[core] = [source];

        return { coreList: cores };
      });
    }

    if (event.message === "NF Stopping") {
      this.setState(prevState => {
        var cores = { ...prevState.coreList };
        var core = source.core;

        if (!(core in cores)) {
          console.error("Error with coreMap");
          return { coreList: cores };
        }

        cores[core] = cores[core].filter(
          i => i.instance_id !== source.instance_id
        );

        if (cores[core].length === 0)
          /* nothing running on core */
          delete cores[core];

        return { coreList: cores };
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

    function isEmpty(obj) {
      for (var key in obj) {
        if (obj.hasOwnProperty(key)) return false;
      }
      return true;
    }

    return (
      <Page.Content>
        <Grid.Row>
          {Object.keys(coreList).map(key => {
            var core = coreList[key];
            if (core && core.length > 0)
              return (
                <Grid.Col md={6} xl={4} key={core[0].core}>
                  <CoreCard
                    coreLabel={core[0].core}
                    extraContent={
                      <NFCoreList history={history} sources={core} />
                    }
                  />
                </Grid.Col>
              );
            else return "";
          })}
          {isEmpty(coreList) && "No Cores have Running NFs!"}
        </Grid.Row>
      </Page.Content>
    );
  }
}

export default CoreMappingsPage;
