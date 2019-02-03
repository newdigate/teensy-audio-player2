// Simple advanced MP3 file player example     
//
// Requires the audio shield:
//   http://www.pjrc.com/store/teensy3_audio.html
//   or dac / pwm / SD shield
//
// Example sketch by Dnstje.nl
// Act like a real MP3/AAC player up to 255 songs.
// Buttons at port 2 3 4, feel free to remap them.
// I don't have a Audioboard yet, just a SDcard breakout and using the onboard DAC and PWM outputs.
//
// This sketch will also using: EEPROM, Bounce library, Serial, SD.
//
// Sending char's to serial port to change tracks.
// P = previous track, N = Next track, S = Stop track, O = Play track I = Pause track, K = list tracklist, C = current track, R = Random track selection.
//
//
// 
// MP3/AAC library code by Frank Boesing.

#include <SD.h>
#include <SPI.h>
#include <EEPROM.h> // store last track
#include "BALibrary.h"
#include <play_sd_mp3.h> //mp3 decoder
#include <play_sd_aac.h> // AAC decoder
#include "ID3Reader.h"
#include "picojpeg.h"
#include "JpegDecoder2.h"
#define minimum(a,b)     (((a) < (b)) ? (a) : (b))

#include <Audio.h>

#define ENCODER_DO_NOT_USE_INTERRUPTS
#include <Encoder.h>

#define BUTTON1 25  //NEXT
#define BUTTON2 28  //Play Pause
#define BUTTON3 32  //PREV 
#define BUTTON4 38  //PREV 

#include <Adafruit_GFX.h>    // Core graphics library
#include <ST7735_t3.h> 
//#include <Adafruit_ST7735.h> // Hardware-specific library

#define ENC2A 29 
#define ENC2B 30 
Encoder knob(ENC2A, ENC2B);
Encoder myEnc(26, 27);

#define sclk 14 //27  // SCLK can also use pin 13,14,
#define mosi 7  // MOSI can also use pin 7,11,28
#define cs   6  // CS & DC can use pins 2, 6, 9, 10, 15, 20, 21, 22, 23
#define dc   12   //  but certain pairs must NOT be used: 2+10, 6+9, 20+23, 21+22
#define rst  17   // RST can use any pin

//Adafruit_ST7735 tft = Adafruit_ST7735(cs, dc, mosi, sclk, rst);
ST7735_t3 tft = ST7735_t3(cs, dc, mosi, sclk, rst);

Bounce bouncer1 = Bounce(BUTTON1, 50); 
Bounce bouncer2 = Bounce(BUTTON2, 50); 
Bounce bouncer3 = Bounce(BUTTON3, 50);
Bounce bouncer4 = Bounce(BUTTON4, 50);

const int chipSelect = BUILTIN_SDCARD;  // if using another pin for SD card CS.
int track;
int tracknum;
int trackext[255]; // 0= nothing, 1= mp3, 2= aac, 3= wav.
String tracklist[255];
File root;
char playthis[64];
boolean trackchange;
boolean dirchange;
boolean paused;

long pos = 0;

AudioPlaySdMp3           playMp31; 
AudioPlaySdAac           playAac1;  
AudioOutputI2S           i2s1;   
AudioInputI2S            i2s2;           //xy=110,306

AudioMixer4              mixleft;
AudioMixer4              mixright;

AudioConnection          patch1(playMp31,0,mixleft,0);
AudioConnection          patch2(playMp31,1,mixright,0);

AudioConnection          patchCord1(i2s2, 0, mixleft, 3);
AudioConnection          patchCord2(i2s2, 1, mixright, 3);

AudioConnection          patch1a(playAac1, 0, mixleft, 1);
AudioConnection          patch2a(playAac1, 1, mixright, 1);

AudioConnection          patch5(mixleft, 0, i2s1, 0);
AudioConnection          patch6(mixright, 0, i2s1, 1);
//AudioControlSGTL5000     sgtl5000_1;     //xy=240,153
using namespace BALibrary;
BAAudioControlWM8731      codecControl;
//AudioControlWM8731       codecControl;       //xy=706,916



#include "directives.h"
#include "TFTSdDirectoryBrowser.h"

TFTSdDirectoryBrowser browser(&tft, &SD, 16,  
  ST7735_BLUE, ST7735_BLACK, 
  ST7735_BLUE, ST7735_WHITE, 
  ST7735_GREEN, ST7735_BLACK,
  ST7735_GREEN, ST7735_WHITE); 

