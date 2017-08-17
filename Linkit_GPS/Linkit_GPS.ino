#include <LGPS.h>
#include <LWiFi.h>
#include <LWiFiClient.h>

#define WIFI_AP "TNSCMDR02"
#define WIFI_PASSWORD "tnscm3133"
#define WIFI_AUTH LWIFI_WPA  // choose from LWIFI_OPEN, LWIFI_WPA, or LWIFI_WEP.
#define SITE_URL "jia.ee.ncku.edu.tw"
#define DATE_URL "api.timezonedb.com"

LWiFiClient client;

gpsSentenceInfoStruct info;
char buff[256];
char json[512];
char date[20];

static unsigned char getComma(unsigned char num, const char *str)
{
  unsigned char i, j = 0;
  int len = strlen(str);
  for (i = 0; i < len; i ++)
  {
    if (str[i] == ',')
      j++;
    if (j == num)
      return i + 1;
  }
  return 0;
}

static double getDoubleNumber(const char *s)
{
  char buf[10];
  unsigned char i;
  double rev;

  i = getComma(1, s);
  i = i - 1;
  strncpy(buf, s, i);
  buf[i] = 0;
  rev = atof(buf);
  return rev;
}

static double getIntNumber(const char *s)
{
  char buf[10];
  unsigned char i;
  double rev;

  i = getComma(1, s);
  i = i - 1;
  strncpy(buf, s, i);
  buf[i] = 0;
  rev = atoi(buf);
  return rev;
}

void parseGPGGA(const char* GPGGAstr)
{
  /* Refer to http://www.gpsinformation.org/dale/nmea.htm#GGA
     Sample data: $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
     Where:
      GGA          Global Positioning System Fix Data
      123519       Fix taken at 12:35:19 UTC
      4807.038,N   Latitude 48 deg 07.038' N
      01131.000,E  Longitude 11 deg 31.000' E
      1            Fix quality: 0 = invalid
                                1 = GPS fix (SPS)
                                2 = DGPS fix
                                3 = PPS fix
                                4 = Real Time Kinematic
                                5 = Float RTK
                                6 = estimated (dead reckoning) (2.3 feature)
                                7 = Manual input mode
                                8 = Simulation mode
      08           Number of satellites being tracked
      0.9          Horizontal dilution of position
      545.4,M      Altitude, Meters, above mean sea level
      46.9,M       Height of geoid (mean sea level) above WGS84
                       ellipsoid
      (empty field) time in seconds since last DGPS update
      (empty field) DGPS station ID number
   *  *47          the checksum data, always begins with *
  */
  double latitude;
  double longitude;
  int tmp, hour, minute, second, num ;
  if (GPGGAstr[0] == '$')
  {
    tmp = getComma(1, GPGGAstr);
    hour     = (GPGGAstr[tmp + 0] - '0') * 10 + (GPGGAstr[tmp + 1] - '0');
    minute   = (GPGGAstr[tmp + 2] - '0') * 10 + (GPGGAstr[tmp + 3] - '0');
    second    = (GPGGAstr[tmp + 4] - '0') * 10 + (GPGGAstr[tmp + 5] - '0');

    sprintf(buff, "UTC timer %2d-%2d-%2d", hour, minute, second);
    Serial.println(buff);

    tmp = getComma(2, GPGGAstr);
    latitude = getDoubleNumber(&GPGGAstr[tmp]);
    tmp = getComma(4, GPGGAstr);
    longitude = getDoubleNumber(&GPGGAstr[tmp]);
    sprintf(buff, "latitude = %10.4f, longitude = %10.4f", latitude, longitude);
    Serial.println(buff);

    tmp = getComma(7, GPGGAstr);
    num = getIntNumber(&GPGGAstr[tmp]);
    sprintf(buff, "satellites number = %d", num);
    Serial.println(buff);
    if (num == 0) {
      get_date();
    }
    String jsonStr = buildJson(latitude, longitude, hour, minute, second, num);
    post_data(jsonStr);
    Serial.println(jsonStr);
  }
  else
  {
    Serial.println("Not get data");
  }
}

String buildJson(double latitude, double longitude, int hour, int minute, int second, int num) {
  double d = 100.0;
  String data = "{";
  data += "\n";
  data += "\"latitude\" : ";
  data += latitude;
  data += ",";
  data += "\n";
  data += "\"longitude\" : ";
  data += longitude;
  data += ",";
  data += "\n";
  data += "\"date\" : \"";
  for (int i = 0; i < 10; i++) {
    data += date[i];
  }
  if (num == 0) {
    data += "T";
    for (int i = 11; i < 19; i++) {
      data += date[i];
    }
    data += "\"";
    data += "\n";
    data += "}";
  }
  else {
    data += "T";
    data += hour;
    data += ":";
    data += minute;
    data += ":";
    data += second;
    data += "\"";
    data += "\n";
    data += "}";
  }
  return data;
}

void post_data(String jsonStr) {
  if (!client.connect(SITE_URL, 80)) {
    Serial.println("Fail");
    return;
  }
  char Json[jsonStr.length()];
  jsonStr.toCharArray(Json, jsonStr.length());
  Serial.println("send HTTP POST request");
  client.connect(SITE_URL, 80);
  client.println(F("POST /locate HTTP/1.1"));
  client.println("Host: " SITE_URL);
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(jsonStr.length());
  client.println();
  client.print(jsonStr);
  Serial.println("Done");
}

boolean disconnectedMsg = false;

void get_date() {
  if (!client.connect(DATE_URL, 80)) {
    Serial.println("Fail to get date");
    return;
  }
  Serial.println("send HTTP GET DATE request");
  client.connect(DATE_URL, 80);
  client.println("GET /v2/get-time-zone?key=AHFTG5ZX72LJ&format=json&by=zone&zone=Asia/Taipei HTTP/1.1");
  client.println("Host: " DATE_URL);
  client.println("Connection: close");
  client.println();
  // waiting for server response
  //Serial.println("waiting HTTP response:");
  while (!client.available())
  {
    delay(100);
  }
  int counter = 0;
  while (client)
  {
    int v = client.read();
    if (v != -1)
    {
      //Serial.print((char)v);
      json[counter] = static_cast<char>(v);
      counter++;
    }
    else
    {
      Serial.println("no more content, disconnect");
      client.stop();
      while (1)
      {
        delay(1);
      }
    }
  }
  if (!disconnectedMsg)
  {
    for (int i = 422; i < 441; i++)
    {
      //Serial.print(json[i]);
      date[i - 422] = json[i];
    }
    //Serial.println("disconnected by server");
    disconnectedMsg = true;
  }
  Serial.println("date:");
  for (int i = 0; i < 20; i++)
  {
    Serial.print(date[i]);
  }
  Serial.println();
  client.stop();
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  LWiFi.begin();
  Serial.println("Connecting to AP");
  while (0 == LWiFi.connect(WIFI_AP, LWiFiLoginInfo(WIFI_AUTH, WIFI_PASSWORD)))
  {
    delay(1000);
  }
  Serial.println("Success!");
  Serial.println("Getting date:");
  get_date();
  /*Serial.println("Connecting to WebSite");
    while (0 == client.connect(SITE_URL, 80))
    {
    Serial.println("Re-Connecting to WebSite");
    delay(1000);
    }
    Serial.println("Success!");*/

  LGPS.powerOn();
  Serial.println("LGPS Power on, and waiting ...");
  delay(3000);
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("LGPS loop");
  LGPS.getData(&info);
  Serial.println((char*)info.GPGGA);
  parseGPGGA((const char*)info.GPGGA);
  delay(10000);
}
