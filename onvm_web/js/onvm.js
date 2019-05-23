// Constants / config data
var UPDATE_RATE = 3000; // constant for how many ms to wait before updating, defaults to 3 seconds
var Y_MIN = 0; // defaults to zero (should never be negative)
var Y_MAX = 30000000; // defaults to 30M pps
var X_RANGE = 30; // defaults to 30 SECONDS

var graphIds; // array of graph ids
var graphDataSets; // hashtable of graph ids and data sets used by graphs
var onvmDataSets; // hashtable of graph ids and data sets storing ALL data that is used for CSV generation
var graphs; // hashtable of graph ids and graphs

var xAxisCounter;

var nfStatsSection;
var portStatsSection;

var graph_colors = ["#3c0f6b", "#d3a85c", "#ace536", "#2e650e",
                    "#2e554c", "#ed7b7b", "#b27caa", "#9a015c",
                    "#eab7c1", "#20c297", "#e7cf3d", "#c71e1e",
                    "#e23a60", "#d7647b", "#eb5703", "#d3abbe"];

var isPaused;

/*
 * List of Functions provided in this file:
 *
 * function initWebStats()
 * function readConfig()
 * function initGraphs()
 * function createDomElement(dataObj, parent)
 * function createNfGraphs(nfArray)
 * function createPortGraphs(portArray)
 * function createPortGraph(port)
 * function createNfGraph(nf)
 * function generateGraph(obj)
 * function updateGraphs()
 * function updateRawText()
 * function refreshGraphById(id)
 * function indexOfNfWithLabel(arr, label)
 * function checkStoppedNfs(nfArray)
 * function renderGraphs(nfArray)
 * function renderText(text)
 * function renderUpdateTime(updateTime)
 * function handleAutoUpdateButtonClick()
 * function determineMaxTime()
 * function generateCSV()
 * function handleDownloadButtonClick()
 */

function initWebStats(){
    var config = readConfig();
    if(config != null){
        if(config.hasOwnProperty('refresh_rate')){
	    UPDATE_RATE = config.refresh_rate;
        }

        if(config.hasOwnProperty('y_min')){
	    Y_MIN = config.y_min;
        }

        if(config.hasOwnProperty('y_max')){
	    Y_MAX = config.y_max;
        }

        if(config.hasOwnProperty('x_range')){
	    X_RANGE = config.x_range;
        }

        if(config.hasOwnProperty('refresh_rate')){
	    UPDATE_RATE = config.refresh_rate;
        }
    }

    nfStatsSection = document.getElementById("onvm_nf_stats");
    portStatsSection = document.getElementById("onvm_port_stats");

    isPaused = false;

    graphIds = [];
    graphs = {};
    graphDataSets = {};
    onvmDataSets = {};
    xAxisCounter = 0;

    initGraphs(); // creates the graph with no data

    updateRawText(); // populates the raw text

    // sets updates to run in background every <UPDATE_RATE> milliseconds
    setInterval(function(){
        if(!isPaused){
	    console.log("Updating.");
            updateGraphs();
            updateRawText();

            // this ensures the X axis is measured in real time seconds
            xAxisCounter += (UPDATE_RATE / 1000);
        }
    }, UPDATE_RATE);
}

function readConfig(){
    // makes a synchronous get request (bad practice, but necessary for reading config before proceeding)
    function syncGetRequest(){
	var req = null;

	try{
	    req = new XMLHttpRequest();
	    req.open("GET", "/config.json", false);
	    req.send(null);
	}catch(e){}

	return {'code': req.status, 'text': req.responseText};
    }

    var config = syncGetRequest();
    if(config.code != 200){
	console.log("Error fetching config. Continuing with default configuration.");
	return null;
    }

    try {
	return JSON.parse(config.text);
    }catch(e){
	console.log("Error parsing config.  Continuing with default configuration.");
	return null;
    }
}

function initGraphs(){
    $.ajax({
        url: "/onvm_json_stats.json",
        method: "GET",
        success: function(resultData){
            createNfGraphs(resultData.onvm_nf_stats);
            createPortGraphs(resultData.onvm_port_stats);
        },
        error: function(){
            console.log("Error fetching graph data!");
        }
    });
}

