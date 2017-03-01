var UPDATE_RATE = 3000; // constant for how many ms to wait before updating

var graphIds; // array of graph ids
var graphDataSets; // hashtable of graph ids and data sets
var graphs; // hashtable of graph ids and graphs

var xAxisCounter;

var nfStatsSection;
var portStatsSection;

var graph_colors = ["#3c0f6b", "#d3a85c", "#ace536", "#2e650e",
                    "#2e554c", "#ed7b7b", "#b27caa", "#9a015c",
                    "#eab7c1", "#20c297", "#e7cf3d", "#c71e1e",
                    "#e23a60", "#d7647b", "#eb5703", "#d3abbe"];

var isPaused;

function initWebStats(){
    nfStatsSection = document.getElementById("onvm_nf_stats");
    portStatsSection = document.getElementById("onvm_port_stats");

    isPaused = false;

    graphIds = [];
    graphs = {};
    graphDataSets = {};
    xAxisCounter = 0;

    initGraphs(); // creates the graph with no data

    updateRawText(); // populates the raw text

    // sets updates to run in background every <UPDATE_RATE> milliseconds
    setInterval(function(){
        if(!isPaused){
            updateGraphs();
            updateRawText();

            // this ensures the X axis is measured in real time seconds
            xAxisCounter += (UPDATE_RATE / 1000);
        }
    }, UPDATE_RATE);
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
    generateNfGraph(port);

    graphIds.push(port.Label);
}

function createNfGraph(nf){
    createDomElement(nf, nfStatsSection);
    generateNfGraph(nf);

    graphIds.push(nf.Label);
}

function generateNfGraph(nf){
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

    var options = {
        title: {
            display: true,
            text: nf.Label
        },
        scales: {
            yAxes: [{
                scaleLabel: {
                    display: true,
                    labelString: 'Packets'
                }
            }],
            xAxes: [{
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

        // remove from data structures ( graphs, datasets, graphids )
        delete graphs[stoppedNfId]; // remove from graphs hashtable
        delete graphDataSets[stoppedNfId]; // remove from graphDataSets hashtable

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

            for(var d = 0; d < nfDataSet.length; ++d){
                var dataSet = nfDataSet[d];

                if(dataSet.label == (nf.Label + " TX")){
                    dataSet.data.push({
                        x: xAxisCounter,
                        y: nf.TX
                    });
                }else if(dataSet.label == (nf.Label + " RX")){
                    dataSet.data.push({
                        x: xAxisCounter,
                        y: nf.RX
                    });
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
