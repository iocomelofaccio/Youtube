
// ./ffmpeg -i MiniTvIntro.mp4 -ar 44100 -ac 1 -ab 24k -filter:a loudnorm -filter:a "volume=-5dB" -f mp3 MiniTvIntro.mp3
// ./ffmpeg -i MiniTvIntro.mp4 -vf "fps=25,scale=-1:128:flags=lanczos,crop=160:in_h:(in_w-160)/2:0" -q:v 11 MiniTvIntro.mjpeg

// TFT Section
#define TFT_CS         5
#define TFT_RST        4
#define TFT_DC         2

// SD module on 2nd SPI bus
#define HSPI_CLK  14
#define HSPI_MISO 27
#define HSPI_MOSI 13
#define HSPI_CS   15

// Button section
#define P_UP      19
#define P_DOWN    21
#define P_OK      22

#include <SPI.h>
#include <SD.h>
SPIClass SPISD(HSPI);

bool mp3Loop = false;
int curFile = 0;
int mp3File = 0;
int mjpegFile = 0;
int SETTINGS = 2;
String *mp3FileName;
String *mjpegFileName;
String *settings;
bool videoRun = false;
int volumeSet = 16;

#include <Arduino_GFX_Library.h>
#define GFX_BL DF_GFX_BL // default backlight pin, you may replace DF_GFX_BL to actual backlight pin

Arduino_DataBus *bus = new Arduino_HWSPI(TFT_DC, TFT_CS);
Arduino_GFX *gfx = new Arduino_ST7735(bus, TFT_RST, 3 /* rotation */, false /* IPS */, 128, 160, 0, 0, 0, 0, false);

#include "MjpegClass.h"

int next_frame = 0;
int skipped_frames = 0;
//unsigned long total_read_audio = 0;
//unsigned long total_read_video = 0;
//unsigned long total_play_video = 0;
//unsigned long total_remain = 0;
unsigned long start_ms, curr_ms, next_frame_ms;

#define FPS 25
#define MJPEG_BUFFER_SIZE (161 * 129 * 2 / 8)
#define READ_BUFFER_SIZE 2048

/* audio */
#include "MyAudio.h"
//#include <Audio.h>
#define I2S_LRC     26
#define I2S_DOUT    25
#define I2S_BCLK    33
MyAudio audio;
//Audio audio;
File mFile;
bool mp3_file_available;
TaskHandle_t Task1;

void setup() {
  Serial.begin(115200);
  disableCore0WDT();
  gfx->begin();
  gfx->fillScreen(BLACK);
  if (!initFS()) {
    while (true);
  }
  sdMediaList();

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, -1, 0);
  audio.setVolume(volumeSet); // 0...21 Will need to add a volume setting in

  pinMode(P_UP, INPUT_PULLUP);
  pinMode(P_DOWN, INPUT_PULLUP);
  pinMode(P_OK, INPUT_PULLUP);
  xTaskCreatePinnedToCore(mjpegAudioTask, "mjpegAudioTask", 10000, NULL, 2, &Task1, xPortGetCoreID()==0?1:0);
  settings = new String[2];
  settings[0] = "Volume";
  settings[1] = "Mp3Loop";
  delay(200);
}

void loop() {
  printMenu();
  while (true) {
    if (digitalRead(P_UP) == 0) {
      curFile--;
      if (curFile < 0) curFile = (mp3File + mjpegFile + SETTINGS) - 1;
      delay(200);
      break;
    }
    if (digitalRead(P_DOWN) == 0) {
      curFile++;
      if (curFile >= mp3File + mjpegFile + SETTINGS) curFile = 0;
      delay(200);
      break;
    }

    if (digitalRead(P_OK) == 0) {
      if (curFile < mp3File) {
        gfx->print("Load:");
        gfx->println(mp3FileName[curFile]);
        if (loadMP3(mp3FileName[curFile])) {
          gfx->print("Play...");
          playMP3();
          mFile.close();
        }
      } else if (curFile >= mp3File && curFile < mp3File + mjpegFile) {
        String af = mjpegFileName[curFile - mp3File].substring(0,mjpegFileName[curFile - mp3File].length() - 5);
        af.concat("mp3");
        Play(mjpegFileName[curFile - mp3File], af);
      } else {
        if (curFile - (mp3File + mjpegFile) == 0) {
          bool volLoop = true;
          delay(200);
          while (volLoop) {
            gfx->fillScreen(BLACK);
            gfx->setCursor(0, 0);
            gfx->setTextColor(YELLOW);
            gfx->println("VOLUME");
            gfx->println("");
            int perc = map(volumeSet, 0, 21, 0, 100);
            gfx->setTextColor(WHITE);
            gfx->print("Volume: "); gfx->print(perc); gfx->println("%");
            bool setVolLoop = true;
            while (setVolLoop) {
              if (digitalRead(P_UP) == 0) {
                volumeSet++;
                setVolLoop = false;
              }
              if (digitalRead(P_DOWN) == 0) {
                volumeSet--;
                setVolLoop = false;
              }
              if (digitalRead(P_OK) == 0) {
                setVolLoop = false;
                volLoop = false;
              }
              delay(200);
            }
            if (volumeSet > 21) volumeSet = 21;
            if (volumeSet < 0) volumeSet = 0;
          }
          audio.setVolume(volumeSet);
        } else if (curFile - (mp3File + mjpegFile) == 1) {
          mp3Loop = !mp3Loop;
        }
      }
      delay(200);
      break;
    }
  }
  delay(100);
}

