# http_elegant_ota
ESP8266 http and elegant ota

Show config json
<pre>/config</pre>

Connect to WiFi
<pre>/wifi?apssid=[SSID_NAME]&appsk=[PASSWORD]</pre>

Disconnect WiFi
<pre>/wifi_disconnect</pre>

Elegant OTA
<pre>/update</pre>

Set server for http update
<pre>/server?url=[http://SERVER_NAME]&port=[SERVER_PORT]</pre>

Delete server url
<pre>/server?del=</pre>

Get firmware update from http/https
<pre>/getupdate</pre>

Change hotspot name / password
<pre>/spot?ssid=[SSID_NAME]&password=[PASSWORD]</pre>

Reset hotspot
<pre>/spot?del=</pre>

Reboot device
<pre>/reboot</pre>

# Just note
- Make firmware.bin : Arduino > Sketch > Export Compiled Binary
- Make filesystem : Arduino > Tools > Sketch Data Upload, then copy file (.bin) from console information path ([SPIFFS] upload  : xxxxxx.bin)
- For http update ota : place in [http://domain]/update/file.bin
