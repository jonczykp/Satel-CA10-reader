#define CFGFILENAME "/satelcfg.json"

void ListAllFilesInDir(String dir_path);

void SaveWiFiConfigFile(String APName, String PASS = "", String HostName = "esp-satelca10",
                        String MQTTServer ="", int MQTTPort = 1883, String MQTTUser="", String MQTTPass ="");
  
String IndexHTMLPage(String APName, String PASS , String HostName,
                     String MQTTServer, int MQTTPort, String MQTTUser, String MQTTPass, String MQTTStatus = "not connected");
