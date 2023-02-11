#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#ifndef STASSID
#define STASSID "<SSID>"
#define STAPSK "<PWD>"
#endif

// BUILTIN_LED 2
#define PIN_ZC 4
#define PIN_DM 5

#define HB_INTERVAL 1000*10

unsigned long currentTime=0;

union ArrayToInteger {
  uint8_t array[4];
  uint32_t integer;
};

const char* ssid = STASSID;
const char* password = STAPSK;

const char* host = "192.168.15.33";
const uint16_t port = 7890;

String ID = "L02";

ESP8266WiFiMulti WiFiMulti;

WiFiClient client;

int prefix_sz = 4;

int cursor = 0;
int buff_index = 0;
byte buff[2028];

volatile long lux = 0;
long target = 0;

ICACHE_RAM_ATTR void zeroCross() {
  if(lux == 80){
    digitalWrite(PIN_DM, HIGH);
  }else if(lux == 0){
    digitalWrite(PIN_DM, LOW);
  }else{
    long t1 = (8200L * (100L - lux) / 100L);
    delayMicroseconds(t1);
    digitalWrite(PIN_DM, HIGH);
    delayMicroseconds(20);
    digitalWrite(PIN_DM, LOW);
  }
}

void login(){

  String header = "{ \"transaction\":\"LOGIN\" }";

  ArrayToInteger msg_sz;
  ArrayToInteger header_sz;

  header_sz.integer = header.length();
  msg_sz.integer = header_sz.integer + ID.length() + prefix_sz + 8;

  client.write("HNIO");
  client.write(msg_sz.array[0]);
  client.write(msg_sz.array[1]);
  client.write(msg_sz.array[2]);
  client.write(msg_sz.array[3]);

  client.write(header_sz.array[0]);
  client.write(header_sz.array[1]);
  client.write(header_sz.array[2]);
  client.write(header_sz.array[3]);

  client.write(header.c_str());
  client.write(ID.c_str());
}

void handleHeartBeat(){
  String header = "{ \"transaction\":\"HB\" }";

  ArrayToInteger msg_sz;
  ArrayToInteger header_sz;

  header_sz.integer = header.length();
  msg_sz.integer = header_sz.integer + ID.length() + prefix_sz + 8;

  client.write("HNIO");
  client.write(msg_sz.array[0]);
  client.write(msg_sz.array[1]);
  client.write(msg_sz.array[2]);
  client.write(msg_sz.array[3]);

  client.write(header_sz.array[0]);
  client.write(header_sz.array[1]);
  client.write(header_sz.array[2]);
  client.write(header_sz.array[3]);

  client.write(header.c_str());
  client.write(ID.c_str());

  currentTime = millis();
}

void handleLux(byte data[], int sz){
  Serial.print("LUX: ");

  String val = "";

  for(int i =0; i < sz; i++){
    val = val + (char) data[i];
  }

  Serial.println(val);
  Serial.println();

  target = val.toInt();
}

void processMessage(){

  int prefix_pos = -1;

  if(buff_index - 4 < 0){
    Serial.println("buff too small");
    return;
  }

  for(byte i = cursor; i < buff_index - 4; i++){

    if(buff[i] == (byte) 'H' && buff[i + 1] == (byte) 'N' && buff[i + 2] == (byte) 'I' && buff[i + 3] == (byte) 'O'){
      prefix_pos = i;
      break;
    }
  }

  if(prefix_pos == -1){
    Serial.println("not found");
    return;
  }

  cursor = cursor + prefix_pos + prefix_sz;

  ArrayToInteger ati_msg_sz = {
    buff[cursor + 0],
    buff[cursor + 1],
    buff[cursor + 2],
    buff[cursor + 3]
  };

  cursor = cursor + 4;

  ArrayToInteger ati_header_sz = {
    buff[cursor + 0],
    buff[cursor + 1],
    buff[cursor + 2],
    buff[cursor + 3]
  };

  cursor = cursor + 4;

  int msg_sz = ati_msg_sz.integer;
  int header_sz = ati_header_sz.integer;

  if(buff_index < msg_sz){
    Serial.println("Incomplete message, will wait for more...");
    return;
  }

  String header = "";

  for(int i = cursor; i < cursor + header_sz; i++){
    header = header + (char) buff[i];
  }

  cursor = cursor + header_sz;

  byte data[msg_sz - header_sz - prefix_sz - 8];

  for(int i = cursor, j = 0; j < sizeof(data); i++, j++){
    data[j] = buff[i];
  }

  if(header.indexOf("HB") > -1){
    handleHeartBeat();
  }else if(header.indexOf("LUX") > -1){
    handleLux(data, sizeof(data));
  }

  cursor = cursor + sizeof(data);

  Serial.println(buff_index);
  Serial.println(msg_sz);
  Serial.println(cursor);

  //limpa todo o buffer de entrada
  if(buff_index == cursor){
    memset(buff, 0, sizeof(buff));
    buff_index = 0;
    cursor = 0;
    Serial.println("buff cleared");
  }else{
    Serial.print("will reprocess ");
    Serial.println(buff_index);
    processMessage();
  }
}

void setup() {

  pinMode(PIN_ZC, INPUT);
  pinMode(PIN_DM, OUTPUT);
  pinMode(BUILTIN_LED, OUTPUT);

  digitalWrite(BUILTIN_LED, LOW);

  Serial.begin(9600);

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(ssid, password);

  Serial.println();
  Serial.println();
  Serial.print("Wait for WiFi... ");

  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  delay(500);

  attachInterrupt(digitalPinToInterrupt(PIN_ZC), zeroCross, RISING);
  Serial.println("Interrupt attached");
}


void loop() {

  if(!client.connected()){

    digitalWrite(BUILTIN_LED, LOW);

    Serial.print("connecting to ");
    Serial.print(host);
    Serial.print(':');
    Serial.println(port);

    if (!client.connect(host, port)) {
      Serial.println("connection failed");
      Serial.println("wait 5 sec...");
      delay(5000);
      return;
    }

    Serial.println("Connected to server!");
    login();
    digitalWrite(BUILTIN_LED, HIGH);

    currentTime = millis();

    delay(3000);
  }else{

    if(currentTime + HB_INTERVAL < millis()){
      handleHeartBeat();
    }

    if (client.available()) {
      byte c = client.read();
      buff[buff_index] = c;
      buff_index = buff_index + 1;
    }else{
      if(buff_index > 0){
        Serial.print("will process ");
        Serial.println(buff_index);
        processMessage();
        delay(100);
      }
    }

    if(lux != target){

      for (; lux < target; lux++) {
        delay(50);
      }

      for (; lux > target; lux--) {
        delay(20);
      }
    }
  }

}