String currentPath; 


long oldPosition = -999;
long positionAtStartOfChange = 0;
unsigned long encHasntChangedSince = 0;
boolean encoderChanged = false;

bool readDirectory(char * dir);
void drawJpeg(const File &jpegFile, ST7735_t3 &tft, int xpos, int ypos);
void renderJPEG( ST7735_t3 &tft, int xpos, int ypos);
char currentDirectoryName[100];

void setup() {
  Serial.begin(115200);


  
  //setup pins with pullups
  pinMode(BUTTON1,INPUT_PULLUP);
  pinMode(BUTTON3,INPUT_PULLUP);
  pinMode(BUTTON2,INPUT_PULLUP);  
  pinMode(BUTTON4,INPUT_PULLUP);  

  EEPROM.write(0, 0); 
  EEPROM.write(1, 0);
  
  // reads the last track what was playing.
  track = EEPROM.read(0); 
  int currentDirectoryNum = EEPROM.read(1);
 //currentDirectory = 0;
 
  Serial.printf("dir: %d \t\t track: %d\n", currentDirectoryNum, track);
  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  //AudioMemory(16);

  codecControl.disable();
  delay(100);
  AudioMemory(24);

  Serial.println("Enabling codec...\n");
  codecControl.enable();
  Serial.println("Enabled...\n");
  delay(100);
  
  //put the gain a bit lower, some MP3 files will clip otherwise.
  mixleft.gain(0,0.5);
  mixright.gain(0,0.5);

  //Start SD card
  if (!SD.begin(BUILTIN_SDCARD)) {
   
  }
  //currentDirectory = SD.open("/", FILE_READ);
  
  Serial.println("started sdcard...\n");
  //Starting to index the SD card for MP3/AAC.
   




  tft.initR(INITR_GREENTAB); // initialize a ST7735R chip, green tab
  tft.setRotation(1);
  tft.setTextWrap(true);
  tft.fillScreen(ST7735_BLACK);
  tft.setTextColor(ST7735_BLUE);   
  tft.print("Teensy audio player...");

  const char *jpeglogopath = "/logo.jpg";
  if (SD.exists(jpeglogopath)) {
    drawJpeg(jpeglogopath);   
  }

  browser.initialize();
  Serial.print("totalFiles:");
  Serial.print(browser.getTotalFileCount(), DEC);
  browser.reload();
  browser.update();
  myEnc.write(0);
  oldPosition = 0;
}

void drawJpeg(const char *filename) {
  File jpegFile = SD.open(filename, FILE_READ);
  boolean decoded = JpegDec.decodeSdFile(jpegFile); // or pass the SD file handle to the decoder,
  //boolean decoded = JpegDec.decodeFsFile(filename);  // or pass the filename (leading / distinguishes SPIFFS files)

  if (decoded) {
    // print information about the image to the serial port
    //jpegInfo();
    //Serial.printf("decoded, rendering... \n");
    // render the image onto the screen at given coordinates
    renderJPEG(tft, 0, 0);
  }
  else {
    Serial.println("Jpeg file format not supported!");
  }
  JpegDec.reset();
  jpegFile.close();
}

