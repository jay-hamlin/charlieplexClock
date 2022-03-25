/*  Filament Clock Arduino sketch , February 24, 2022
*   Jay F. Hamlin.
*
*   This sketch is for the Adafruit HUZZAH32 ESP32 Feather board.
*        HUZZAH32 PN= 3405
*
*/

/*  circuit details
 *  GPIO OUTPUTS USED
 *    A0 = LED1 output
 *    A1 = LED2 output
 *    A5, IO4 LATCH output
 *       
 *  SPI
 *    SCK
 *    MOSI
 *    MISO   
 *    
 */

 /* 
  *  Adafruit time service for thee ESP32
  *  https://learn.adafruit.com/adafruit-metro-esp32-s2/getting-the-date-time?gclid=CjwKCAiA9tyQBhAIEiwA6tdCrDQj2zNDamuGvMNLzVigDuVxWU-LgXNB2_vewJ9btt1L-pL_j_nSBBoC1w4QAvD_BwE
  *  
  * 
  */

#include <SPI.h>            // SPI
#include <HardwareSerial.h> // UARTS
#include <WiFi.h>
#include "time.h"

// build flags
#define   REVERSE_VIEW      0
#define   INCLUDE_SMTP_MAIL 0

#if INCLUDE_SMTP_MAIL
#include <ESP_Mail_Client.h>
#endif

// Clock pin definitions
#define LATCH_Pin 4
#define led1Pin  25
#define led2Pin  26

// button pins
#define buttonSET 14
#define buttonPLUS 32
#define buttonMINUS 15
#define buttonHOLD 33
// DIP switch mode
#define dipswMODE0  16
#define dipswMODE1  17
#define dipswMODE2  21

#define SET_BUTTON_STATE    (1<<0)
#define PLUS_BUTTON_STATE   (1<<1)
#define MINUS_BUTTON_STATE  (1<<2)
#define HOLD_BUTTON_STATE   (1<<3)
#define MODE_0_SW_STATE     (1<<4)
#define MODE_1_SW_STATE     (1<<5)
#define MODE_2_SW_STATE     (1<<6)

// task state enum
enum taskStates {
    INITIAL_STATE =   0,
    IDLING_STATE,
    INVALID_TIME_STATE,
    GET_NTP_TIME_CONNECT_STATE,
    GET_NTP_TIME_CONNECTING_STATE,
    GET_NTP_TIME_COMMUNICATE_STATE,
    GET_NTP_TIME_DISCONNECT_STATE
};

// type Alias
#define uInt8 unsigned char

// local function prototypes
void  SetTaskState(char newState);
void  BlinkLEDTask(void);
void  ButtonsTask(void);
void  DisplayScan(void);
void  WriteDisplayNumbers(char hh,char mm, char ss);
void  WriteDisplaySegments(char d0,char d1,char d2,char d3,char d4,char d5);
void  DisplayTime(void);
uInt8 ReadButtons(uInt8 *modes);
void  SendEmail(char *msg1, char *msg2);

char  ConnectToWifi(void);
void  SerialPrintLocalTime(void);

// some global variables
long  loopCounter;
char  taskState;
word  prevTaskState;
char  doDisplayUpdate;
char  colonState;
char  validTimeOfDay;
char  wifiTries;
uInt8 segmentBitMap[6];   // we have 6 7 segment digits
char  segmentNumber;      // the segment which is currently being scanned


// Wifi Credentials
const char* ssid     = "my wifi ssid";
const char* password = "my wifi password";

// NTP time server url
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = (-8*60*60);
const int   daylightOffset_sec = (60*60);

// SMTP Mail includes
#if INCLUDE_SMTP_MAIL
    // https://github.com/mobizt/ESP-Mail-Client

#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT esp_mail_smtp_port_587 //port 465 is not available for Outlook.com

/* The log in credentials */
#define AUTHOR_EMAIL "mygmail@gmail.com"
#define AUTHOR_PASSWORD "mygmailpassword"

/* The SMTP Session object used for Email sending */
SMTPSession smtp;

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status);

const char rootCACert[] PROGMEM = "-----BEGIN CERTIFICATE-----\n"
                                  "-----END CERTIFICATE-----\n";

#endif

