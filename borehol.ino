#include "config.h"
#include <ESP8266WiFiMulti.h>
#define DEVICE "ESP8266"
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <ESP_Mail_Client.h>

const char wifi_ssid[] = WIFI_SSID;

const char wifi_password[] = WIFI_PASSWORD;
const char influxdb_url[] = INFLUXDB_URL;
const char influx_token[] = INFLUXDB_TOKEN; 
const char influx_org[] = INFLUXDB_ORG;
const char influx_bucket[] = INFLUXDB_BUCKET;

// For emailing
const char smtp_host[] = SMTP_HOST;
const int smtp_port = SMTP_PORT;
const char author_email[] = AUTHOR_EMAIL;
const char author_password[] = AUTHOR_PASSWORD;
const char recipient_email[] = RECIPIENT_EMAIL;
const char recipient_email2[] = RECIPIENT_EMAIL2;

ESP8266WiFiMulti wifiMulti;
SMTPSession smtp;

// For timing influxdb
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

InfluxDBClient client(influxdb_url, influx_org, influx_bucket, influx_token, InfluxDbCloud2CACert);

// Data point
Point sensor("Borehol");

const int numReadings = 5;
const int sensorPin=A0;
const float warningResetLimit = 34.5;
const float warningLimitLow = 33.0;
int readings[numReadings];      // the readings from the analog input
int readIndex = 0;              // the index of the current reading
float total = 0;                  // the running total
float average = 0;
boolean warningSent = false;

void setup() {
  Serial.begin(115200);

for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(wifi_ssid, wifi_password);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // Add tags
  sensor.addTag("device", DEVICE);
  sensor.addTag("SSID", WiFi.SSID());

  // Accurate time is necessary for certificate validation and writing in batches
  // For the fastest time sync find NTP servers in your area: https://www.pool.ntp.org/zone/
  // Syncing progress and the time will be printed to Serial.
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}
void loop() {
  // Clear fields for reusing the point. Tags will remain untouched
  sensor.clearFields();
 
  average = readAndSmooth();
  Serial.println(average);
  //average = total / numReadings;
    
  float waterlevel = ((average*60)/1024) + 2.4;
  sensor.addField("level", waterlevel);
  Serial.println(waterlevel);
  resetWarning(waterlevel);
  checkIfWarningShouldBeSent(waterlevel);

  // Print what are we exactly writing
  //Serial.print("Writing: ");
  Serial.println(waterlevel);
  Serial.println(sensor.toLineProtocol());

  // If no Wifi signal, try to reconnect it
  if ((WiFi.RSSI() == 0) && (wifiMulti.run() != WL_CONNECTED)) {
    Serial.println("Wifi connection lost");
  }

  // Write point
  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  //Serial.println("Wait 30s");
  delay(30000);
}

void checkIfWarningShouldBeSent(float level) {
  
if(level < warningLimitLow && warningSent == false) {
  //sendEmail_sub(level, "lavnivå");
  sendEmail(level, "Lågt nivå i borehol! Hold att på tappingen!");
  warningSent = true;
  }   
}

void resetWarning(float level) {
  if(level > warningResetLimit && warningSent == true) {
    //sendEmail_sub(level, "høgnivå");
    sendEmail(level, "Borehol på normalt nivå att");
  warningSent = false;
  }
}

float readAndSmooth() {
  float average = 0.0;
  for (int i = 0; i < 10; i++) {
    int reading = analogRead(sensorPin);
    average=average+reading;
    //Serial.println(reading);
    delay(10);    
  }
  return average = average / 10;  
}

void sendEmail_sub(float msg, String melding){
  Serial.print("Sender epost! - nivå: ");
  Serial.println(melding);
  Serial.println(msg);
  
}

void sendEmail(float msg, String textMsg){

  char result[8];
  dtostrf(msg, 6, 2, result); 

  ESP_Mail_Session session;

  session.server.host_name = smtp_host;
  session.server.port = smtp_port;
  session.login.email = author_email;
  session.login.password = author_password;
  session.login.user_domain = "";

  SMTP_Message message;

  message.sender.name = "Borehol";
  message.sender.email = author_email;
  message.subject = result;
  message.addRecipient("Eldar", recipient_email);
  message.addRecipient("Melissa", recipient_email2);

  message.text.content = textMsg.c_str();
  message.text.charSet = "us-ascii";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  
  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;
  message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;

  if (!smtp.connect(&session))
    return;

  if (!MailClient.sendMail(&smtp, &message))
    Serial.println("Error sending Email, " + smtp.errorReason());
}