void playFileMP3(const char *filename)
{
  Serial.print("Playing file: ");
  Serial.print(track);
  Serial.print(" - ");
  Serial.println(filename);
  trackchange = true; //auto track change is allowed.
  // Start playing the file.  This sketch continues to
  // run while the file plays.
  EEPROM.write(0,track); //meanwhile write the track position to eeprom address 0
  
  tft.fillScreen(ST7735_BLACK);
 
  char trackname[255];
  char albumname[255];
  char composername[255];
  memset(trackname, 0, sizeof(trackname));
  memset(albumname, 0, sizeof(albumname));
  memset(composername, 0, sizeof(composername));

  ID3Reader id3reader = ID3Reader();  

  id3reader.onID3Tag = [&] (const char *tag, const char *text) {
  
        Serial.printf("!!!! -%s : %s\n", tag, text);
        if (memcmp(tag,"TIT2",4) == 0) {
          sprintf(trackname, text);

        } else if (memcmp(tag,"TALB",4) == 0) {
          sprintf(albumname, text);
          //tft.setCursor(0,64);

        }
        else if (memcmp(tag,"TPE2",4) == 0) {
          sprintf(composername, text);
          //tft.setCursor(0,64);
          //tft.setTextSize(1);
          //tft.setTextColor(ST7735_GREEN);   
          //tft.print(text);
          //tft.print("\n");
        }
    };
    
  id3reader.onID3JpegImageTag = [&] (File &jpegFile, char *text, uint64_t alength) {
    //Serial.printf("<><>img : %s\n", text);
    //File currentImage = SD.open("/current.jpg", FILE_WRITE);
    //uint64_t totalBytes = 0;
    //int n = 0;
    //char buff[255];
    //while (((n = jpegFile.read(buff, sizeof(buff))) > 0)  && (totalBytes < alength)) {
    // totalBytes += n;
    //  currentImage.write(buff, n);
      //Serial.printf("<><>img :rrrrrrrrr");
    //}   
    //currentImage.close();
    //Serial.printf("<><>img :complete");
    
    drawJpeg(jpegFile, tft, 0,0);
  };

  String fullpath = "/" + String(currentDirectoryName) + String(filename);
  id3reader.open(fullpath.c_str());

  playMp31.play(fullpath.c_str()) ;

  bool displayedInfo = false;
  // Simply wait for the file to finish playing.
  while (playMp31.isPlaying()) {
    // update controls!
    controls();

    //Serial.printf("%d - %d \n", playMp31.positionMillis(), playMp31.lengthMillis());
    //progressBar.drawProgress(playMp31.positionMillis(), playMp31.lengthMillis());


    if (!displayedInfo) {

      //drawJpeg("/current.jpg");
      displayedInfo = true;

      tft.setCursor(0,5);
      if (trackname[0] != 0) {  
        tft.setTextSize(1);
        tft.setTextColor(ST7735_WHITE);   
        tft.print(trackname);
        tft.print(" ");
      }
      if (albumname[0] != 0) {
        tft.setTextSize(1);
        tft.setTextColor(ST7735_GREEN);   
        tft.print(albumname);
        tft.print("\n");
      }
      if (composername[0] != 0)  {
        tft.setTextSize(1);
        tft.setTextColor(ST7735_BLUE);   
        tft.print(composername);
        tft.print("\n");
      }
    }   
  }
  Serial.println("Song complete");
}

void playFileAAC(const char *filename)
{
  Serial.print("Playing file: ");
  Serial.print(track);
  Serial.print(" - ");
  Serial.println(filename);
  trackchange = true; //auto track change is allowed.
  // Start playing the file.  This sketch continues to
  // run while the file plays.
  EEPROM.write(0,track); //meanwhile write the track position to eeprom address 0
  playAac1.play(filename);


  // Simply wait for the file to finish playing.
  while (playAac1.isPlaying()) {
    // update controls!
    controls();
  }
}
bool readDirectory(char *directory) {
  tracknum = 0;
  strcpy(currentDirectoryName, directory);
  Serial.printf("Opening: %s\n", directory);
  File dir = SD.open(directory);
  while (true) { 

    File files =  dir.openNextFile();
    if (!files) {
      //If no more files, break out.
      break;
    }

    String curfile = files.name(); //put file in string
    
    //look for MP3 or AAC files
    int m = curfile.lastIndexOf(".MP3");
    int a = curfile.lastIndexOf(".AAC");
    int a1 = curfile.lastIndexOf(".MP4");
    int a2 = curfile.lastIndexOf(".M4A");
    int underscore = curfile.indexOf("_");
    //int w = curfile.lastIndexOf(".WAV");

    // if returned results is more then 0 add them to the list.
    if ((m > 0 || a > 0 || a1 > 0 || a2 > 0 ) && (underscore != 0)) {  

      tracklist[tracknum] = files.name();
      if(m > 0) trackext[tracknum] = 1;
      if(a > 0) trackext[tracknum] = 2;  
      if(a1 > 0) trackext[tracknum] = 2;
      if(a2 > 0) trackext[tracknum] = 2;
      Serial.printf("file: %s\n",files.name());
      //  if(w > 0) trackext[tracknum] = 3;
      //Serial.print(m);
      tracknum++;  
    }
    
    files.close();
  }
    // close 
  dir.close();
  
  if (tracknum == 0) {
    Serial.print("there are not traks in this folders");
    // if there are no files...
    //currentDirectory++;
    EEPROM.write(1,0);
    EEPROM.write(0,0);
    track = 0;
    return false;
  } else
    //check if tracknum exist in tracklist from eeprom, like if you deleted some files or added.
  if(track > tracknum){
    //if it is too big, reset to 0
    EEPROM.write(0,0);
    track = 0;
    return false;
  }
  return true;
}


