// Get current sensor readings when the page loads  
window.addEventListener('load', getReadings);

var container = document.getElementById('gauge-card');
var size = container.offsetWidth;
var lastTime;

var gaugeWater = new RadialGauge({
    renderTo: 'gauge',
    width: size,
    height: size,
    units: "Gallons",
    minValue: 0,
    maxValue: 800,
    colorValueBoxRect: "#049faa",
    colorValueBoxRectEnd: "#049faa",
    colorValueBoxBackground: "#f1fbfc",
    valueBox: true,
    valueInt: 3,
    valueDec: 0,
    majorTicks: [
        "0",
        "100",
        "200",
        "300",
        "400",
        "500",
        "600",
        "700",
        "800"
    ],
    minorTicks: 0,
    strokeTicks: true,
    colorPlate: "#fff",
    borderShadowWidth: 0,
    borders: false,
    needleType: "line",
    colorNeedle: "#007F80",
    colorNeedleEnd: "#007F80",
    needleWidth: 3,
    needleCircleSize: 3,
    colorNeedleCircleOuter: "#007F80",
    needleCircleOuter: true,
    needleCircleInner: false,
    animation: false
  }).draw();
  
// Function to get current readings on the webpage when it loads for the first time
function getReadings(){
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var myObj = JSON.parse(this.responseText);
      console.log(myObj);
      gaugeWater.value = myObj.waterlevel;
    }
  }; 
  xhr.open("GET", "/readings", true);
  xhr.send();
}

if (!!window.EventSource) {
  var source = new EventSource('/events');
  
  source.addEventListener('open', function(e) {
    console.log("Events Connected");
  }, false);

  source.addEventListener('error', function(e) {
    if (e.target.readyState != EventSource.OPEN) {
      console.log("Events Disconnected");
    }
  }, false);
  
  source.addEventListener('message', function(e) {
    console.log("message", e.data);
  }, false);
  
  source.addEventListener('new_readings', function(e) {
    console.log("new_readings", e.data);
    var myObj = JSON.parse(e.data);
    console.log(myObj);
    var timeDelta = myObj.time - lastTime;
    lastTime = myObj.time;
    console.log(timeDelta/1000);
    gaugeWater.value = myObj.waterlevel;
    document.getElementById('inches').innerHTML = myObj.distanceInch;
  }, false);
}