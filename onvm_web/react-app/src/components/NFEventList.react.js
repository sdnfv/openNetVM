// @flow

import * as React from "react";

import { Table } from "tabler-react";
import type { OnvmEvent } from "../pubsub";

type Props = {|
  +events: Array<OnvmEvent>
|};

function NFEventList(props: Props): React.Node {
  return (
    <Table
      cards={true}
      striped={true}
      responsive={true}
      className="table-vcenter"
    >
      <Table.Header>
        <Table.Row>
          <Table.ColHeader>Event</Table.ColHeader>
          <Table.ColHeader>Timestamp</Table.ColHeader>
        </Table.Row>
      </Table.Header>
      <Table.Body>
        {props.events.map(event => (
          <Table.Row>
            <Table.Col>{event.message}</Table.Col>
            <Table.Col>{event.timestamp}</Table.Col>
          </Table.Row>
        ))}
      </Table.Body>
    </Table>
  );
}

export default NFEventList;