long last_millis = 0;
void controls() {
  bouncer1.update();
  bouncer2.update();
  bouncer3.update();
  bouncer4.update();
  if ( bouncer1.fallingEdge()) { 
    nexttrack();
  }
  if ( bouncer2.fallingEdge()) {  
    pausetrack();
  }
  if ( bouncer3.fallingEdge()) { 
    //prevtrack();
  }  
  if ( bouncer4.fallingEdge()) {
    playMp31.stop();
    playAac1.stop();
    Serial.println("button4!!!");
    Serial.println(browser.selectedFilename());
    while (!readDirectory(browser.selectedFilename())) { 
      delay(100);
      EEPROM.write(0, 0); 
      //EEPROM.write(1, currentDirectory);  
    } 
  }  

  long newPosition = myEnc.read();
  if (newPosition != oldPosition) {
    if (!encoderChanged) {
      oldPosition = newPosition;
    //Serial.println(newPosition);
      encHasntChangedSince = millis();
    }
    encoderChanged = true;
    if (encoderChanged && millis() - encHasntChangedSince >= 100) {
      signed char delta = delta = (newPosition > oldPosition)? 1 : -1;
      oldPosition = newPosition;
      //Serial.printf("currentDirectory %d \n", currentDirectory);

      encoderChanged = false;
   }
  }

  long current_millis = millis();
  if (current_millis > last_millis + 100) {
    //Serial.print(".");
    // put your main code here, to run repeatedly:
    long newLeft;
    newLeft = knob.read() / 4;
    if (newLeft < 0) {
      knob.write(0);
      newLeft = 0;
    } else if (newLeft > browser.getTotalFileCount()-1) {
      newLeft = browser.getTotalFileCount()-1;
      knob.write(newLeft*4);
    }
    
    if (newLeft != pos) {
      pos = newLeft;
      browser.setSelectFileIndex(pos);     
    }
    last_millis = current_millis;
  }
}



void loop() {
  controls();
  


  //Serial.println();
  //Serial.println(tracknum);
  //Serial.println(track);
  //Serial.println(trackext[track]);
  //Serial.println(tracklist[track]);
  String path = "/" + currentPath + "/" + tracklist[track];
  //Serial.println(path.c_str());
  if(trackext[track] == 1){
    Serial.println("MP3" );
    
    playFileMP3(path.c_str());
  }else if(trackext[track] == 2){
    Serial.println("aac");
    playFileAAC(playthis);
  }

  //Serial.println("...");
  if(trackchange == true){ //when your track is finished, go to the next one. when using buttons, it will not skip twice.
    nexttrack();
    Serial.println("next track...");
  }
  //delay(100);
}

void nexttrack(){
  Serial.println("Next track!");
  trackchange=false; // we are doing a track change here, so the auto trackchange will not skip another one.
  playMp31.stop();
  playAac1.stop();
  track++;
  if(track >= tracknum){ // keeps in tracklist.
    track = 0;
  }  
  tracklist[track].toCharArray(playthis, sizeof(tracklist[track])); //since we have to convert String to Char will do this    
}


void pausetrack(){
  if(paused){
    Serial.println("Playing!");
  }
  else{
    Serial.println("Paused!");
  }
  paused = playMp31.pause(!paused);
}


void randomtrack(){
  Serial.println("Random track!");
  trackchange=false; // we are doing a track change here, so the auto trackchange will not skip another one.
  if(trackext[track] == 1) playMp31.stop();
  if(trackext[track] == 2) playAac1.stop();

  track= random(tracknum);

  tracklist[track].toCharArray(playthis, sizeof(tracklist[track])); //since we have to convert String to Char will do this    
}








