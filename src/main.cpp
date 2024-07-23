// This program displays all the bitmap images found on an SPI connected
// SD card one by one in the center of a 2.1 inch SPI TFT display attached
// to a NodeMCU 1.0 microcontroller.
// It will automatically initiate the image display functionality
// upon boot when a FAT formated SD card is found.
// The program also has another mode intended for a small message with
// an animation using bitmaps. This mode is access by pressing the button 
//  attached to D1.
// Made as an anniversary gift :).

// As written, this uses the microcontroller's SPI interface for the screen
// (not 'bitbang') and must be wired to specific pins (e.g. for Arduino Uno,
// MOSI = pin 11, MISO = 12, SCK = 13). Other pins are configurable below.

// Wiring details for Node MCU 1.0
// SPI:
// CLK -> D5
// MISO -> D6
// MOSI -> D7
// TFT DC -> D2
// CS (Display) -> D8
// CS (SD card) -> D3
// SD card SPI uses the same pins (Hardware SPI)

#include <Adafruit_GFX.h>         // Core graphics library
#include <Adafruit_ILI9341.h>     // Hardware-specific library
#include <SdFat.h>                // SD card & FAT filesystem library
#include <Adafruit_SPIFlash.h>    // SPI / QSPI flash library
#include <Adafruit_ImageReader.h> // Image-reading functions


// TFT display and SD card share the hardware SPI interface, using
// 'select' pins for each to identify the active device on the bus.

#define SD_CS   D3 // SD card select pin
#define TFT_CS D8 // TFT select pin
#define TFT_DC  D2 // TFT display/command pin

// Assign human-readable names to some common 16-bit color values:
#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

SdFat                SD;         // SD card filesystem
Adafruit_ImageReader reader(SD); // Image-reader object, pass in SD filesys
Adafruit_ILI9341     tft    = Adafruit_ILI9341(TFT_CS, TFT_DC);
String rootDir = "/";
const uint16_t screenWidth = 240,
              screenLength = 320;
const int slideshowRefreshTime = 4000; // The time it takes to change between two pictures, in milliseconds
volatile bool changeButtonState = false;
const uint8_t maxStates = 2;
uint8_t currentState = 0;
uint32_t prevStateChange = millis(); // Millis timer for setting refresh, used as a button debouncer

// Helper function to display a single image to the TFT display
// given a full file path from the root directory
void displayImageTFT(Adafruit_ILI9341 tft, String filename){

  ImageReturnCode stat; // Status from image-reading functions
  int32_t            width  = 0, // BMP image dimensions
                     height = 0;
  stat = reader.bmpDimensions(filename.c_str(), &width, &height);
  reader.printStatus(stat);  
  stat = reader.drawBMP(filename.c_str(), tft, (screenWidth-width)/2, (screenLength-height)/2);
  reader.printStatus(stat);   
}
// Overload to display the bitmap image at a different location
void displayImageTFT(Adafruit_ILI9341 tft, String filename, int x, int y){

  ImageReturnCode stat; // Status from image-reading functions
  stat = reader.drawBMP(filename.c_str(), tft, x, y);
  reader.printStatus(stat);   
}

// Queries all the images in a directory and display them to the tft screen object
// Supports a maximum filename length of 50 characters
void displayAllImagesDir(Adafruit_ILI9341 tft,String rootDir, volatile bool changeStateButton){

  const uint8_t nameLengthMax = 50; 
  char name[nameLengthMax];
  String completeFilename = rootDir;

  File32 dir = SD.open(rootDir); // Open the root directory of SD card
  File32 entry = dir.openNextFile(); // Open the first file in the SD card
  uint32_t time_1 = millis(); // Might cause an overflow close to day 50....

  //Display the first image in the SD card
  entry.getName(name,nameLengthMax);
  completeFilename.concat(name);
  displayImageTFT(tft,completeFilename);

  // If there any more files display them with the set delay
  entry = dir.openNextFile();
  while(entry){

    // Exit upon encountering the mode change button
    if(changeButtonState){
      return;
    }

    if(millis()-time_1>=slideshowRefreshTime){
      entry.getName(name,nameLengthMax);
      completeFilename=rootDir;
      completeFilename.concat(name);
      displayImageTFT(tft,completeFilename);

      entry = dir.openNextFile();
      time_1 = millis();
    }  
    yield();
  } 

  // Delay for the last picture :)
  while(!(millis()-time_1>=slideshowRefreshTime)){
    yield();
  }

}

// ISR handler for the mode change button, only sets a boolean value
IRAM_ATTR void modeChangeInterupt(){
 changeButtonState = true; 
}