// We define a few vertical and horizontal bars for the display.
// pass thesee constants to WriteDisplaySegments() to show them
enum segmentBars {
    ALL_SEGMENTS_OFF   = 0x00,
    VERTICAL_BAR_LEFT  = 0x30,
    VERTICAL_BAR_RIGHT = 0x06,
    HORIZONTAL_BAR_TOP = 0x01,
    HORIZONTAL_BAR_MID = 0x40,
    HORIZONTAL_BAR_BOT = 0x08
};


// this is the character map.  '1' is segment ON
// sevenSegmentEncoder is the classic 7 segment display encoding
#if REVERSE_VIEW
uInt8    sevenSegmentEncoder[16] = {
    0x3F, // 0
    0x30, // 1
    0x6D, // 2
    0x79, // 3
    0x72, // 4
    0x5B, // 5
    0x5F, // 6
    0x31, // 7
    0x7F, // 8
    0x73, // 9
    0x77, // A
    0x5E, // B
    0x0F, // C
    0xEC, // D
    0x4F, // E
    0x47,  // F
};
#else
uInt8    sevenSegmentEncoder[16] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x67, // 9
    0x77, // A
    0x7C, // B
    0x39, // C
    0x5E, // D
    0x79, // E
    0x71,  // F
    };

#endif

/*
     segmentStatesBytes is the Charlieplexed byte value to send out
     the segment number is the index into the arraay.
       
*/
#if 0
// this was for the Arduino prototype
unsigned char    segmentStatesBytes[8]={
//         pin 0 1 2 3 4 5 6 7
      0xEC, // 1,1,1,0,1,1,0,0    //segment 0x01, 'A' + pin 5, - pin 4
      0x10, // 0,0,0,1,0,0,0,0    //segment 0x02, 'B' + pin 4, - pin 3
      0x18, // 0,0,0,1,1,0,0,0    //segment 0x04, 'C' + pin 3, - pin 2
      0x80, // 1,0,0,0,0,0,0,0    //segment 0x08, 'D' + pin 2, - pin 7
      0x7C, // 0,1,1,1,1,1,0,0    //segment 0x10, 'E' + pin 6, - pin 7
      0x3C, // 0,0,1,1,1,1,0,0    //segment 0x20, 'F' + pin 5, - pin 6
      0xC4, // 1,1,0,0,0,1,0,0    //segment 0x40, 'G' + pin 6, - pin 3
      0xFC  // 1,1,1,1,1,1,0,0    // all off
};
#else
// this is for the pcb version with shift registers
unsigned char    segmentStatesBytes[8]={
//         pin 0 1 2 3 4 5 6 7
//         bit 0 1 7 6 5 4 3 2
      0xDC, // 1,1,0,1,1,1,0,0    //segment 0x01, 'A' + pin 5, - pin 4
      0x20, // 0,0,1,0,0,0,0,0    //segment 0x02, 'B' + pin 4, - pin 3
      0x60, // 0,1,1,0,0,0,0,0   //segment 0x04, 'C' + pin 3, - pin 2
      0x04, // 0,0,0,0,0,1,0,0    //segment 0x08, 'D' + pin 2, - pin 7
      0xF8, // 1,1,1,1,1,0,0,0    //segment 0x10, 'E' + pin 6, - pin 7
      0xF0, // 1,1,1,1,0,0,0,0    //segment 0x20, 'F' + pin 5, - pin 6
      0x8C, // 1,0,0,0,1,1,0,0    //segment 0x40, 'G' + pin 6, - pin 3
      0xFC  // 1,1,1,1,1,1,0,0    // all off
};

#endif
  
void setup(void) {

    SPI.begin();         // initialize the SPI library
    Serial.begin(9600);   // console serial

    // setup the GPIOs
    pinMode(led1Pin, OUTPUT);
    pinMode(led2Pin, OUTPUT);
    pinMode(LATCH_Pin, OUTPUT);

    pinMode(buttonSET, INPUT);
    pinMode(buttonPLUS, INPUT);
    pinMode(buttonMINUS, INPUT);
    pinMode(buttonHOLD, INPUT);

    pinMode(dipswMODE0, INPUT);
    pinMode(dipswMODE1, INPUT);
    pinMode(dipswMODE2, INPUT);

    digitalWrite(LATCH_Pin, LOW); // 

    delay(100);

    Serial.println("Filament Clock firmware 03112222");
    
    SetTaskState(INITIAL_STATE);

    loopCounter=0;
    doDisplayUpdate = 0;
    validTimeOfDay  = 0;   
    segmentNumber=0;
    
}

