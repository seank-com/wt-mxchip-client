#include "AZ3166WiFi.h"
#include "Arduino.h"
#include "WebSocketClient2.h"
#include "AudioClassV2.h"
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
  Playing
};

RGB_LED rgbLED;

static STATUS status = Idle;

static bool hasWifi;
static bool isWsConnected;

static char webSocketServerUrl[] = "ws://192.168.1.101:8686/";
static WebSocketClient *wsClient = NULL;

static AudioClass& Audio = AudioClass::getInstance();
static int wavFileSize;
static char *waveFile = NULL;

static void initWiFi()
{
  Screen.clean();
  Screen.print(2, "Connecting...");

  if (WiFi.begin() == WL_CONNECTED)
  {
    IPAddress ip = WiFi.localIP();
    Screen.print(1, ip.get_address());
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

static void DoIdle()
{
  //memset(waveFile, 0, AUDIO_BUFFER_SIZE);

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
    WebSocketReceiveResult *recvResult = wsClient->receive(waveFile, AUDIO_BUFFER_SIZE);
    if (recvResult != NULL)
    {
      if (recvResult->length > 0 && recvResult->isEndOfMessage)
      {
        // Enter the Play mode
        Screen.clean();
        Screen.print(0, "Playing...");
        rgbLED.setColor(0,255,0);

        Audio.startPlay(waveFile, recvResult->length);
        status = Playing;
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
    wavFileSize = Audio.convertToMono(waveFile, wavFileSize, BITS_PER_SAMPLE);
    if (wavFileSize <= 0)
    {
      Serial.println("ConvertToMono failed! ");
      EnterIdleState();
    }
    else
    {
      Screen.clean();
      Screen.print(0, "Processing...");
      Screen.print(1, "Sending...");

      status = Sending;
    }
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
    isWsConnected = wsClient->connected();
    if (!isWsConnected)
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
      case Playing:
        DoPlaying();
        break;
    }
  }

  delay(LOOP_DELAY);
}
