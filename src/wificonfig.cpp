#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>    

#include "wificonfig.h"


void SaveWiFiConfigFile(String APName, String PASS, String HostName, String MQTTServer, int MQTTPort, String MQTTUser, String MQTTPass) {
      
      JsonDocument jsonBuffer;
      jsonBuffer["wifiAP"] = APName;
      jsonBuffer["wifiPASS"] = PASS;
      jsonBuffer["wifiHOSTNAME"] = HostName;
      jsonBuffer["mqttSERVER"] = MQTTServer;
      jsonBuffer["mqttPORT"] = MQTTPort;
      jsonBuffer["mqttUSER"] = MQTTUser;
      jsonBuffer["mqttPASS"] = MQTTPass;

      File configFile = LittleFS.open(CFGFILENAME, "w");
      if (!configFile) {
        Serial.println("failed to open WIFI config file for writing");
      }

      serializeJson(jsonBuffer, Serial);
      serializeJson(jsonBuffer, configFile);
      configFile.close();
      //end save
}

void ListAllFilesInDir(String dir_path) {
	Dir dir = LittleFS.openDir(dir_path);
  Serial.println("Files on the ESP:");
	while(dir.next()) {
		if (dir.isFile()) 
			Serial.println("File: " + dir_path + dir.fileName());
		if (dir.isDirectory()) {
			// print directory names
			Serial.println("Dir: " + dir_path + dir.fileName() + "/");
			// recursive file listing inside new directory
			ListAllFilesInDir(dir_path + dir.fileName() + "/");
		}
	}
}



String IndexHTMLPage(String APName, String PASS , String HostName,
                     String MQTTServer, int MQTTPort, String MQTTUser, String MQTTPass, String MQTTStatus)
{
 return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Wi-Fi Credentials</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <link rel="stylesheet" type="text/css" href="style.css">
</head>
<body>
  <div class="topnav">
    <h1>ESP Satel CA Wi-Fi &amp; MQTT Config</h1>
  </div>
  <div class="content">
    <div class="card-grid">
      <div class="card">
        <form action="/" method="POST">
                    
			<div><strong>WiFi Configuration</strong></div>			
            <table style="width: 100%">
				<tr>
					<td style="width: 79px; height: 26px;">
			  			<label for="ssid">SSID:</label></td>
					<td style="height: 26px">
            			<input type="text" id ="ssid" name="ssid" style="width: 200px" value=")rawliteral" + APName + R"rawliteral("></td>
				</tr>
				<tr>
					<td style="width: 79px">
            			<label for="pass">Password:</label></td>
					<td>
            			<input type="password" id ="pass" name="pass" style="width: 200px" value=")rawliteral" + PASS + R"rawliteral("></td>
				</tr>
				<tr>
					<td style="width: 79px; height: 26px;">
			            <label for="hostname">Host name:</label></td>
					<td style="height: 26px">
			            <input type="text" id ="hostname" name="hostname" value=")rawliteral" + HostName + R"rawliteral(" style="width: 200px"></td>
				</tr>			
			</table>
			<br>
	       	<div><strong>MQTT Server Configuration</strong></div>
            <table style="width: 100%">
				<tr>
					<td style="width: 82px"><label for="mqttserver">Server:</label></td>
					<td>
					<input type="text" id ="mqttserver" name="mqttserver" style="width: 197px" value=")rawliteral" + MQTTServer + R"rawliteral("></td>
				</tr>
				<tr>
					<td style="width: 82px"><label for="mqttport">Port: </label></td>
					<td>
					<input type="number" id ="mqttport" name="mqttport" value=")rawliteral" + String(MQTTPort) + R"rawliteral(" style="width: 49px"></td>
				</tr>
				<tr>
					<td style="width: 82px"><label for="mqttuser">User:</label></td>
					<td>
					<input type="text" id ="mqttuser" name="mqttuser" style="width: 111px" value=")rawliteral" + MQTTUser + R"rawliteral("></td>
				</tr>
				<tr>
					<td style="width: 82px"><label for="mqttpass">Password:</label></td>
					<td>
					<input type="password" id ="mqttpass" name="mqttpass" value=")rawliteral" + MQTTPass + R"rawliteral("></td>
				</tr>
				<tr>
					<td style="width: 82px">Status:</td>
					<td>)rawliteral" + MQTTStatus + R"rawliteral(</td>
				</tr>
			</table>

            <br>
            <input type ="submit" value ="Submit">
          
        </form>
      </div>
    </div>
  </div>
</body>
</html>)rawliteral";
}