void drawJpeg(const File &jpegFile, ST7735_t3 &tft, int xpos, int ypos) {

  Serial.println("===========================");
  Serial.print("Drawing file: "); 
  Serial.println("===========================");

  // Open the named file (the Jpeg decoder library will close it after rendering image)
  //fs::File jpegFile = SPIFFS.open( filename, "r");    // File handle reference for SPIFFS
  //  File jpegFile = SD.open( filename, FILE_READ);  // or, file handle reference for SD library

  //ESP32 always seems to return 1 for jpegFile so this null trap does not work
  //if ( !jpegFile ) {
  //  Serial.print("ERROR: File not open\""); 
  //  return;
 // }

  // Use one of the three following methods to initialise the decoder,
  // the filename can be a String or character array type:

  //boolean decoded = JpegDec.decodeFsFile(jpegFile); // Pass a SPIFFS file handle to the decoder,
  boolean decoded = JpegDec.decodeSdFile(jpegFile); // or pass the SD file handle to the decoder,
  //boolean decoded = JpegDec.decodeFsFile(filename);  // or pass the filename (leading / distinguishes SPIFFS files)

  if (decoded) {
    // print information about the image to the serial port
    //jpegInfo();
    Serial.printf("decoded, rendering... \n");
    // render the image onto the screen at given coordinates
    renderJPEG(tft, xpos, ypos);
  }
  else {
    Serial.println("Jpeg file format not supported!");
  }
  JpegDec.reset();
}



//====================================================================================
//   Decode and paint onto the TFT screen
//====================================================================================
void renderJPEG( ST7735_t3 &tft, int xpos, int ypos) {

  // retrieve infomration about the image
  uint16_t *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  uint32_t scaleFactor1000_x = 128 * 1000 / max_x;
  uint32_t scaleFactor1000_y = 128 * 1000 / max_y;

  // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
  // Typically these MCUs are 16x16 pixel blocks
  // Determine the width and height of the right and bottom edge image blocks
  uint32_t min_w = minimum(mcu_w, max_x % mcu_w);
  uint32_t min_h = minimum(mcu_h, max_y % mcu_h);

  // save the current image block size
  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  // record the current time so we can measure how long it takes to draw an image
  uint32_t drawTime = millis();

  // save the coordinate of the right and bottom edges to assist image cropping
  // to the screen size
  max_x += xpos;
  max_y += ypos;

  // read each MCU block until there are no more
  while ( JpegDec.read()) {

    // save a pointer to the image block
    pImg = JpegDec.pImage;

    // calculate where the image block should be drawn on the screen
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    //pImg += (JpegDec.MCUx * mcu_w * JpegDec.MCUy * mcu_h)*2;

    // check if the image block size needs to be changed for the right and bottom edges
    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;
    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;

    // calculate how many pixels must be drawn
    uint32_t mcu_pixels = win_w * win_h;


    // draw image block if it will fit on the screen  
    //if ( ( mcu_x + win_w) <= tft.width() && ( mcu_y + win_h) <= tft.height()) {  
      //Serial.printf("jpeg window: (%d,%d, %d,%d) - %d", mcu_x, mcu_y, win_w, win_h, mcu_pixels);  
      // open a window onto the screen to paint the pixels into
      //TFTscreen.setAddrWindow(mcu_x, mcu_y, mcu_x + win_w - 1, mcu_y + win_h - 1);
      //tft.setAddrWindow(mcu_x, mcu_y, mcu_x + win_w - 1, mcu_y + win_h - 1);
      // push all the image block pixels to the screen
      //while (mcu_pixels--) ; tft.pushColor(*pImg++); // Send to TFT 16 bits at a time
      uint16_t x = 0, y = 0, old_x = 1, old_y = 1;     
      for (uint16_t i=0; i<win_w; i++)
      {
        y = (scaleFactor1000_y * (mcu_y + i))/1000;
        if (y != old_y) {
          for (uint16_t j=0; j<win_h; j++) {
            x = (scaleFactor1000_x * (mcu_x + j))/1000;           
            if (x != old_x)
              tft.drawPixel(x, y, *pImg++);
            else
              pImg++;
            old_x = x;
            old_y = y;
          }
        } else pImg+=win_h;
      }
    //} 

    // stop drawing blocks if the bottom of the screen has been reached
    // the abort function will close the file
//    else 
    if ( ( mcu_y + win_h) >= tft.height()) JpegDec.abort();

  }

  // calculate how long it took to draw the image
  drawTime = millis() - drawTime; // Calculate the time it took

  // print the results to the serial port
  Serial.print  ("Total render time was    : "); Serial.print(drawTime); Serial.println(" ms");
  Serial.println("=====================================");
}
