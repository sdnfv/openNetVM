// @flow

import * as React from "react";

import { Table, Button } from "tabler-react";
import type { OnvmEvent } from "../pubsub";

type Props = {|
  +sources: Array<OnvmEvent>
|};

function NFCoreList(props: Props): React.Node {
  var colName = "";
  return (
    <Table
      cards={true}
      striped={true}
      responsive={true}
      className="table-vcenter"
    >
      <Table.Body>
        {props.sources.map(source => {
          if(source.type === "NF")
            colName = `NF ${source.instance_id}`;
          else 
            colName = "Manager";
          return <Table.Row>
            <Table.Col>{colName}</Table.Col>
            <Button
                RootComponent="a"
                color="secondary"
                size="sm"
                className="ml-2"
                onClick={() => {
                  const history = props.history;
                  if (history)
                    history.push(`/nfs/NF ${source.instance_id}`);
                  else
                    console.error("Failed to go to single NF page");
                }}
              >
                View More Info
            </Button>
          </Table.Row>
        })}
      </Table.Body>
    </Table>
  );
}

export default NFCoreList;
