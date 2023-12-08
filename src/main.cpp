#include <M5Stack.h>
#include "lw_packets.h"

// device info
//const char *devAddr = "00000000";
// const char *nwkSKey = "11111111111111111111111111111111";
// const char *appSKey = "11111111111111111111111111111111";
const char *devAddr = "<devAddr>";
const char *nwkSKey = "<nwkSKey>";
const char *appSKey = "<appSKey>";

// lorawan-packets
Lorawan_fcnt_t fcnt;
Lorawan_devCfg_t devCfg;
lwPackets_api_t api;
lwPackets_state_t state;

uint32_t DevAddr = 0;
uint8_t NwkSKey[16] = {0};
uint8_t AppSKey[16] = {0};

char pkt_buffer[128];
uint8_t payload[64];
int payload_len = 0;

// serial buffer
static bool is_exist = false;
static char recv_buf[512];
int recv_index = 0;

/*********************************************************/
static void debugLog(const char *format, ...)
{
  char buffer[100];
  va_list args;
  va_start(args, format);
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  if (len > 100 - 1)
  {
    strcpy(&buffer[100 - 5], "...\n");
  }
  va_end(args);
  printf("%s", buffer);
  return;
}

/*********************************************************/
const char *hex2bin(const char *hex, char *bin, size_t binsize)
{
  if (hex && bin)
  {
    for (; binsize && isxdigit(hex[0]) && isxdigit(hex[1]); hex += 2, bin += 1, binsize -= 1)
    {
      int r = sscanf(hex, "%2hhx", bin);
      if (r != 1)
      {
        break;
      }
    }
  }
  return hex;
}

/*********************************************************/
void hexdump(const void *mem, uint32_t len, uint8_t cols = 16)
{
  const uint8_t *src = (const uint8_t *)mem;
  Serial.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
  for (uint32_t i = 0; i < len; i++)
  {
    if (i % cols == 0)
    {
      Serial.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
    }
    Serial.printf("%02X ", *src);
    src++;
  }
  Serial.printf("\n");
}


/*********************************************************/
static int at_send_check_response(char *p_ack, int timeout_ms, char *p_cmd, ...)
{
  int ch = 0;
  int index = 0;
  int startMillis = 0;
  va_list args;
  memset(recv_buf, 0, sizeof(recv_buf));
  va_start(args, p_cmd);
  Serial1.printf(p_cmd, args);
  Serial.printf(p_cmd, args);
  va_end(args);
  delay(200);
  startMillis = millis();

  if (p_ack == NULL)
  {
    return 0;
  }

  do
  {
    while (Serial1.available() > 0)
    {
      ch = Serial1.read();
      recv_buf[index++] = ch;
      Serial.print((char)ch);
      delay(2);
    }

    if (strstr(recv_buf, p_ack) != NULL)
    {
      return 1;
    }

  } while (millis() - startMillis < timeout_ms);
  return 0;
}

/*********************************************************/
void display_payload_0x06()
{
  Serial.println();
  Serial.printf("id: %02x\n", payload[0]);
  Serial.printf("eventStatus: %06x\n", payload[1] << 16 | payload[2] << 8 | payload[3]);
  Serial.printf("motionSegmentNumber: %d\n", payload[4]);
  Serial.printf("utcTime: %d\n", long(payload[5] << 24 | payload[6] << 16 | payload[7] << 8 | payload[8]));
  Serial.printf("longitude: %.6lf\n", (double)long(payload[9] << 24 | payload[10] << 16 | payload[11] << 8 | payload[12]) / 1000000);
  Serial.printf("latitude: %.6lf\n", (double)long(payload[13] << 24 | payload[14] << 16 | payload[15] << 8 | payload[16]) / 1000000);
  Serial.printf("temperature: %.1f\n", (float)(payload[17] << 8 | payload[18]) / 10);
  Serial.printf("light: %d\n", payload[19] << 8 | payload[20]);
  Serial.printf("batteryLevel: %d\n", payload[21]);

  M5.Lcd.fillRect(0, 48, 320, 240 - 48, TFT_BLACK);
  M5.Lcd.setCursor(0, 48);
  M5.Lcd.printf("utcTime: %d\n", long(payload[5] << 24 | payload[6] << 16 | payload[7] << 8 | payload[8]));
  M5.Lcd.printf("longitude: %.6lf\n", (double)long(payload[9] << 24 | payload[10] << 16 | payload[11] << 8 | payload[12]) / 1000000);
  M5.Lcd.printf("latitude: %.6lf\n", (double)long(payload[13] << 24 | payload[14] << 16 | payload[15] << 8 | payload[16]) / 1000000);
  M5.Lcd.printf("temperature: %.1f\n", (float)(payload[17] << 8 | payload[18]) / 10);
  M5.Lcd.printf("light: %d\n", payload[19] << 8 | payload[20]);
  M5.Lcd.printf("batteryLevel: %d\n", payload[21]);
}

