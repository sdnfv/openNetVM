// @flow

import * as React from "react";
import {
  HashRouter as Router,
  Route,
  Switch,
  Redirect
} from "react-router-dom";

import AppWrapper from "./AppWrapper.react";
import NFDashboardPage from "./pages/NFDashboardPage.react";
import PortDashboardPage from "./pages/PortDashboardPage.react";
import SingleNFPage from "./pages/SingleNFPage.react";
import CoreMappingsPage from "./pages/CoreMappingsPage.react";
import Error404Page from "./pages/Error404Page.react";

import "tabler-react/dist/Tabler.css";
import "./c3jscustom.css";

type Props = {||};

function App(props: Props): React.Node {
  return (
    <Router>
      <AppWrapper>
        <Router>
          <Switch>
            <Route
              exact
              path="/"
              render={() => <Redirect to={{ pathname: "/nfs" }} />}
            />
            <Route exact path="/nfs" component={NFDashboardPage} />
            <Route exact path="/nfs/:nfLabel" component={SingleNFPage} />
            <Route exact path="/ports" component={PortDashboardPage} />
            <Route exact path="/core-mappings" component={CoreMappingsPage} />
            <Route component={Error404Page} />
          </Switch>
        </Router>
      </AppWrapper>
    </Router>
  );
}

export default App;
