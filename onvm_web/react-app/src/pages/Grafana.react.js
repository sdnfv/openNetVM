// @flow

import * as React from "react";

const hostName = window.location.hostname;

class Grafana extends React.Component {
  constructor( props ){
    super();
    this.state = { ...props };
  }
  componentWillMount(){
    window.open(`http://${hostName}:3000`);
  }
  render(){
    return (<section>...</section>);
  }
}
 
export default Grafana;