void display_payload_0x11()
{
  Serial.println();
  Serial.printf("id: %02x\n", payload[0]);
  Serial.printf("unknown: %d\n", payload[1]);
  Serial.printf("eventStatus: %06x\n", payload[2] << 16 | payload[3] << 8 | payload[4]);
  Serial.printf("utcTime: %d\n", long(payload[5] << 24 | payload[6] << 16 | payload[7] << 8 | payload[8]));
  Serial.printf("temperature: %.1f\n", (float)(payload[9] << 8 | payload[10]) / 10);
  Serial.printf("light: %d\n", payload[11] << 8 | payload[12]);
  Serial.printf("batteryLevel: %d\n", payload[13]);

  M5.Lcd.fillRect(0, 48, 320, 240 - 48, TFT_BLACK);
  M5.Lcd.setCursor(0, 48);
  M5.Lcd.printf("utcTime: %d\n", long(payload[5] << 24 | payload[6] << 16 | payload[7] << 8 | payload[8]));
  M5.Lcd.printf("longitude: \n");
  M5.Lcd.printf("latitude: \n");
  M5.Lcd.printf("temperature: %.1f\n", (float)(payload[9] << 8 | payload[10]) / 10);
  M5.Lcd.printf("light: %d\n", payload[11] << 8 | payload[12]);
  M5.Lcd.printf("batteryLevel: %d\n", payload[13]);
}

/*********************************************************/
int process_frame()
{
  String str_tmp;
  int str_len = 0;
  int payload_len = 0;
  char *p_start = NULL;
  int rssi = 0;
  int snr = 0;

  String str = String(recv_buf);

  if (str.startsWith("+TEST: RX \""))
  {
    str_tmp = str.substring(11, str.length());
    str_tmp = str_tmp.substring(0, str_tmp.indexOf("\""));
    str_len = str_tmp.length();
    hex2bin(str_tmp.c_str(), pkt_buffer, sizeof(pkt_buffer));

    lorawan_packet_t *packet = LoRaWAN_UnmarshalPacketFor((const uint8_t *)pkt_buffer, str_len / 2, 0);
    if (packet == NULL)
    {
      payload[0] = 0;
      LoRaWAN_DeletePacket(packet);
    }
    else
    {
      lorawan_logLoraPacket(packet, true);
      payload_len = packet->BODY.MACPayload.payloadLength;
      memcpy(payload, packet->pPayload, packet->BODY.MACPayload.payloadLength);
      LoRaWAN_DeletePacket(packet);

      if (payload[0] == 0x06)
      {
        display_payload_0x06();
      }
      else if (payload[0] == 0x11)
      {
        display_payload_0x11();
      }
    }
  }

  p_start = strstr(recv_buf, "RSSI:");
  if (p_start && (1 == sscanf(p_start, "RSSI:%d,", &rssi)))
  {
    M5.Lcd.setCursor(0, 16);
    M5.Lcd.print("                ");
    M5.Lcd.setCursor(16, 16);
    M5.Lcd.print("rssi:");
    M5.Lcd.print(rssi);
  }

  p_start = strstr(recv_buf, "SNR:");
  if (p_start && (1 == sscanf(p_start, "SNR:%d", &snr)))
  {
    M5.Lcd.setCursor(0, 32);
    M5.Lcd.print("                ");
    M5.Lcd.setCursor(16, 32);
    M5.Lcd.print("snr :");
    M5.Lcd.print(snr);
  }

  return 1;
}

/*********************************************************/
void setup(void)
{
  M5.begin();
  Serial1.begin(9600, SERIAL_8N1, 22, 21);

  Serial.print("SenseCAP T-1000 Viewer!\r\n");
  M5.Lcd.setTextFont(2);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print("SenseCAP T-1000 Viewer");
  M5.Lcd.setCursor(5, 0);

  api.LogError = debugLog;
  api.LogInfo = debugLog;

  fcnt.AFCntDwn = 0;
  fcnt.FCntUp = 0;
  fcnt.NFCntDwn = 0;

  devCfg.LorawanVersion = LORAWAN_VERSION_1_0;
  devCfg.DevAddr = DevAddr;
  char *p = (char *)hex2bin((const char *)appSKey, (char *)AppSKey, sizeof(AppSKey));
  for (int i = 0; i < 16; i++)
  {
    devCfg.AppSKey[i] = AppSKey[i];
    devCfg.FNwkSIntKey[i] = AppSKey[i];
    devCfg.NwkSEncKey[i] = AppSKey[i];
    devCfg.SNwkSIntKey[i] = AppSKey[i];
  }
  state.pDevCfg = &devCfg;
  state.pFCntCtrl = &fcnt;

  LoRaWAN_PacketsUtil_Init(api, state);


  if (at_send_check_response("+AT: OK", 100, "AT\r\n"))
  {
    is_exist = true;
    at_send_check_response("+MODE: TEST", 1000, "AT+MODE=TEST\r\n");
    at_send_check_response("+TEST: RFCFG", 1000, "AT+TEST=RFCFG,923.400,SF9,125,8,8,14,ON,OFF,ON\r\n");
    at_send_check_response("+TEST: RXLRPKT", 1000, "AT+TEST=RXLRPKT\r\n");
  }
  else
  {
    is_exist = false;
    Serial.print("No E5 module found.\r\n");
    M5.Lcd.setCursor(0, 1);
    M5.Lcd.print("unfound E5 !");
  }

  recv_index = 0;
}

/*********************************************************/
void loop(void)
{
  M5.update();

  while (Serial1.available())
  {
    char ch = Serial1.read();
    recv_buf[recv_index++] = ch;
    if (ch == '\n')
    {
      recv_buf[recv_index++] = 0;
      Serial.println();
      Serial.print(recv_buf);
      process_frame();
      recv_index = 0;
    }
  }

  delay(1);
}
