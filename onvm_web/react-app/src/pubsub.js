// @flow

export type OnvmNfData = {|
  Label: string,
  RX: number,
  TX: number,
  TX_Drop_Rate: number,
  RX_Drop_Rate: number,
  instance_id: number,
  service_id: number,
  core: number
|};

export type ColumnRestoreData = Array<Array<string | number>>;

type NFSubscriberCallback = (data: OnvmNfData, counter: number) => void;
type NFSubscriberMap = { [nfLabel: string]: Array<NFSubscriberCallback> };
type NFColumnRestoreMap = { [nfLabel: string]: ColumnRestoreData };

type OnvmResponse = {|
  last_updated: string,
  onvm_port_stats: Object, // FIXME: specify type
  onvm_nf_stats: {| [string]: OnvmNfData |}
|};

export type OnvmEvent = {|
  message: string,
  instance_id: number,
  service_id: number,
  core: number
|};
type OnvmEventsResponse = Array<OnvmEvent>;

type OnvmEventSubscriberCallback = OnvmEvent => void;

type OnvmEventSubscriberInfo = {|
  name: string,
  callback: OnvmEventSubscriberCallback,
  includePastEvents: boolean,
  hasBeenCalled: boolean
|};

const nfColumnRestoreMap: NFColumnRestoreMap = {};
const nfSubscriberMap: NFSubscriberMap = {};
let eventSubscriberList: Array<OnvmEventSubscriberInfo> = [];
let counter = 0;
let lastOnvmEventsResponse: ?OnvmEventsResponse = null;

let lastSeenEventIndex = -1;
export function handleEventData(data: OnvmEventsResponse): void {
  // process all unseen events
  while (lastSeenEventIndex < data.length - 1) {
    ++lastSeenEventIndex;
    const event = data[lastSeenEventIndex];
    if (event) {
      eventSubscriberList.forEach(subscriberInfo =>
        subscriberInfo.callback(event)
      );
    }
  }
  lastOnvmEventsResponse = data;
}

export function registerEventSubscriber(
  name: string,
  callback: OnvmEventSubscriberCallback,
  includePastEvents: boolean
) {
  const hasBeenCalled = false;
  const subInfo: OnvmEventSubscriberInfo = {
    name,
    callback,
    includePastEvents,
    hasBeenCalled
  };
  if (includePastEvents && lastOnvmEventsResponse) {
    const data = lastOnvmEventsResponse;
    console.log("Catching event subscriber up to all past events.");
    let i = 0;
    while (i !== lastSeenEventIndex + 1) {
      subInfo.callback(data[i]);
      ++i;
    }
    subInfo.hasBeenCalled = true;
  }
  eventSubscriberList.push(subInfo);
}

export function unregisterEventSubscriber(name: string) {
  eventSubscriberList = eventSubscriberList.filter(e => e.name !== name);
}

export function handleNFData(data: OnvmResponse): void {
  // call NF Subscriber callbacks
  const nfStats = data.onvm_nf_stats;
  for (const nfLabel in nfStats) {
    if (nfLabel in nfSubscriberMap) {
      // eslint-disable-next-line
      nfSubscriberMap[nfLabel].forEach(callback =>
        callback(nfStats[nfLabel], counter)
      );
    } else {
      console.log("No NF Subscriber callback for label: " + nfLabel);
      if (nfLabel in nfColumnRestoreMap) {
        const arrMaxSize = 40;
        const nfData = nfStats[nfLabel];
        const columns = nfColumnRestoreMap[nfLabel];
        columns[0].push(counter);
        columns[0] = trimToSize(columns[0], arrMaxSize);
        columns[1].push(nfData.TX);
        columns[1] = trimToSize(columns[1], arrMaxSize);
        columns[2].push(nfData.RX);
        columns[2] = trimToSize(columns[2], arrMaxSize);
      } else {
        console.error("No NF Restore state for label: " + nfLabel);
      }
    }
  }
  // call port subscriber callbacks
  const portStats = data.onvm_port_stats;
  for (const nfLabel in portStats) {
    if (nfLabel in nfSubscriberMap) {
      // eslint-disable-next-line
      nfSubscriberMap[nfLabel].forEach(callback =>
        callback(portStats[nfLabel], counter)
      );
    } else {
      console.log("No NF Subscriber callback for label: " + nfLabel);
      if (nfLabel in nfColumnRestoreMap) {
        const arrMaxSize = 40;
        const nfData = portStats[nfLabel];
        const columns = nfColumnRestoreMap[nfLabel];
        columns[0].push(counter);
        columns[0] = trimToSize(columns[0], arrMaxSize);
        columns[1].push(nfData.TX);
        columns[1] = trimToSize(columns[1], arrMaxSize);
        columns[2].push(nfData.RX);
        columns[2] = trimToSize(columns[2], arrMaxSize);
      } else {
        console.error("No NF Restore state for label: " + nfLabel);
      }
    }
  }
  ++counter;
}

function trimToSize(arr: Array<*>, size: number): Array<*> {
  if (arr.length < size) return arr;
  const firstElem = arr[0];
  arr = arr.slice(2);
  while (arr.length > size) {
    arr = arr.slice(1);
  }
  return [firstElem, ...arr];
}

export function registerNFSubscriber(
  nfLabel: string,
  callback: NFSubscriberCallback
): ?ColumnRestoreData {
  if (nfLabel in nfSubscriberMap) {
    nfSubscriberMap[nfLabel].push(callback);
  } else {
    nfSubscriberMap[nfLabel] = [callback];
  }

  if (nfLabel in nfColumnRestoreMap) {
    const columnRestoreData = nfColumnRestoreMap[nfLabel];
    return columnRestoreData;
  }
}

export function unregisterNFSubscriber(
  nfLabel: string,
  callback: NFSubscriberCallback,
  currentState: ?ColumnRestoreData
): void {
  if (nfLabel in nfSubscriberMap) {
    const updatedSubscriberList = nfSubscriberMap[nfLabel].filter(
      fn => fn !== callback
    );
    if (updatedSubscriberList.length === 0) {
      delete nfSubscriberMap[nfLabel];
    }
    if (currentState != null) {
      nfColumnRestoreMap[nfLabel] = currentState;
    }
  }
}
