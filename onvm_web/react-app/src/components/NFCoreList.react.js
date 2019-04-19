// @flow

import * as React from "react";

import { Table, Button } from "tabler-react";
import type { OnvmEvent } from "../pubsub";

type Props = {|
  +sources: Array<OnvmEvent>
|};

function NFCoreList(props: Props): React.Node {
  return (
    <Table
      cards={true}
      striped={true}
      responsive={true}
      className="table-vcenter"
    >
      <Table.Body>
        {props.sources.map(source => {
          if(!source.type)
            return ( <Table.Row>
              <Table.Col>{`${source.msg.split(" ")[0]} Thread`}</Table.Col>
            </Table.Row> );

          return <Table.Row>
              <Table.Col>{`${source.type} - ${source.instance_id}`}</Table.Col>
              <Button
                  RootComponent="a"
                  color="secondary"
                  size="sm"
                  className="ml-2"
                  onClick={() => {
                    const history = props.history;
                    if (history)
                      history.push(`/nfs/${source.type} - ${source.instance_id}`);
                    else
                      console.error("Failed to go to single NF page");
                  }}
                >
                  View More Info
              </Button>
            </Table.Row>;
        })}
      </Table.Body>
    </Table>
  );
}

export default NFCoreList;