function createDomElement(dataObj, parent){
    var graph = document.createElement('canvas');
    graph.id = dataObj.Label;
    graph.style = "display: inline-block; padding: 15px;";
    graph.width = 25;
    graph.height = 25;
    parent.appendChild(graph);
}

function createNfGraphs(nfArray){
    for(var i = 0; i < nfArray.length; ++i){
        var nf = nfArray[i];

        createNfGraph(nf);
    }

}

function createPortGraphs(portArray){
    for(var i = 0; i < portArray.length; ++i){
        var port = portArray[i];

        createPortGraph(port);
    }

}

function createPortGraph(port){
    createDomElement(port, portStatsSection);
    generateGraph(port);

    graphIds.push(port.Label);
}

function createNfGraph(nf){
    createDomElement(nf, nfStatsSection);
    generateGraph(nf);

    graphIds.push(nf.Label);
}

function generateGraph(nf){
    var nfGraphCtx = document.getElementById(nf.Label);

    var nfDataSet = {
        datasets: [
            {
                label: nf.Label + " RX",
                borderColor: "#778899",
                data: [
                    {
                        x: xAxisCounter,
                        y: nf.RX
                    }
                ]
            },
            {
                label: nf.Label + " TX",
                borderColor: "#96CA2D",
                data: [
                    {
                        x: xAxisCounter,
                        y: nf.TX
                    }
                ]
            }
        ]
    };

    graphDataSets[nf.Label] = nfDataSet;
    onvmDataSets[nf.Label] = jQuery.extend(true, {}, nfDataSet);

    // add drop rates to ONVM DATA SETS ONLY
    // do not add to graph data sets directly, will mess with graphs
    var tx_drop_rate = {
        label: nf.Label + " TX Drop Rate",
        data: [
            {
                x: xAxisCounter,
                y: nf.TX_Drop_Rate
            }
        ]
    };

    var rx_drop_rate = {
        label: nf.Label + " RX Drop Rate",
        data: [
            {
                x: xAxisCounter,
                y: nf.RX_Drop_Rate
            }
        ]
    };

    if(nf.Label.toUpperCase().includes("NF")){
        onvmDataSets[nf.Label].datasets.push(tx_drop_rate);
        onvmDataSets[nf.Label].datasets.push(rx_drop_rate);
    }

    //console.log(onvmDataSets[nf.Label]);

    var TITLE;
    if(nf.Label.toUpperCase().includes("NF")){
        TITLE = nf.Label + " (Drop Rates: {tx: " + nf.TX_Drop_Rate + ", rx: " + nf.RX_Drop_Rate + "})";
    }else{
        TITLE = nf.Label;
    }

    var options = {
        title: {
            display: true,
            text: TITLE
        },
        scales: {
            yAxes: [{
		ticks: {
		    min: Y_MIN,
		    max: Y_MAX
		},
                scaleLabel: {
                    display: true,
                    labelString: 'Packets'
                }
            }],
            xAxes: [{
		ticks: {
		    min: 0,
		    max: X_RANGE
		},
                scaleLabel: {
                    display: true,
                    labelString: 'Seconds'
                }
            }]
        }
    };

    var nfGraph = new Chart(nfGraphCtx, {
        type: 'scatter',
        data: nfDataSet,
        options: options
    });

    graphs[nf.Label] = nfGraph;

    graphs[nf.Label].options.scales.xAxes[0].ticks.min = xAxisCounter;
    graphs[nf.Label].options.scales.xAxes[0].ticks.max = xAxisCounter + X_RANGE;
}

function updateGraphs(){
    $.ajax({
        url: "/onvm_json_stats.json",
        method: "GET",
        success: function(resultData){
            renderUpdateTime(resultData.last_updated);
            renderGraphs(resultData.onvm_nf_stats.concat(resultData.onvm_port_stats));
        },
        error: function(){
            console.log("Error fetching graph data!");
        }
    });
}

function updateRawText(){
    $.ajax({
        url: "/onvm_stats.txt",
        method: "GET",
        success: function(resultData){
            renderText(resultData);
        },
        error: function(){
            console.log("Error fetching stats text!");
        }
    });
}

