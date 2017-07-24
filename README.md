# ESP8266 based Spotify currently playing track display
This is an ESP8266 based Spotify client built with 256x64 OLED (SSD1322 based) display. The main purpose is to retrieve information about the currently playing track and show it on the display. ESP8266 connects directly to Spotify, so no third-party proxy services are used.

Application uses [Spotify Web API](https://developer.spotify.com/web-api/) and implements OAuth 2.0a authorization as described [here](https://developer.spotify.com/web-api/authorization-guide/#authorization_code_flow). [Currently-playing](https://developer.spotify.com/web-api/get-the-users-currently-playing-track/) endpoint is used to retrieve the track information and [play](https://developer.spotify.com/web-api/start-a-users-playback/), [pause](https://developer.spotify.com/web-api/pause-a-users-playback/) and [next](https://developer.spotify.com/web-api/skip-users-playback-to-next-track/) endpoints are used to control the playback.

Unicode is supported. Glyphs for all characters found in Arial Unicode MS font are embedded in the binary. The glyphs are regular and bold variants with the sizes of 10 and 13.

## Building the hardware/software
The same hardware is used as in my other project [ESP8266 Twitter Client](https://github.com/andrei7c4/esptwitterclient/) so please check the schematics and instructions from there. The same applies to the software building and binary flashing.

## Usage
Device settings can be changed through the serial interface (921600/8-N-1). The following syntax should be used:
```
parameter:value<CR>
```
At least the following parameters must be set by the user:
 - ssid - WiFi SSID
 - pass - WiFi password
 - client_id - Spotify app client id
 - client_secret - Spotify app client secret key
 - auth_code - Spotify app authorization code

In order to obtain Spotify app client id and secret key, user must create a new Spotify app with her own account:
 1. Go to https://developer.spotify.com/my-applications/#!/applications/create
 2. Fill the form. Set *Redirect URIs* to `http://httpbin.org/anything` and other fields to values of your choice.
 3. When the app is created, copy *Client ID* and *Client Secret* values into the device.

Next, we need to obtain the authorization code:
 1. In the following URL replace <client_id> with your app client id and open the URL with your browser. `https://accounts.spotify.com/authorize/?client_id=<client_id>&response_type=code&redirect_uri=http%3A%2F%2Fhttpbin.org%2Fanything&scope=user-read-private%20user-read-currently-playing%20user-read-playback-state%20user-modify-playback-state`
 2. Authorize the app and you will be redirected to httpbin.org/anything.
 3. Copy the code (without quotes) into the device. The code can also be copied from browser address bar.

The device should now be able to login to Spotify with the user's account and display the currently playing track. By default the device will poll Spotify server every 10 seconds. Poll interval can be changed with `poll` parameter (value in seconds). Please see the [config.c](src/config.c) file for additional supported parameters.

There are two buttons: one to pause and resume the playback and another to skip to next track.
