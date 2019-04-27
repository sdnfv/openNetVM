// @flow

import * as React from "react";

import { Card } from "tabler-react";

type Props = {|
  coreLabel: string,
  history?: Object, // react router history object
  extraContent?: React.Node
|};

function CoreCard(props: Props): React.Node {
  return (
    <Card>
      <Card.Header>
        <Card.Title>Core {props.coreLabel}</Card.Title>
      </Card.Header>
      <Card.Body>
        {props.extraContent}
      </Card.Body>
    </Card>
  );
}

export default CoreCard;