function refreshGraphById(id){
    var graph = graphs[id];
    graph.update();
}

function indexOfNfWithLabel(arr, label){
    for(var i = 0; i < arr.length; ++i){
        if(arr[i].Label == label){
            return i;
        }
    }

    return -1;
}

function checkStoppedNfs(nfArray){
    if(nfArray.length == graphIds.length) return;

    var stoppedNfIds = [];

    for(var i = 0; i < graphIds.length; ++i){
        var graphId = graphIds[i];

        var index = indexOfNfWithLabel(nfArray, graphId);

        if(index == -1) stoppedNfIds.push(graphId);
    }

    for(var j = 0; j < stoppedNfIds.length; ++j){
        var stoppedNfId = stoppedNfIds[j];

        // remove from data structures ( graphs, graphids )
        delete graphs[stoppedNfId]; // remove from graphs hashtable
        // do not remove from graphDataSets hashtable, this enables us to export its data to CSV

        var idIndex = graphIds.indexOf(stoppedNfId);

        var tmp = graphIds[0];
        graphIds[0] = graphIds[idIndex];
        graphIds[idIndex] = tmp;

        graphIds.shift();

        // remove from DOM
        var element = document.getElementById(stoppedNfId);
        element.parentNode.removeChild(element);
    }
}

function renderGraphs(nfArray){
    for(var i = 0; i < nfArray.length; ++i){
        var nf = nfArray[i];

        if(graphIds.indexOf(nf.Label) == -1){
            // it doesnt exist yet, it's a new nf
            // create a graph for it
            createNfGraph(nf);
        }else{
            //update its data
            var nfDataSet = graphDataSets[nf.Label].datasets;
	    var onvmDataSet = onvmDataSets[nf.Label].datasets;

            var title = graphs[nf.Label].options.title;
            var TITLE;
            if(nf.Label.toUpperCase().includes("NF")){
                TITLE = nf.Label + " (Drop Rates: {tx: " + nf.TX_Drop_Rate + ", rx: " + nf.RX_Drop_Rate + "})";
            }else{
                TITLE = nf.Label;
            }
            title.text = TITLE;

            //console.log("ONVM DATA SET");
            //console.log(onvmDataSet);
            if(nf.Label.toUpperCase().includes("NF")){
                for(var z = 0; z < onvmDataSet.length; ++z){
                    if(onvmDataSet[z].label == (nf.Label + " RX Drop Rate")){
                        //console.log("INSERT RXDR DATA");
                        onvmDataSet[z].data.push({
                            x: xAxisCounter,
                            y: nf.RX_Drop_Rate
                        });
                    }else if(onvmDataSet[z].label == (nf.Label + " TX Drop Rate")){
                        //console.log("INSERT TXDR DATA");
                        onvmDataSet[z].data.push({
                            x: xAxisCounter,
                            y: nf.TX_Drop_Rate
                        });
                    }
                }
            }

            for(var d = 0; d < nfDataSet.length; ++d){
                var dataSet = nfDataSet[d];
		var onvmSet = onvmDataSet[d];

                if(dataSet.label == (nf.Label + " TX")){
                    dataSet.data.push({
                        x: xAxisCounter,
                        y: nf.TX
                    });
		    onvmSet.data.push({
			x: xAxisCounter,
			y: nf.TX
		    });
		    if((dataSet.data.length * (UPDATE_RATE / 1000.0)) > X_RANGE){
			dataSet.data.shift();
			graphs[nf.Label].options.scales.xAxes[0].ticks.min = dataSet.data[0].x;
			graphs[nf.Label].options.scales.xAxes[0].ticks.max = dataSet.data[dataSet.data.length - 1].x;
		    }
                }else if(dataSet.label == (nf.Label + " RX")){
                    dataSet.data.push({
                        x: xAxisCounter,
                        y: nf.RX
                    });
		    onvmSet.data.push({
			x: xAxisCounter,
			y: nf.RX
		    });
		    if((dataSet.data.length * (UPDATE_RATE / 1000.0)) > X_RANGE){
			dataSet.data.shift();
			graphs[nf.Label].options.scales.xAxes[0].ticks.min = dataSet.data[0].x;
			graphs[nf.Label].options.scales.xAxes[0].ticks.max = dataSet.data[dataSet.data.length - 1].x;
		    }
                }else{
                    // something went wrong, TX and RX should be the only 2 datasets (aka lines) on a graph
                    console.log("Error with data sets!");
                }
            }

            refreshGraphById(nf.Label);
        }
    }

    checkStoppedNfs(nfArray);
}