// A mode for the heart animation and the personal message :)
void patternMode(Adafruit_ILI9341 tft){
  
  tft.fillScreen(BLACK); // Sets the background
  displayImageTFT(tft,"/pattern/bottom-message.bmp",-5,220); // Manual offset due to the text not being quite centered...

  // Locations to store the positions of each animated sprite
  const uint16_t spriteSize = 32; // assumes square sprites, used for max border calculation
  const uint16_t maxSprites = 10; // Don't make this too big or collisions will grind the animation to a halt...
  const uint16 xBorderSprite = screenWidth-spriteSize;
  const uint16 yBorderSprite = screenLength-spriteSize-100; // 100 is the hight of the message sprite
  uint16_t posXArr[maxSprites]={0}; // Arrays to store the position of all the sprites in the animation
  uint16_t posYArr[maxSprites]={0};
  bool onScreen[maxSprites]={false}; // Array to store the state of each sprite
  const int refreshTime = 500; // Do the animation activity at the millisecond rate specified ("refresh it")
  uint32_t time1 = millis(); // timer variable for millis timer
  
  // Loop for the heart animations :)
  // Done by loading a sprite at a location where one isn't loaded
  // If one is loaded, load a "blank" sprite the same color as the background
  while(true){
    
    yield();
    // Exit upon encountering the mode change button
    if(changeButtonState){
      return;
    }

    if(millis()-time1>=refreshTime){

    //Pick a random sprite and see if it is loaded, if so clear it
    uint16_t spritePick = random(maxSprites);

    if(onScreen[spritePick]){
      // clear the sprite if it was already on the screen
      displayImageTFT(tft,"/pattern/clear-32.bmp",posXArr[spritePick],posYArr[spritePick]);
      onScreen[spritePick] = false;
    }else{
      // if the sprite was not loaded, pick a new location and load it in the screen
      uint16_t xPos = random(xBorderSprite);
      uint16_t yPos = random(yBorderSprite);

      // Detect sprite collision and re-roll position if detected
      bool collision  = false;
      do{
        collision=false;
        if(changeButtonState){
          return;
        }
        for(int i=0;i<maxSprites;i++){
          if(onScreen[i]){
            // Detect collision in the X plane
            if ( !(xPos<(posXArr[i]-spriteSize-1) || xPos>(posXArr[i]+spriteSize+1)) ){
              // Collision in the x direction detected, lets check the y direction
              if( !(yPos<(posYArr[i]-spriteSize-1) || yPos>(posYArr[i]+spriteSize+1)) ){
                Serial.println("Y Collision detected!");
                collision = true;
                // Roll cordinates again
                xPos = random(xBorderSprite);
                yPos=random(yBorderSprite);
                continue;
              }
            }
          }
        }
      }while(collision);

      posXArr[spritePick] = xPos;
      posYArr[spritePick] = yPos;
      onScreen[spritePick] = true;
      // Display the image once the proper cordinates have been loaded 
      displayImageTFT(tft,"/pattern/heart.bmp",xPos,yPos);
    
    }

      time1 = millis();// refresh the time variable
    }

  }

}

// Splash screen for when there is no SD card in the device
void errorMode(Adafruit_ILI9341 tft,String message){
  tft.fillScreen(BLACK);
  tft.setCursor(0,0);
  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  tft.println(message);
  tft.println(" ");
  tft.println("Please unplug and \nreplug the device");
}

void setup(void) {

  Serial.begin(9600);
  tft.begin();          // Initialize screen
  // The Adafruit_ImageReader constructor call (above, before setup())
  // accepts an uninitialized SdFat or FatVolume object. This MUST
  // BE INITIALIZED before using any of the image reader functions!
  Serial.print(F("Initializing filesystem..."));
  if(!SD.begin(SD_CS, SD_SCK_MHZ(25))) { // ESP32 requires 25 MHz limit
    Serial.println(F("SD begin() failed"));
      errorMode(tft,"SD card not detected");
    while(true){
      yield();
    }    
  }

  Serial.println(F("OK!"));

  pinMode(D1,INPUT);
  attachInterrupt(digitalPinToInterrupt(D1),modeChangeInterupt,FALLING);

}

void loop() {
  

  if(changeButtonState&&(millis()-prevStateChange)>=50){ // check if button has been pressed, and debounce by 50 ms
    Serial.println("State Change");
    currentState++;
    //loopback to first state (state 0) when max number of states is reached
    if(currentState>=maxStates){
      currentState=0;
    }
    changeButtonState=false;
    prevStateChange=millis();
  }else{
    changeButtonState=false;
  }

  // Switch between states
  switch (currentState)
  {
  case 0:
    displayAllImagesDir(tft,rootDir,changeButtonState);
    break;
  case 1:
    patternMode(tft);
    break;
  default: // By default do state 0, though it should never get here
    displayAllImagesDir(tft,rootDir,changeButtonState);
    break;
  }
}