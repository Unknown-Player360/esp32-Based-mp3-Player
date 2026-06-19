#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <ESP32Encoder.h>
#include <Bounce2.h>
#include <SPI.h>
#include <SD.h>
#include <vector>

#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// put function declarations here:
//int myFunction(int, int);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);
std::vector<String> fileList;
// Encoder
#define ENC_A 32
#define ENC_B 33

// Buttons
#define BTN_UP     35
#define BTN_DOWN   34
#define BTN_SELECT 36
#define BTN_BACK   39
#define SD_CS 5

ESP32Encoder encoder;
long lastEncoderValue = 0;
Bounce btnUp = Bounce();
Bounce btnDown = Bounce();
Bounce btnSelect = Bounce();
Bounce btnBack = Bounce();

//Menu State code
int selectedIndex = 0;
int topIndex = 0;   // first visible item
const int visibleItems = 5;

AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceSD *file = nullptr;
AudioOutputI2S *out = nullptr;

float volume = 0.03;   
bool isPlaying = false;
#define MAX_VOLUME 0.08 //safe loud volume do not exceed VERY RISKY.
#define MIN_VOLUME 0.0 

bool isPaused = false;
bool displayFrozen = false;
//displayFrozen is not used anymore, it is a relic of the older times

String currentPath = "/";
bool inFolder = false;

//Uncomment below for scrolling text
//int scrollOffset = 0;
//unsigned long lastScroll = 0;

TaskHandle_t audioTaskHandle = NULL;
SemaphoreHandle_t audioMutex;

uint32_t trackLength = 0;

void setupInput() {

    // Encoder
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachFullQuad(ENC_A, ENC_B);
    encoder.setCount(0);

    // Buttons
    //pinMode(BTN_UP, INPUT_PULLUP);
    //pinMode(BTN_DOWN, INPUT_PULLUP);
    //all buttons use the input only pins
    //if using pins with internal resistors use the above code
    pinMode(BTN_UP, INPUT);
    pinMode(BTN_DOWN, INPUT);
    pinMode(BTN_SELECT, INPUT);
    pinMode(BTN_BACK, INPUT);

    btnUp.attach(BTN_UP);
    btnUp.interval(5);

    btnDown.attach(BTN_DOWN);
    btnDown.interval(5);

    btnSelect.attach(BTN_SELECT);
    btnSelect.interval(5);

    btnBack.attach(BTN_BACK);
    btnBack.interval(5);
}

void playSelectedFile() {
    if (fileList.size() == 0) return;

    if (xSemaphoreTake(audioMutex, portMAX_DELAY)){
    if (mp3) {
        mp3->stop();
        delete mp3;
        mp3 = nullptr;
    }

    if (file) {
        delete file;
        file = nullptr;
    }

    String path = currentPath;
    if (!path.endsWith("/")) path += "/";
    path += fileList[selectedIndex];

    file = new AudioFileSourceSD(path.c_str());
    mp3 = new AudioGeneratorMP3();
    if (mp3->begin(file, out)) {
        isPlaying = true;
        isPaused = false;
        trackLength = file->getSize();

        u8g2.clearBuffer();
        u8g2.drawStr(0, 10, "PLAYING:");
        u8g2.drawStr(0, 25, fileList[selectedIndex].c_str());
        u8g2.sendBuffer();
        displayFrozen = true;
    } else {
        delete mp3;
        delete file;
        mp3 = nullptr;
        file = nullptr;
        isPlaying = false;
    }
    xSemaphoreGive(audioMutex);
    }
}

void nextTrack() {
    if (fileList.size() == 0) return;

    selectedIndex++;
    if (selectedIndex >= fileList.size()) selectedIndex = 0;

    playSelectedFile();
}

void prevTrack() {
    if (fileList.size() == 0) return;

    selectedIndex--;
    if (selectedIndex < 0) selectedIndex = fileList.size() - 1;

    playSelectedFile();
}

void stopPlayback() {
    if (xSemaphoreTake(audioMutex, portMAX_DELAY)){
    if (mp3) {
        mp3->stop();
        delete mp3;
        mp3 = nullptr;
    }

    if (file) {
        delete file;
        file = nullptr;
    }

    isPlaying = false;
    isPaused = false;
    displayFrozen = false;

    xSemaphoreGive(audioMutex);
    }
}

void listFiles(String path) {
    fileList.clear();

    File root = SD.open(path);

    File entry = root.openNextFile();

    while (entry) {
        String fullName = entry.name();
        String name = fullName;

        // strip path, keep only last part
        int lastSlash = name.lastIndexOf('/');
        if (lastSlash != -1) {
            name = name.substring(lastSlash + 1);
        }

        // Skip junk folders
        if (name == "System Volume Information") {
            entry = root.openNextFile();
            continue;
        }

        if (entry.isDirectory()) {
            fileList.push_back("[" + name + "]");
        } else if (name.endsWith(".mp3")){
            fileList.push_back(name);
        }

        entry = root.openNextFile();
    }

    selectedIndex = 0;
    topIndex = 0;
}

