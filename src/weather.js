//Stolen from Pebble tutorial

var myAPIKey = 'c0a93048736d6dbdd3194b0086abc59a';

var xhrRequest = function (url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    callback(this.responseText);
  };
  xhr.open(type, url);
  xhr.send();
};

function locationSuccess(pos) {
  // Construct URL
  var url = "http://api.openweathermap.org/data/2.5/weather?lat=" +
      pos.coords.latitude + "&lon=" + pos.coords.longitude + '&appid=' + myAPIKey;

  // Send request to OpenWeatherMap
  xhrRequest(url, 'GET', 
    function(responseText) {
       try {
      // responseText contains a JSON object with weather info
      var json = JSON.parse(responseText);

      // Temperature in Kelvin requires adjustment
      var temperature = Math.round(json.main.temp - 273.15);

      // Conditions
      var conditions = json.weather[0].main;      
      var description = json.weather[0].description;
      console.log(temperature + " Conditions " + conditions + " " + description + " " + json.name);
           
      description = description.replace(" intensity ", " ").replace(" and ", " & ").replace(" with ", " w/ ").replace("shower ", "");
      
      // Assemble dictionary using our keys
      var dictionary = {
        "KEY_TEMPERATURE": temperature,
        "KEY_CONDITIONS": description.length > 20 ? conditions : description
      };
          console.log(navigator.battery, navigator.getBattery, navigator);
      // Send to Pebble
      Pebble.sendAppMessage(dictionary,
        function(e) {
          console.log("Weather info sent to Pebble successfully!");
        },
        function(e) {
          console.log("Error sending weather info to Pebble!", e);
        }
      );
       } catch(e) {
          console.log("Error parsing weather data", e);
       }
    }      
  );
}

function locationError(err) {
  console.log("Error requesting location!", err);
}

function getWeather() {
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    {timeout: 15000, maximumAge: 60000}
  );
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
  function(e) {
    console.log("PebbleKit JS ready!");

    // Get the initial weather
    getWeather();
  }
);

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
  function(e) {
    console.log("AppMessage received!", e);
    getWeather();
  }                     
);