void loop(void) {
    struct tm timeinfo;
    uInt8   pushEvt,modesw;
    
    delay(1); // 1 ms
    yield();
    
//    if((loopCounter%1)==0){    // once per x ms
        DisplayScan();  // meant to be called once per ms.
//    }
    
    if((loopCounter%25)==0){    // once per 25ms
 
         pushEvt = ReadButtons(&modesw);
        // There are 4 push button switches: SET, PLUS, MINUS, HOLD
        if(pushEvt&SET_BUTTON_STATE){
            Serial.println("SET button pressed");
         }
        if(pushEvt&PLUS_BUTTON_STATE){
            Serial.println("PLUS button pressed");
        }
        if(pushEvt&MINUS_BUTTON_STATE){
            Serial.println("MINUS button pressed");
        }
        if(pushEvt&HOLD_BUTTON_STATE){
            Serial.println("HOLD button pressed");
        }
        // There are 3 slide switches for modes. functions not yet defined
        if(modesw&MODE_0_SW_STATE){
          
        }
        doDisplayUpdate=1;  // could think of a smarter place for this
    }

    switch(taskState){
        case INITIAL_STATE:
            WriteDisplaySegments(HORIZONTAL_BAR_MID,HORIZONTAL_BAR_MID,HORIZONTAL_BAR_MID,HORIZONTAL_BAR_MID,HORIZONTAL_BAR_MID,HORIZONTAL_BAR_MID);
            doDisplayUpdate=1;
            SetTaskState(INVALID_TIME_STATE);
        break;
        case IDLING_STATE:    // normal working
            
        break;
        case  INVALID_TIME_STATE:
            SetTaskState(GET_NTP_TIME_CONNECT_STATE);
            break;
        case GET_NTP_TIME_CONNECT_STATE:    //  go here when the time needs to be updated.
            // Connect to Wi-Fi
            Serial.print("Connecting to ");
            Serial.println(ssid);
            WiFi.begin(ssid, password);
            SetTaskState(GET_NTP_TIME_CONNECTING_STATE);
            wifiTries = 0;
        break;
        case GET_NTP_TIME_CONNECTING_STATE:
            if((loopCounter%500)==0){    // once per 500ms
                if(WiFi.status() == WL_CONNECTED) {
                    SetTaskState(GET_NTP_TIME_COMMUNICATE_STATE);
                    wifiTries = 0;
                } else {
                    /// also check for timeout here
                    Serial.print(".");
                    wifiTries++;
                    if(wifiTries> 25 ) {  // 12.5 seconds.  OK????
                      // timeout.  now what?
                       Serial.println("Failed to connect WIFI");
                       SetTaskState(GET_NTP_TIME_DISCONNECT_STATE);
                    }
                }

            }
            break;
        case GET_NTP_TIME_COMMUNICATE_STATE: 
            if((loopCounter%100)==0){    // once per 100ms

                configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
                if(getLocalTime(&timeinfo)){    // did we get a valid time??
                    validTimeOfDay = 1;
                    SerialPrintLocalTime();
                    SetTaskState(GET_NTP_TIME_DISCONNECT_STATE);
                } else {
                    // we can try a few times before giving up
                    wifiTries++;
                    if(wifiTries> 50 ) {  // 5 seconds.  
                      // timeout.  now what?
                      SetTaskState(GET_NTP_TIME_DISCONNECT_STATE);
                    }
                }
            }
        break;
        case GET_NTP_TIME_DISCONNECT_STATE:
            //disconnect WiFi as it's no longer needed
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);  
            if(validTimeOfDay == 0){
                SetTaskState(INVALID_TIME_STATE);
            } else {
                SetTaskState(IDLING_STATE);
            }
        break;
      default:
        SetTaskState(IDLING_STATE);
        break;
    }

    if(doDisplayUpdate){
        doDisplayUpdate =0;
        DisplayTime();  // updates (writes) new data to the display
    }


    if((loopCounter%60000)==0){  // about 1 minute
        SerialPrintLocalTime();
     }

     BlinkLEDTask();
     
     loopCounter++;

}


void    SetTaskState(char newState){

    char  taskStateStrings[7][32]={
        "INITIAL_STATE",
        "IDLING_STATE",
        "INVALID_TIME_STATE",
        "GET_NTP_TIME_CONNECT_STATE",
        "GET_NTP_TIME_CONNECTING_STATE",
        "GET_NTP_TIME_COMMUNICATE_STATE",
        "GET_NTP_TIME_DISCONNECT_STATE"
    };
    
  if(newState != taskState){
    taskState = newState;
    Serial.print("Set state ");
    Serial.println(taskStateStrings[newState]);
  }
}

    // not currently used
