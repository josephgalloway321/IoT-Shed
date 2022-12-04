var url = "ws://192.168.1.2:1337/";

// This is called when the page finishes loading
function init(){
    // Assign button elements to variables
    ledButton = document.getElementById("toggleLedButton");
    fanButton = document.getElementById("toggleFanButton");
    airCompressorButton = document.getElementById("toggleAirCompressorButton");
    releaseValveButton = document.getElementById("toggleReleaseValveButton");

    // Assign label elements to variables
    ledState = document.getElementById("showLedState");
    fanState = document.getElementById("showFanState");
    airCompressorState = document.getElementById("showAirCompressorState");
    releaseValveState = document.getElementById("showReleaseValveState");
    indoorTempValue = document.getElementById("indoorTempValue");
    outdoorTempValue = document.getElementById("outdoorTempValue");
    pirValue = document.getElementById("pirValue");
    batterySensorValue = document.getElementById("batterySensorValue");
    rainSensorValue = document.getElementById("rainSensorValue");
    limitSwitchValue = document.getElementById("limitSwitchValue");
    
    wsConnect(url);
}

// Call this to connect to the WebSocket server
function wsConnect(url){
    // Connect to WebSocket server
    websocket = new WebSocket(url);

    // Assign callbacks
    websocket.onopen = function(evt) {onOpen(evt)};
    websocket.onclose = function(evt) {onClose(evt)};
    websocket.onmessage = function(evt) {onMessage(evt)};
    websocket.onerror = function(evt) {onError(evt)};
}


/*
* Functions for WebSocket
*/ 
// Called when a WebSocket connection is established with the server
function onOpen(evt){
    // Log connection state
    console.log("Connected");
    alert("Connected");

    // Enable buttons
    ledButton.disabled = false;
    fanButton.disabled = false;
    airCompressorButton.disabled = false;
    releaseValveButton.disabled = false;
}

// Called when the WebSocket connection is closed
function onClose(evt){
    // Log disconnection state
    console.log("Disconnected");
    alert("Disconnected");

    // Disable buttons
    ledButton.disabled = true;
    fanButton.disabled = true;
    airCompressorButton.disabled = true;
    releaseValveButton.disabled = true;

    // Try to reconnect after a few seconds
    setTimeout(function() {wsConnect(url)}, 2000);
}

// Called when a message is received from the server
function onMessage(evt){
    // Print out our received message
    console.log("Received: " + evt.data);

    // Update the status of outputs
    switch(evt.data) {
        case "LED_OFF":
            ledState.innerHTML = "OFF";
            ledState.style.color = "red";
            ledState.style.backgroundColor = "black";
            break;
        case "LED_ON":
            ledState.innerHTML = "ON";
            ledState.style.color = "green";
            break;
        case "FAN_OFF":
            fanState.innerHTML = "OFF";
            fanState.style.color = "red";
            fanState.style.backgroundColor = "black";
            break;
        case "FAN_ON":
            fanState.innerHTML = "ON";
            fanState.style.color = "green";
            break;
        case "AIR_COMPRESSOR_OFF":
            airCompressorState.innerHTML = "OFF";
            airCompressorState.style.color = "red";
            airCompressorState.style.backgroundColor = "black";
            break;
        case "AIR_COMPRESSOR_ON":
            airCompressorState.innerHTML = "ON";
            airCompressorState.style.color = "green";
            break;
        case "RELEASE_VALVE_OFF":
            releaseValveState.innerHTML = "OFF";
            releaseValveState.style.color = "red";
            releaseValveState.style.backgroundColor = "black";
            break;
        case "RELEASE_VALVE_ON":
            releaseValveState.innerHTML = "ON";
            releaseValveState.style.color = "green";
            break;
        default:
            break;
    }

    // Update the status of inputs
    // Order: [NONE, Limit Switch, Rain sensor, Indoor Temp., Outdoor Temp., PIR, Battery]
    if(evt.data.substr(0, 7) == "SENSORS"){
        var splitArray = evt.data.split(",");
        limitSwitchValue.innerHTML = splitArray[1];
        rainSensorValue.innerHTML = splitArray[2];
        indoorTempValue.innerHTML = splitArray[3];
        outdoorTempValue.innerHTML = splitArray[4];
        pirValue.innerHTML = splitArray[5];
        batterySensorValue.innerHTML = splitArray[6];
    }
}
 
// Called when a WebSocket error occurs
function onError(evt){
    console.log("ERROR: " + evt.data);
}


/*
* Functions for interacting with website
*/
// Sends a message to the server (and prints it to the console)
function doSend(message){
    console.log("Sending: " + message);
    websocket.send(message);
}

// Called whenever the Toggle LED button is pressed
function onLedButtonPress(){
    doSend("toggleLedButton");
}

// Called whenever the Fan button is pressed
function onFanButtonPress(){
    doSend("toggleFanButton");
}

// Called whenever the Air Compressor button is pressed
function onAirCompressorButtonPress(){
    doSend("toggleAirCompressorButton");
}

// Called whenever the Release Valve button is pressed
function onReleaseValveButtonPress(){
    doSend("toggleReleaseValveButton");
}

// Call the init function as soon as the page loads
window.addEventListener("load", init, false);