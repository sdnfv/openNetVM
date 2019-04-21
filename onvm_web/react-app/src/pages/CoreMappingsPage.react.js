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

    if (event.message === "NF Ready" || event.message.includes("Start")) {
      this.setState(prevState => {
        var cores = { ...prevState.coreList };
        var core = source.core;
        source.msg = event.message;

        if (core in cores && cores[core].length){
          /* we already have the core */
          var id = source.instance_id;
          if(id && id !== cores[core][0].instance_id)
            /* make sure we don't have nf in list */
            cores[core].unshift(source);
        } else
          /* create new map entry for core */ 
          cores[core] = [source];

        return { coreList: cores };
      });
    }

    if (event.message === "NF Stopping" || event.message.includes("End")) {
      this.setState(prevState => {
        var cores = { ...prevState.coreList };
        var core, instance_id = event.source.instance_id;
        var done = false;

        function is_instance(item){
          done = (item.instance_id === instance_id);
          return !done;
        }

        /* Bad performance O(cores*coreLength), but can be optimized with core_num later */
        for (core in cores) {
          if (cores.hasOwnProperty(core)) {
            cores[core] = cores[core].filter(i => is_instance(i));
            if (done)
              break;
          }
        }

        if (!done)
          console.log("Nothing removed from list!");

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