void handleInput() {
    // Update buttons
    btnUp.update();
    btnDown.update();
    btnSelect.update();
    btnBack.update();
    if (fileList.size() == 0) return;

    // Encoder movement
    long newValue = encoder.getCount();

    long diff = (newValue - lastEncoderValue) / 2;

    if (diff != 0) {
      lastEncoderValue = newValue;

       if (isPlaying) {
        volume += diff * 0.002;

        if (volume > MAX_VOLUME) volume = MAX_VOLUME;
        if (volume < MIN_VOLUME) volume = MIN_VOLUME;

        out->SetGain(volume);

        Serial.print("Volume: ");
        Serial.println(volume);
    } else {
        // menu scrolling
        selectedIndex += diff;

        if (fileList.size() == 0) return;
        if (selectedIndex < 0) selectedIndex = 0;
        if (selectedIndex >= fileList.size()) selectedIndex = fileList.size() - 1;
    }
    }

    // Button navigation for playing/accessing and stuff
    if (btnUp.fell()) {
        if (isPlaying) prevTrack();
        else selectedIndex--;
    }

    if (btnDown.fell()) {
        if (isPlaying) nextTrack();
        else selectedIndex++;
    }

    if (btnSelect.fell()) {
      String name = fileList[selectedIndex];
    
      if (name.startsWith("[") && name.endsWith("]")) {
         name.remove(0, 1); // remove [
         name.remove(name.length() - 1); // remove ]

         currentPath = currentPath;
         if (!currentPath.endsWith("/"))
         currentPath += "/";
         currentPath += name;
         listFiles(currentPath);
         return;
      }
      if (!isPlaying) {
            playSelectedFile();
            return;
      } 
      if (xSemaphoreTake(audioMutex, portMAX_DELAY)){
      isPaused = !isPaused;
      xSemaphoreGive(audioMutex);
      }
    }

    if (btnBack.fell()) {
        if (isPlaying) {
          stopPlayback();
          listFiles(currentPath);
          return;
        }

        if (currentPath != "/") {
          int lastSlash = currentPath.lastIndexOf('/', currentPath.length() - 2);
          currentPath = currentPath.substring(0, lastSlash + 1);

          listFiles(currentPath);
        }
    }

    // Clamp again
    if (selectedIndex < 0) selectedIndex = 0;
    if (selectedIndex >= fileList.size()) selectedIndex = fileList.size() - 1;

}

void updateScroll() {
    if (selectedIndex < topIndex) {
        topIndex = selectedIndex;
    }
    else if (selectedIndex >= topIndex + visibleItems) {
        topIndex = selectedIndex - visibleItems + 1;
    }
}

float getProgress(){
    if (!file || trackLength == 0) return 0;
    return (float)file->getPos() / trackLength;
}

void drawMenu() {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_6x10_tf);

    int visibleItems = 5;

    if (fileList.size() == 0) {
      u8g2.drawStr(0, 10, "NO FILES");
      u8g2.sendBuffer();
      return;
    }

    if (isPlaying) {
          u8g2.clearBuffer();
          u8g2.drawStr(0, 10, isPaused ? "PAUSED" : "PLAYING:");
          u8g2.drawStr(0, 25, fileList[selectedIndex].c_str());

          float progress = getProgress();
          int barWidth = progress * 120;

          u8g2.drawFrame(4, 50, 120, 10);
          u8g2.drawBox(4, 50, barWidth, 10);

          u8g2.sendBuffer();
          return;
        }

    for (int i = 0; i < visibleItems; i++) {
        int itemIndex = topIndex + i;
        if (itemIndex >= fileList.size()) break;

        if (itemIndex == selectedIndex) {
            u8g2.drawBox(0, i * 12, 128, 12);
            u8g2.setDrawColor(0);
        } else {
            u8g2.setDrawColor(1);
        }

        //Uncomment Below for scrolling names

        //String name = fileList[itemIndex];
        //if (itemIndex == selectedIndex) {
          //  if (millis() - lastScroll > 300) {
            //    scrollOffset++;
              //  if (scrollOffset > name.length()) scrollOffset = 0;
                //lastScroll = millis();
            //}

            //String visible = name.substring(scrollOffset);
            //u8g2.drawStr(2, i * 12 + 10, visible.c_str());
        //}
        //else {
        //    u8g2.drawStr(2, i * 12 + 10, name.c_str());
        //}

        //Comment Below if using scrolling names
        u8g2.drawStr(2, i * 12 + 10, fileList[itemIndex].c_str());
    }

    u8g2.sendBuffer();
}

void audioTask(void *param) {
    while (true) {
    if (xSemaphoreTake(audioMutex, portMAX_DELAY)){
        if (isPlaying && mp3 && !isPaused) {
            if (mp3->isRunning()) {
                mp3->loop();
            } else {
                mp3->stop();

                delete mp3;
                mp3 = nullptr;

                delete file;
                file = nullptr;

                isPlaying = false;
                displayFrozen = false;

                Serial.println("Song finished");

                // AUTO NEXT TRACK
                nextTrack();
            }
        }
        xSemaphoreGive(audioMutex);
    }

        vTaskDelay(1); // VERY IMPORTANT(prevents watchdog reset)
    }
}

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  Wire.setClock(400000);
  u8g2.setFont(u8g2_font_6x10_tf);
  setupInput();

  bool sdOK = false;

  for (int i = 0; i < 5; i++) {
      if (SD.begin(SD_CS, SPI, 40000000)) {
          sdOK = true;
          break;
      }
      delay(300);
  }

  if (!sdOK) {
      u8g2.clearBuffer();
      u8g2.drawStr(0, 10, "SD FAIL");
      u8g2.sendBuffer();

      while (true);
  }

  out = new AudioOutputI2S();
  out->SetPinout(26, 25, 13); // BCLK, LRCK, DIN
  out->SetGain(volume); // volume (0.0 - 1.0)

  currentPath = "/";
  listFiles(currentPath);
  
  xTaskCreatePinnedToCore(
    audioTask,        // function
    "Audio Task",     // name
    4096,             // stack size
    NULL,
    1,                // priority
    &audioTaskHandle,
    0                 // CORE 0
  );

  audioMutex = xSemaphoreCreateMutex();
}

void loop() {
    handleInput();
    updateScroll();
    drawMenu();
}