bool initFS() {
  SPISD.begin(HSPI_CLK, HSPI_MISO, HSPI_MOSI);
  if (!SD.begin(HSPI_CS, SPISD)) {
    gfx->println("ERROR: File system mount failed!");
    return false;
  }
  return true;
}

void sdMediaList() {
  File folder = SD.open("/");

  while (true) {
    File entry = folder.openNextFile();
    if (!entry) {
      folder.rewindDirectory();
      break;
    } else {
      String nome = entry.name();
      if (!nome.startsWith(".")) {
        if (nome.endsWith("mp3")) mp3File++;
        if (nome.endsWith("mjpeg")) mjpegFile++;
      }
    }
    entry.close();
  }

  if (mp3File > 0)    mp3FileName = new String[mp3File];
  if (mjpegFile > 0)  mjpegFileName = new String[mjpegFile];

  int x = 0, y = 0, z = 0;
  while (true) {
    File entry = folder.openNextFile();
    if (!entry) {
      folder.rewindDirectory();
      break;
    } else {
      String nome = entry.name();
      if (!nome.startsWith(".")) {
        if (nome.endsWith("mp3") && y < mp3File) mp3FileName[y++] = "/" + nome;
        if (nome.endsWith("mjpeg") && z < mjpegFile) mjpegFileName[z++] = "/" + nome;
      }
    }
    entry.close();
  }
  folder.close();
}

void Play(String mjpegFile, String MP3_file) {
  delay(500);
  File aFile = SD.open(MP3_file);
  next_frame = 0;
  if (!aFile || aFile.isDirectory())
  {
    Serial.println("ERROR: Failed to open %s file for reading!");
  }
  else{
    char __dataFileName[MP3_file.length() + 1];
    MP3_file.toCharArray(__dataFileName, sizeof(__dataFileName));
    const char* s = __dataFileName;
    //audio.connecttoFS(SD, s);
    audio.connecttoSD(s);
    File vFile = SD.open(mjpegFile);
    static MjpegClass mjpeg;
    if (!vFile || vFile.isDirectory()){
      gfx->println("ERROR: Failed to open  file for reading");
    }
    else
    {
      //uint8_t *aBuf = (uint8_t *)malloc(2940);
      uint8_t *aBuf = (uint8_t *)malloc(5880);
      if (!aBuf)
      {
        Serial.println(F("aBuf malloc failed!"));
      }
      else{
        uint8_t *mjpeg_buf = (uint8_t *)malloc(MJPEG_BUFFER_SIZE);
        if (!mjpeg_buf){
          Serial.println(F("mjpeg_buf malloc failed!"));
        }
        else{
          start_ms = millis();
          curr_ms = millis();
          mjpeg.setup(vFile, mjpeg_buf, (Arduino_TFT *)gfx, true);
          next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
          videoRun = true;
          //total_read_video=0;
          while (vFile.available() && digitalRead(P_OK)) {
            curr_ms = millis();
            // Read video
            mjpeg.readMjpegBuf();
            //total_read_video += millis() - curr_ms;
            curr_ms = millis();
            if (millis() < next_frame_ms) // check show frame or skip frame
            {
              // Play video
              mjpeg.drawJpg();
              //total_play_video += millis() - curr_ms;

              int remain_ms = next_frame_ms - millis();
              //total_remain += remain_ms;

              if (remain_ms > 0) {
                delay(remain_ms);
              }
            } else
            {
              ++skipped_frames;
              //Serial.println(F("Skip frame"));
            }
            curr_ms = millis();
            next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
          }
          //int time_used = millis() - start_ms;
          videoRun = false;
          vFile.close();
          aFile.close();
          //int played_frames = next_frame - 1 - skipped_frames;
          //float fps = 1000.0 * played_frames / time_used;
        }
      }
    }
  }
}

