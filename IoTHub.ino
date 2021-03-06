#include <SPI.h>
#include <WiFi.h> //For other arduino 
//#include <ESP8266WiFi.h> //For esp8233
#include <WiFiClientSecure.h>

const int LED_PIN = 2 ;

///*** WiFi Network Config ***///
const char* ssid = "{SSID}";               
const char* password = "{SSID PASS}";   
const String devices_name = "{Device Name}";
const String api_ver = "2016-11-14";

///*** Azure IoT Hub Config ***///
const char hostname[] = "{IoTHub Host}"; 

//format : SharedAccessSignature sr=xxx.azure-devices.net&sig=xxxxxxx&se=xxxxxxxxxx
const String authSAS = "{SAS}"; 



//message receive URI
const String feeduri = "/devices/"+devices_name+"/messages/devicebound?api-version="+api_ver; //feed URI 
String azureReceive  = "/devices/"+devices_name+"/messages/devicebound?api-version="+api_ver; 
String azureComplete = "/devices/"+devices_name+"/messages/devicebound/etag?api-version="+api_ver;         
String azureReject   = "/devices/"+devices_name+"/messages/devicebound/etag?reject&api-version="+api_ver;  
String azureAbandon  = "/devices/"+devices_name+"/messages/devicebound/etag/abandon?&api-version="+api_ver; 

///*** Azure IoT Hub Config ***///
unsigned long cur_time;
unsigned long lastConnectionTime = 0;            
const unsigned long pollingInterval = 5L * 1000L; // 


WiFiClientSecure    client;

void setup() {
   Serial.begin(115200);
   pinMode(LED_PIN, OUTPUT);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
}

void loop() 
{
  String response = "";
  char c;
  while (client.available()) {
    c = client.read();
    response.concat(c);
  }

  if (!response.equals(""))
  {
    if (response.startsWith("HTTP/1.1 204"))
    {
      //turn off onboard LED
      //Serial.println(response);
    }
    else if (response.startsWith("HTTP/1.1 200"))
    {
      String eTag=getHeaderValue(response,"ETag");
      String msg = getResponsePayload(response);
      // Do somethig if success
      azureIoTCompleteMessage(eTag);
      delay(2000);
    }
    else
    {
      Serial.println("Error ");
      Serial.println(response);

    }
  }
  // polling..if pollingInterval has passed
  if (millis() - lastConnectionTime > pollingInterval) {
    azureIoTReceiveMessage();
  }
}

// this method makes an HTTPS connection to the Azure IOT Hub Server:
void azureHttpRequest() {
 
  // close any connection before send a new request.
  // This will free the socket on the WiFi shield
  client.stop();
 
  // if there's a successful connection:
  if (client.connect(hostname, 443)) {
    //make the GET request to the Azure IOT device feed uri
    client.print("GET ");  //Do a GET
    client.print(feeduri);  // On the feedURI
    client.println(" HTTP/1.1"); 
    client.print("Host: "); 
    client.println(hostname);  //with hostname header
    client.print("Authorization: ");
    client.println(authSAS);  //Authorization SAS token obtained from Azure IoT device explorer
    client.println("Connection: close");
    client.println();
 
    // note the time that the connection was made:
    lastConnectionTime = millis();
  }
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
  }
}

void httpRequest(String verb, String uri,String contentType, String content)
{
    if(verb.equals("")) return;
    if(uri.equals("")) return;
 
    // close any connection before send a new request.
    // This will free the socket on the WiFi shield
    client.stop();
   
    // if there's a successful connection:
    if (client.connect(hostname, 443)) {
      client.print(verb); //send POST, GET or DELETE
      client.print(" ");  
      client.print(uri);  // any of the URI
      client.println(" HTTP/1.1"); 
      client.print("Host: "); 
      client.println(hostname);  //with hostname header
      client.print("Authorization: ");
      client.println(authSAS);  //Authorization SAS token obtained from Azure IoT device explorer
      client.println("Connection: close");
      lastConnectionTime = millis();
      if(verb.equals("POST"))
      {
          client.print("Content-Type: ");
          client.println(contentType);
          client.print("Content-Length: ");
          client.println(content.length());
          client.println();
          client.println(content);
      }else
      {
          client.println();
      }
    }
}

void azureIoTReceiveMessage()
{
  httpRequest("GET", azureReceive, "","");
}
 
//Tells Azure IoT Hub that the message with the msgLockId is handled and it can be removed from the queue.
void azureIoTCompleteMessage(String eTag)
{
  String uri=azureComplete;
  uri.replace("etag",trimETag(eTag));
 
  httpRequest("DELETE", uri,"","");
}
 
 
//Tells Azure IoT Hub that the message with the msgLockId is rejected and can be moved to the deadletter queue
void azureIoTRejectMessage(String eTag)
{
  String uri=azureReject;
  uri.replace("etag",trimETag(eTag));
 
  httpRequest("DELETE", uri,"","");
}
 
//Tells Azure IoT Hub that the message with the msgLockId is abondoned and can be requeued.
void azureIoTAbandonMessage(String eTag)
{
  String uri=azureAbandon;
  uri.replace("etag",trimETag(eTag));
 
  httpRequest("POST", uri,"text/plain","");
}

String getHeaderValue(String response, String headerName)
{
  String headerSection=getHeaderSection(response);
  String headerValue="";
  
  int idx=headerSection.indexOf(headerName);
  
  if(idx >=0)
  { 
  int skip=0;
  if(headerName.endsWith(":"))
    skip=headerName.length() + 1;
  else
    skip=headerName.length() + 2;

  int idxStart=idx+skip;  
  int idxEnd = headerSection.indexOf("\r\n", idxStart);
  headerValue=response.substring(idxStart,idxEnd);  
  }
  
  return headerValue;
}

//For some reason Azure IoT sets ETag string enclosed in double quotes 
//and that's not in sync with its other endpoints. So need to remove the double quotes
String trimETag(String value)
{
    String retVal=value;

    if(value.startsWith("\""))
      retVal=value.substring(1);

    if(value.endsWith("\""))
      retVal=retVal.substring(0,retVal.length()-1);

    return retVal;     
}

//To get all the headers from the HTTP Response
String getHeaderSection(String response)
{
  int idxHdrEnd=response.indexOf("\r\n\r\n");

  return response.substring(0, idxHdrEnd);
}

//To get only the message payload from http response.
String getResponsePayload(String response)
{
  int idxHdrEnd=response.indexOf("\r\n\r\n");

  return response.substring(idxHdrEnd + 4);
}



