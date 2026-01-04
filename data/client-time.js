  // Get the client's local datetime as an ISO string
  const clientTime = new Date().toISOString();

  // Send the datetime to the ESP32 using fetch (POST)
  fetch('/clienttime', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({datetime: clientTime})
  })
  .then(response => response.text())
  .then(data => console.log(data));

  // for repeated time checks, implement the logic in load-cell-coop-netwizard
  // this adds an event listener and sends periodic updates
  