bool loadMP3(String MP3_file) {
  mp3_file_available = false;
  //gfx->print("Open MP3 file: "); gfx->println(MP3_file);
  mFile = SD.open(MP3_file);
  if (mFile) {
    mp3_file_available = true;
    char __dataFileName[MP3_file.length() + 1];
    MP3_file.toCharArray(__dataFileName, sizeof(__dataFileName));
    const char* s = __dataFileName;
    //audio.connecttoFS(SD, s);
    audio.connecttoSD(s);
  }
  if (!mFile || mFile.isDirectory()) {
    gfx->print("ERROR: Failed to open "); gfx->print(MP3_file); gfx->println(" file for reading");
    return false;
  }
  return true;
}


void playMP3() {
  delay(500);
  while (digitalRead(P_OK) && audio.isRunning()) {
    audio.loop();
  }
  delay(200);
  if (!digitalRead(P_OK)) mp3Loop = false;
  if (mp3Loop) {
    curFile++;
    if (curFile >= mp3File) {
      curFile = 0;
    }
    gfx->setTextColor(WHITE);
    gfx->print("Load:");
    gfx->println(mp3FileName[curFile]);
    if (loadMP3(mp3FileName[curFile])) {
      gfx->print("Play...");
      playMP3();
      mFile.close();
    }
  }
  delay(200);
}

void mjpegAudioTask( void * parameter) {
  //bool oldStatus = videoRun;
  while (true) {
    if (videoRun) {
      audio.loop();
    }
    //if (videoRun == true && oldStatus == false) Serial.println("Start Audio");
    //if (videoRun == false && oldStatus == true) Serial.println("Stop Audio");
    //oldStatus = videoRun;
    delay(0.1);
  }
}

void printMenu(){
  gfx->fillScreen(BLACK);
  gfx->setCursor(0, 0);
  gfx->setTextColor(YELLOW);
  gfx->println("MP3 FILE LIST");
  gfx->setTextColor(WHITE);
  for (int i = 0; i < mp3File; i++) {
    gfx->setTextColor(RED);
    if (i == curFile) gfx->print("-> ");
    else gfx->print("   ");
    gfx->setTextColor(WHITE);
    gfx->println(mp3FileName[i]);
  }
  gfx->setTextColor(YELLOW);
  gfx->println("MJPEG FILE LIST");
  gfx->setTextColor(WHITE);
  for (int i = 0; i < mjpegFile; i++) {
    gfx->setTextColor(RED);
    if (i == curFile - mp3File) gfx->print("-> ");
    else gfx->print("   ");
    gfx->setTextColor(WHITE);
    gfx->println(mjpegFileName[i]);
  }
  gfx->setTextColor(YELLOW);
  gfx->println("SETTINGS");
  gfx->setTextColor(WHITE);
  for (int i = 0; i < SETTINGS; i++) {
    gfx->setTextColor(RED);
    if (i == curFile - (mp3File + mjpegFile)) gfx->print("-> ");
    else gfx->print("   ");
    gfx->setTextColor(WHITE);
    if (settings[i] == "Mp3Loop") {
      gfx->print(settings[i]);
      gfx->setTextColor(GREEN);
      if (mp3Loop) {
        gfx->println(" V");
        gfx->setTextColor(RED);
        gfx->println("   tenere premuto OK per interrompere il loop");
      }
      else {
        gfx->println(" X");
      }
    } else {
      gfx->println(settings[i]);
    }
  }
}
/*
// optional
void audio_info(const char *info) {
  Serial.print("info        "); Serial.println(info);
}
void audio_id3data(const char *info) { //id3 metadata
  Serial.print("id3data     "); Serial.println(info);
}
void audio_eof_mp3(const char *info) { //end of file
  Serial.print("eof_mp3     "); Serial.println(info);
}
void audio_showstation(const char *info) {
  Serial.print("station     "); Serial.println(info);
}
void audio_showstreamtitle(const char *info) {
  Serial.print("streamtitle "); Serial.println(info);
}
void audio_bitrate(const char *info) {
  Serial.print("bitrate     "); Serial.println(info);
}
void audio_commercial(const char *info) { //duration in sec
  Serial.print("commercial  "); Serial.println(info);
}
void audio_icyurl(const char *info) { //homepage
  Serial.print("icyurl      "); Serial.println(info);
}
void audio_lasthost(const char *info) { //stream URL played
  Serial.print("lasthost    "); Serial.println(info);
}
void audio_eof_speech(const char *info) {
  Serial.print("eof_speech  "); Serial.println(info);
}
*/
