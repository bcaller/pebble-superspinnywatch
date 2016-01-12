//Stolen from Pebble tutorial

//var myAPIKey = 'KEY ********************';
var myAPIKey = 'c0a93048736d6dbdd3194b0086abc59a';

var UPDATE_INTERVAL = 60 * 60 * 1000;
var doWeather = !localStorage.getItem('NOWEATHER');
var fahrenheit = localStorage.getItem('TEMP_UNITS_F');

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
      localStorage.setItem('KEY_TEMPERATURE', temperature); //always in Celsius
      localStorage.setItem('KEY_CONDITIONS', description.length > 20 ? conditions : description);
           localStorage.setItem('lut', Date.now());
           reportWeather();
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
    if(!doWeather) return;
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    {timeout: 30000, maximumAge: 20 * 60 * 1000}
  );
    setTimeout(getWeather, UPDATE_INTERVAL);
}

function reportWeather() {
    if(!doWeather) return;
      // Send to Pebble
    var dict = {
        KEY_TEMPERATURE: parseInt(localStorage.getItem('KEY_TEMPERATURE')),
        KEY_CONDITIONS: localStorage.getItem('KEY_CONDITIONS')
      };
    if(fahrenheit) {
        dict.KEY_TEMPERATURE = Math.round(dict.KEY_TEMPERATURE * 9 / 5) + 32;
    }
    console.log(JSON.stringify(dict));
      Pebble.sendAppMessage(dict,
        function(e) {
          console.log("Weather info sent to Pebble successfully!", dict);
        },
        function(e) {
          console.log("Error sending weather info to Pebble!", e);
        }
      );
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
  function(e) {
    console.log("PebbleKit JS ready!");

    // Get the initial weather
    var lastUpdateTime = localStorage.getItem('lut');
    var currentTime = Date.now();
    if(!lastUpdateTime || currentTime < lastUpdateTime) {
        getWeather();
    } else if(currentTime - lastUpdateTime > UPDATE_INTERVAL * 3 / 4) {
        reportWeather();
        getWeather();
    } else {
        console.log("reporting old weather conditions (cheeky)");
        reportWeather();
        setTimeout(getWeather, UPDATE_INTERVAL - (currentTime - lastUpdateTime));
    }
  }
);

Pebble.addEventListener('showConfiguration', function(e) {
  // Show config page
  Pebble.openURL('https://bcaller.github.io/pebble-superspinnywatch/config.html');
});

function boolConfig(key, configData) {
    if(configData[key]) localStorage.setItem(key, true);
    else localStorage.removeItem(key);
    return configData[key];
}

Pebble.addEventListener('webviewclosed', function(e) {
  var configData = JSON.parse(decodeURIComponent(e.response));
    if("BG" in configData) {
        //Valid settings
        var didWeather = doWeather;
        doWeather = configData['WEATHER'];
        if(!doWeather) localStorage.setItem('NOWEATHER', true);
        else localStorage.removeItem('NOWEATHER');
        fahrenheit = boolConfig('TEMP_UNITS_F', configData);        
          Pebble.sendAppMessage(configData, function() {
            console.log('Send successful: ' + JSON.stringify(configData));
          }, function() {
            console.log('Send failed! ' + JSON.stringify(configData));
          });
        if(doWeather) {
            if(!didWeather) getWeather();
            else reportWeather();
        }
    } else {
        console.log("Config data is bad " + JSON.stringify(configData));
    }
});