function renderText(text){
    $("#onvm_raw_stats").html(text);
}

function renderUpdateTime(updateTime){
    $("#last-updated-time").html(updateTime);
}

function handleAutoUpdateButtonClick(){
    if(isPaused){
        $("#auto-update-button").text("Pause Auto Update");
    }else{
        $("#auto-update-button").text("Resume Auto Update");
    }

    isPaused = !isPaused;
}

function determineMaxTime(dataSets){
    var maxTime = -1;

    for(var key in dataSets){
    	if(dataSets.hasOwnProperty(key)){
	    var objDS = dataSets[key].datasets; // var for object data sets (TX and RX Data)
            //console.log(objDS);
	    for(var i = 0; i < objDS.length; ++i){

		var singleDsData = objDS[i].data;

                if(singleDsData.length == 0){
                    maxTime = 0;
                    continue;
                }

		var singleDsMaxTime = singleDsData[singleDsData.length - 1].x;

		if(singleDsMaxTime > maxTime) maxTime = singleDsMaxTime;
	    }
    	}
    }

    return maxTime;
}

function generateCSV(){
    // deep copy the graphDataSets object to avoid recieving new data during this process and messing up calculations / export
    var copiedDataSets = jQuery.extend(true, {}, onvmDataSets);

    // this gets us the highest value on the x-axis to generate the CSV to
    var maxTime = determineMaxTime(copiedDataSets);

    var header = "";
    var keys = []; // this array is used to keep data in same order as header
    for(var key in copiedDataSets){
    	if(copiedDataSets.hasOwnProperty(key)){
	    var dsArr = copiedDataSets[key].datasets;
	    for(var i = 0; i < dsArr.length; ++i){
		header += (dsArr[i].label + ",");
                if(dsArr[i].label != null){
		    keys.push({'key': key, 'label': dsArr[i].label});
                }
	    }
    	}
    }
    if(header.length > 0){
	header = "time (s)," + header.substring(0, header.length -1);
    }

    var csvArr = [];
    csvArr.push(header);

    // for each time we have data for (0 thru max time)
    for(var time = 0; time <= maxTime; time += (UPDATE_RATE / 1000)){
	var csvLine = "" + time;

	// iterate through each NF data IN ORDER (based on key array) and fetch its data for that time
        //console.log("KEYS:");
        //console.log(keys);
	for(var i = 0; i < keys.length; ++i){
	    var key = keys[i];

	    // locate the data set we are looking for based on the key
	    var dsArr = copiedDataSets[key.key].datasets;
	    var currentDataSet = null;
	    for(var j = 0; j < dsArr.length; ++j){
		if(dsArr[j].label == key.label){
		    currentDataSet = dsArr[j];
		    break;
		}
	    }

	    if(currentDataSet == null) continue; // something went wrong

	    // find the data point we are looking for, if it's not there we will denote using -1
	    var data = currentDataSet.data;
	    var yVal = -1;
	    for(var j = 0; j < data.length; ++j){
		if(data[j].x == time){
		    yVal = data[j].y;
		    break;
		}
	    }
	    csvLine += ("," + yVal);
	}

	csvArr.push(csvLine);
    }

    // convert the array of csv lines to a single string
    var csvStr = "";
    for(var i = 0; i < csvArr.length; ++i){
	csvStr += (csvArr[i] + "\n");
    }

    return csvStr;
}

function handleDownloadButtonClick(){
    // generate the CSV from the current data
    var csv = generateCSV();

    // creates a fake element and clicks it
    var dataLink = document.createElement("a");
    dataLink.textContent = 'download';
    dataLink.download = "onvm-data.csv";
    dataLink.href="data:text/csv;charset=utf-8," + escape(csv);

    document.body.appendChild(dataLink);
    dataLink.click(); // simulate a click of the link
    document.body.removeChild(dataLink);
}