char  ledState;
void  BlinkLEDTask(void){

    if((loopCounter%250)==0){
      switch(ledState){
          case 0:
            digitalWrite(led1Pin, LOW);
            digitalWrite(led2Pin, LOW);
            break;
          case 1:
            digitalWrite(led1Pin, HIGH);
            digitalWrite(led2Pin, LOW);
            break;
          case 2:
            digitalWrite(led1Pin, LOW);
            digitalWrite(led2Pin, HIGH);
            break;
          case 3:
            digitalWrite(led1Pin, HIGH);
            digitalWrite(led2Pin, HIGH);
            break;
          default:
            ledState = 0;
            break;
     }
     ledState++;
     ledState &= 0x03;
    }     
}

char    colonBlinker;

/*      DisplayTime writes new data to the segment array.
 *        Call this only to put new data into the display 
 *        DisplayScan keeps the display running
 *        
 *        Note: there are 2 colons
 *          the left colon, between hours and minutes, is bit 0 of colonState
 *          the right colon, between minutes and seconds, is bit 1 of colonState
 *          The default is to blink the colons once per second.
 *       
 */
void  DisplayTime(void){
    struct tm timeinfo;

    digitalWrite(led1Pin, HIGH);
 
    colonBlinker++;
    colonState = 0;
    if(colonBlinker>20){
        colonState = 0x03;
        digitalWrite(led2Pin, HIGH);
    }
    if(colonBlinker>40)
        colonBlinker= 0;

    if(validTimeOfDay)  { // you can only read the time if it is valid otherwise crash
        if(getLocalTime(&timeinfo))
            validTimeOfDay = 1;
        else {
            validTimeOfDay = 0;
            SetTaskState(INVALID_TIME_STATE);
        }
    }
    
    if(validTimeOfDay)  {
        WriteDisplayNumbers(timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
    } else {  // invalid time. display horizontal bars
        WriteDisplaySegments(HORIZONTAL_BAR_MID,HORIZONTAL_BAR_MID,HORIZONTAL_BAR_MID,HORIZONTAL_BAR_MID,HORIZONTAL_BAR_MID,HORIZONTAL_BAR_MID);
    }
    
   digitalWrite(led1Pin, LOW);
   digitalWrite(led2Pin, LOW);

}

/*    
 *     Write7SegDisplay is, essentially, the 7 segment encoder.
 *        called with the number to put into the display, the segmentBitMap is updated
 *        which then gets clocked out by DisplayScan
 */
void  WriteDisplayNumbers(char hh,char mm, char ss){

      // hh,mm,ss are binary numbers displayed as decimal from 0-99
      segmentBitMap[5] = sevenSegmentEncoder[(hh/10)];
      segmentBitMap[4] = sevenSegmentEncoder[(hh%10)];

      segmentBitMap[3] = sevenSegmentEncoder[(mm/10)];
      segmentBitMap[2] = sevenSegmentEncoder[(mm%10)];
      
      segmentBitMap[1] = sevenSegmentEncoder[(ss/10)];
      segmentBitMap[0] = sevenSegmentEncoder[(ss%10)];
     
}

void  WriteDisplaySegments(char d0,char d1,char d2,char d3,char d4,char d5){

      segmentBitMap[5] = d0;
      segmentBitMap[4] = d1;

      segmentBitMap[3] = d2;
      segmentBitMap[2] = d3;
      
      segmentBitMap[1] = d4;
      segmentBitMap[0] = d5;
}


/*    DisplayScan is called once per millisecond and is the routine which wites the segmentsBitMap out SPI
 *      to the shift registers.
 *      
 *      It writes one segment each cycle in a loop: 0,1,2,3,4,5,6,<repeat>...
 *      The segmentStatesBytes converts from the individual segment ON or OFF to the value written out the port.
 *     
 */
void  DisplayScan(void){
    char   idx;
    uInt8  spiByte,segmentBit;

    // output one segment on each cycle for each digit
    
    digitalWrite(LATCH_Pin, LOW); // 
    
    idx=0;
    while(idx<6){   // 6 digits
      segmentBit = (1<<segmentNumber);
      if(segmentBitMap[idx]&segmentBit)
          spiByte = segmentStatesBytes[segmentNumber];
      else
          spiByte = 0;

      // now the colons
      if(idx == 3) {  // LEFT colon
        if(colonState&0x02)     // LEFT colon
            spiByte &= 0xFC;    // ON
        else
            spiByte |= 0x03;    // OFF
      }
     if(idx == 1) {  // RIGHT colon
        if(colonState&0x01)     // RIGHT colon
            spiByte &= 0xFC;    // ON
        else
            spiByte |= 0x03;    // OFF
      }

      SPI.transfer(spiByte);
      idx++;
    }

    segmentNumber++;
    if(segmentNumber>6) // 7 segments 
        segmentNumber=0;
        
    digitalWrite(LATCH_Pin, HIGH); // ouput registers latch on rising edge
 
}

char  ConnectToWifi(void){
  char  err;

  err=0;
  
    // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  
  err=1;
  
  return(err);
}

char  weekDayList[7][10] = {
  "Sunday",
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saturday"
};

char monthList[12][10] = {
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December" 
};

char timeStringBuffer[128];

char *GetTimeStringFromTimeInfo(tm *timeinfo);
char *GetTimeStringFromTimeInfo(tm *timeinfo){

 /*
     year  = timeinfo.tm_year; // year since 1900
     month = timeinfo.tm_mon;
     wday = timeinfo.tm_wday;   // day of the week
     mday = timeinfo.tm_mday;    // day of the month
     hour = timeinfo.tm_hour;
     minute = timeinfo.tm_min;
     second = timeinfo.tm_sec;
*/
// "Today is Saturday, February 26, 2022, the time is HH:MM:SS PST"
//
     sprintf(timeStringBuffer, "Today is %s, %s, %d, %d T=%.2d:%.2d:%.2d PST"
                        ,weekDayList[timeinfo->tm_wday],monthList[timeinfo->tm_mon],timeinfo->tm_mday,(timeinfo->tm_year+1900)
                        ,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
                        
    return(timeStringBuffer);
}


void  SerialPrintLocalTime(void){
  struct tm timeinfo;
  char *buffer;

    if(validTimeOfDay)  { // you can only read the time if it is valid otherwise crash
        if(getLocalTime(&timeinfo)){
 
            buffer = GetTimeStringFromTimeInfo(&timeinfo);
            Serial.println(buffer);

        } else {
            Serial.println("invalid time");
        }
    }
}

// the push buttons produce only 1 "down" event when they are pressed
// the mode switches are steady state
uInt8   buttonStates,prevButtonStates;
uInt8    ReadButtons(uInt8 *modes){
    uInt8   pushEvent;
  
    pushEvent =0;
    buttonStates =0;
    
    if(digitalRead(buttonSET)==0)
        buttonStates |= SET_BUTTON_STATE;
    if(digitalRead(buttonPLUS)==0)
        buttonStates |= PLUS_BUTTON_STATE;
    if(digitalRead(buttonMINUS)==0)
        buttonStates |= MINUS_BUTTON_STATE;
    if(digitalRead(buttonHOLD)==0)
        buttonStates |= HOLD_BUTTON_STATE;

     
   // filter pushbutton events
    if(buttonStates != prevButtonStates){
      prevButtonStates = buttonStates;
      pushEvent = buttonStates;
#if 0    
  char buffer[128];
  sprintf(buffer, "pushEvent=%d buttonStates=%d prevButtonStates=%d ",pushEvent,buttonStates,prevButtonStates);
  Serial.println(buffer);
#endif
    }

    if(digitalRead(dipswMODE0)==0)
        buttonStates |= MODE_0_SW_STATE;
    if(digitalRead(dipswMODE1)==0)
        buttonStates |= MODE_1_SW_STATE;
    if(digitalRead(dipswMODE2)==0)
        buttonStates |= MODE_2_SW_STATE;

    (*modes) = (buttonStates&(MODE_0_SW_STATE|MODE_1_SW_STATE|MODE_2_SW_STATE));

    return(pushEvent);
}


void  SendEmail(char *msg1, char *msg2){

    struct tm timeinfo;
    char *buffer;
    char  textMsg[256];

 #if INCLUDE_SMTP_MAIL
 
    if(getLocalTime(&timeinfo)){
          buffer = GetTimeStringFromTimeInfo(&timeinfo);

          Serial.println(buffer);
    }

    if(ConnectToWifi()){


        /** Enable the debug via Serial port
          * none debug or 0
          * basic debug or 1
          * 
          * Debug port can be changed via ESP_MAIL_DEFAULT_DEBUG_PORT in ESP_Mail_FS.h
        */
        smtp.debug(1);

        /* Set the callback function to get the sending results */
        smtp.callback(smtpCallback);

        /* Declare the session config data */
        ESP_Mail_Session session;

        /* Set the session config */
        session.server.host_name = SMTP_HOST;
        session.server.port = SMTP_PORT;
        session.login.email = AUTHOR_EMAIL;
        session.login.password = AUTHOR_PASSWORD;
        session.login.user_domain = F("mydomain.net");

        /* Set the NTP config time */
        session.time.ntp_server = F("pool.ntp.org,time.nist.gov");
        session.time.gmt_offset = -8;
        session.time.day_light_offset = 0;

        /* Declare the message class */
        SMTP_Message message;

        /* Set the message headers */
        message.sender.name = F("Jay Hamlin");
        message.sender.email = AUTHOR_EMAIL;
        message.subject = F("Test sending plain text Email");
        message.addRecipient(F("Jay"), F("redwoodranch@mac.com"));

        sprintf(textMsg, "This is simple plain text message\n %s",buffer);
        
        message.text.content = textMsg;

        /** If the message to send is a large string, to reduce the memory used from internal copying  while sending,
        * you can assign string to message.text.blob by cast your string to uint8_t array like this
        * 
         * String myBigString = "..... ......";
         * message.text.blob.data = (uint8_t *)myBigString.c_str();
         * message.text.blob.size = myBigString.length();
         * 
         * or assign string to message.text.nonCopyContent, like this
         * 
         * message.text.nonCopyContent = myBigString.c_str();
         * 
         * Only base64 encoding is supported for content transfer encoding in this case. 
         */

       /** The Plain text message character set e.g.
         * us-ascii
         * utf-8
         * utf-7
         * The default value is utf-8
         */
        message.text.charSet = F("us-ascii");

        /** The content transfer encoding e.g.
         * enc_7bit or "7bit" (not encoded)
         * enc_qp or "quoted-printable" (encoded)
         * enc_base64 or "base64" (encoded)
         * enc_binary or "binary" (not encoded)
         * enc_8bit or "8bit" (not encoded)
         * The default value is "7bit"
        */
        message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

        /** The message priority
         * esp_mail_smtp_priority_high or 1
         * esp_mail_smtp_priority_normal or 3
         * esp_mail_smtp_priority_low or 5
         * The default value is esp_mail_smtp_priority_low
        */
        message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;



        //message.response.reply_to = "someone@somemail.com";
        //message.response.return_path = "someone@somemail.com";

        /** The Delivery Status Notifications e.g.
         * esp_mail_smtp_notify_never
         * esp_mail_smtp_notify_success
         * esp_mail_smtp_notify_failure
         * esp_mail_smtp_notify_delay
         * The default value is esp_mail_smtp_notify_never
        */
       //message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;

        /* Set the custom message header */
         message.addHeader(F("Message-ID: AUTHOR_EMAIL"));

        //For Root CA certificate verification (ESP8266 and ESP32 only)
        //session.certificate.cert_data = rootCACert;
        //or
        //session.certificate.cert_file = "/path/to/der/file";
        //session.certificate.cert_file_storage_type = esp_mail_file_storage_type_flash; // esp_mail_file_storage_type_sd
        //session.certificate.verify = true;
        
        /* Connect to server with the session config */
        if (smtp.connect(&session)){

            /* Start sending Email and close the session */
            if (!MailClient.sendMail(&smtp, &message))
              Serial.println("Error sending Email, " + smtp.errorReason());

            //to clear sending result log
            //smtp.sendingResult.clear();

            ESP_MAIL_PRINTF("Free Heap: %d\n", MailClient.getFreeHeap());
             //disconnect WiFi as it's no longer needed
        }
        
       WiFi.disconnect(true);
       WiFi.mode(WIFI_OFF);
    }
 #endif
}

#if INCLUDE_SMTP_MAIL
 
/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status)
{
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success())
  {
    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failled: %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);

      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients.c_str());
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject.c_str());
    }
    Serial.println("----------------\n");

    //You need to clear sending result as the memory usage will grow up.
    smtp.sendingResult.clear();
  }
}
#endif
