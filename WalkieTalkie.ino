#include "AudioClassV2.h"
#include "AZ3166WiFi.h"
#include "Arduino.h"
#include "WebSocketClient2.h"
#include "RGB_LED.h"

#define MAX_RECORD_DURATION 5
#define SAMPLE_RATE         8000
#define BITS_PER_SAMPLE     16
#define BYTES_PER_SAMPLE    2
#define SAMPLE_CHANNELS     2
#define BYTES_PER_SECOND    (SAMPLE_RATE * (BYTES_PER_SAMPLE * SAMPLE_CHANNELS))
#define AUDIO_BUFFER_SIZE   ((BYTES_PER_SECOND * MAX_RECORD_DURATION) + 45 + 1)

#define LOOP_DELAY          100

enum STATUS
{
  Idle,
  Recording,
  Recorded,
  Sending,
  Receiving,
  Playing
};

RGB_LED rgbLED;

static STATUS status = Idle;

static bool hasWifi;
static bool isWsConnected;

static char webSocketServerUrl[] = "ws://192.168.1.6:8686/";
static WebSocketClient *wsClient = NULL;

static AudioClass& Audio = AudioClass::getInstance();
static int wavFileSize;
static int offset;
static char *waveFile = NULL;

static void initWiFi()
{
  Screen.clean();
  Screen.print(2, "Connecting...");

  if (WiFi.begin() == WL_CONNECTED)
  {
    unsigned char mac[WL_MAC_ADDR_LENGTH];
    WiFi.macAddress(mac);

    char buf[250];
    sprintf(buf, "%x:%x:%x:%x:%x:%x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.println(buf);

    IPAddress ip = WiFi.localIP();
    Screen.print(1, ip.get_address());
    Serial.println(ip.get_address());

    hasWifi = true;
    Screen.print(2, "Connected");
  }
  else
  {
    Screen.clean();
    Screen.print(0, "No WiFi");
    Screen.print(1,"Enter AP Mode");
    Screen.print(2, "to config");
  }
}

static bool connectWebSocket()
{
  Screen.clean();
  Screen.print(0, "Connect to WS...");
  
  wsClient = new WebSocketClient(webSocketServerUrl);
  isWsConnected = wsClient->connect();

  if (isWsConnected)
  {
    Screen.print(1, "connect WS successful.");
    Serial.println("WebSocket connect successful.");
  }
  else
  {
    Screen.print(1, "Connect WS failed.");
    Serial.print("WebSocket connection failed, isWsConnected: ");
    Serial.println(isWsConnected);
  }
  return isWsConnected;
}

static void EnterIdleState()
{
  status = Idle;
  Screen.clean();
  Screen.print(0, "Hold B to talk");
  rgbLED.turnOff();
}

static void EnterPlayingState(int size)
{
  // Enter the Play mode
  Screen.clean();
  Screen.print(0, "Playing...");
  rgbLED.setColor(0,255,0);

  Audio.startPlay(waveFile, size);
  status = Playing;
}

static void DoIdle()
{
  if (digitalRead(USER_BUTTON_B) == LOW)
  {
    // Enter the Recording mode
    Screen.clean();
    Screen.print(0, "Recording...");
    Screen.print(1, "Release to send");
    rgbLED.setColor(255,0,0);

    Audio.format(SAMPLE_RATE, BITS_PER_SAMPLE);
    Audio.startRecord(waveFile, AUDIO_BUFFER_SIZE);
    status = Recording;
  }
  else
  {
    WebSocketReceiveResult *recvResult = wsClient->receive(waveFile, AUDIO_BUFFER_SIZE, 1000);
    if (recvResult != NULL)
    {
      if (recvResult->length > 0)
      {
        Serial.print("Received Message, length:");
        Serial.print(recvResult->length);
        Serial.print(" type:");
        Serial.print((recvResult->messageType == WS_Message_Binary) ? "binary" : "text");
        Serial.print(" final:");
        Serial.println(recvResult->isEndOfMessage);

        if (recvResult->isEndOfMessage)
        {
          int len = recvResult->length;
          if (recvResult->length > AUDIO_BUFFER_SIZE)
          {
            len = AUDIO_BUFFER_SIZE;
          } 
          EnterPlayingState(len);
        }
        else
        {
          // Enter the Receiving mode
          Screen.clean();
          Screen.print(0, "Receiving...");
          rgbLED.setColor(0,0,255);
          offset = recvResult->length;
          status = Receiving;
        }
      }
    }
  }
}

static void DoRecording()
{
  if (digitalRead(USER_BUTTON_B) == HIGH)
  {
    Audio.stop();
    status = Recorded;
  }
}

static void DoRecorded()
{
  wavFileSize = Audio.getCurrentSize();
  if (wavFileSize > 0)
  {
    Screen.clean();
    Screen.print(0, "Processing...");
    Screen.print(1, "Sending...");

    status = Sending;
  }
  else
  {
    Serial.println("No Data Recorded! ");
    EnterIdleState();
  }
}

static void DoSending()
{
  int res = wsClient->send(waveFile, wavFileSize, WS_Message_Binary, true);
  if (res < 0)
  {
    Serial.println("Sending failed.");
  }
  EnterIdleState();
}

static void DoReceiving()
{
  int size = AUDIO_BUFFER_SIZE - offset;
  if (offset > AUDIO_BUFFER_SIZE)
  {
    offset = AUDIO_BUFFER_SIZE - 1;
    size = 1;
  }

  WebSocketReceiveResult *recvResult = wsClient->receive(waveFile + offset, size);
  if (recvResult != NULL)
  {
    Serial.print("Received Message, length:");
    Serial.print(recvResult->length);
    Serial.print(" type:");
    Serial.print((recvResult->messageType == WS_Message_Binary) ? "binary" : "text");
    Serial.print(" final:");
    Serial.println(recvResult->isEndOfMessage);

    if (recvResult->length > 0 && recvResult->isEndOfMessage)
    {
      EnterPlayingState(offset + recvResult->length);
    }
    else if (recvResult->length > 0)
    {
      offset += recvResult->length;
    }
  }
}

static void DoPlaying()
{
  if (Audio.getAudioState() != AUDIO_STATE_PLAYING)
  {
    EnterIdleState();
  }
}

void setup()
{
  hasWifi = false;
  isWsConnected = false;

  Screen.init();
  Serial.begin(115200);

  initWiFi();
  if (hasWifi)
  {
    waveFile = (char *)malloc(AUDIO_BUFFER_SIZE);
    if (waveFile != NULL)
    {
      if (connectWebSocket())
      {
        EnterIdleState();
      }
    }
  }

  pinMode(USER_BUTTON_B, INPUT);
}

void loop()
{
  if (hasWifi)
  {
    if (wsClient == NULL || (isWsConnected = wsClient->connected()) == false)
    {
      Screen.clean();
      Screen.print("Reboot....");
      SystemReboot();
      return;
    }

    switch (status)
    {
      case Idle:
        DoIdle();
        break;
      case Recording:
        DoRecording();
        break;
      case Recorded:
        DoRecorded();
        break;
      case Sending:
        DoSending();
        break;
      case Receiving:
        DoReceiving();
        break;
      case Playing:
        DoPlaying();
        break;
    }
  }

  delay(LOOP_DELAY);
}
