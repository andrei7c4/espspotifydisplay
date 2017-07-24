# ESP8266 based Spotify currently playing track display
This is an ESP8266 based Spotify client built with 256x64 OLED (SSD1322 based) display. The main purpose is to retrieve information about the currently playing track and show it on the display. ESP8266 connects directly to Spotify, so no third-party proxy services are used.

Application uses [Spotify Web API](https://developer.spotify.com/web-api/) and implements OAuth 1.0a authorization as described [here](https://developer.spotify.com/web-api/authorization-guide/).

[currently-playing](https://developer.spotify.com/web-api/get-the-users-currently-playing-track/) endpoint is used to retrieve the track information and [play](https://developer.spotify.com/web-api/start-a-users-playback/), [pause](https://developer.spotify.com/web-api/pause-a-users-playback/) and [next](https://developer.spotify.com/web-api/skip-users-playback-to-next-track/) endpoints are used to control the playback.

