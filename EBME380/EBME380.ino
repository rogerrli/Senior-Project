#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>     // used to communicate with Teensy
#include <SdFat.h>     // used to interact with the SD card

#define sclk 13       // SCLK can also use pin 14
#define mosi 11       // MOSI can also use pin 7
#define cs   10       // CS & DC can use pins 2, 6, 9, 10, 15, 20, 21, 22, 23
#define dc   9        //  but certain pairs must NOT be used: 2+10, 6+9, 20+23, 21+22
#define rst  5        // RST can use any pin
#define sdcs 4        // CS for SD card, can use any pin
#define com 6         // communication line to pump
#define flexiforce A3 // flexiforce line in
#define SECS_PER_MIN  (60UL) //used to calculate time in HH:MM:SS
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24L)
#define numberOfSeconds(_time_) (_time_ % SECS_PER_MIN)  
#define numberOfMinutes(_time_) ((_time_ / SECS_PER_MIN) % SECS_PER_MIN) 
#define numberOfHours(_time_) (( _time_% SECS_PER_DAY) / SECS_PER_HOUR)

// Color definitions
#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF

// variables
Adafruit_ST7735 tft = Adafruit_ST7735(cs, dc, rst);
SdFat sd;
char fileName[11];
char dataFileName[12];
String time;
File datalog;
int maxValue;
int minValue;
int vol;
int row = 0;
int avg = 0;
int prevAVG = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;
int milliseconds = 0;
int pumpTimeReference = 0;
int screenTimeReference = 0;
int currentTime = 0;
int screenRefresh = 0;
int indexValue;
int maxArray[5];
int minArray[5];
int currentSample[20];
boolean pumpBoolean = false;

/**
  * Prints the given string to the screen as a character array.
  * @param text the text to be converted to a character array and printed to the screen.
  * @param size the size of the text to be printed to the screen.
  * @param reset the starting y position of text to be printed on the screen, set -1 to be based off of last printed text.
*/
void toScreen(String text, int size, int reset) {
  if (reset != -1) {
    row = reset;
  }
  tft.setCursor(5, row);
  char charArray[text.length() + 1];
  text.toCharArray(charArray, sizeof(charArray));
  tft.println(charArray);
  row += size * 10;
}

/**
  * Calculates the average values of any array
*/
int average(int array[]) {
  int total = 0;
  int nonZeroElements = 0;
  for (int i = 1; i < sizeof(array); i++){
    if (array[i] != 0){
      total = total + array[i];
      nonZeroElements++;
    }
  }
  return total / nonZeroElements;
}

/**
  * Adds new max value to array
  * Moves all values one index closer to 0
  * Inserts newest value at end
  */

void addMax(int maxVal) {
  for(int i = 1; i < 4; i++){
    maxArray[i] = maxArray[i+1];
  }
  maxArray[4] = maxVal;
}

void addMin(int minVal) {
  for(int i = 1; i < 4; i++){
    minArray[i] = minArray[i+1];
  }
  minArray[4] = minVal;
}

/**
  * Prints an arrow into the screen indicating whether the blood pressure has increased or decreased since the last reading
  * @param previous the previously measured blood pressure
  * # param current the currently measured blood pressure
*/
void printArrow(int previous, int current, int size) {
  if (previous > current) {    // decreased bp
    int x0 = 7 * size * 8;
    int y0 = row - 10.5 * size;
    tft.fillTriangle(x0, y0, x0 + 7 * size, y0, x0 + 3.5 * size, y0 + 7 * size, BLACK);
  }
  else if (previous < current) {    // increased bp
    int x0 = 7 * size * 8;
    int y0 = row - 3.5 * size;
    tft.fillTriangle(x0, y0, x0 + 7 * size, y0, x0 + 3.5 * size, y0 - 7 * size, BLACK);
  }
}

/**
  * Updates the time string to reflect the current timestamp
*/
void updateTime(boolean useMillis) {
  hours = numberOfHours(millis() / 1000);
  minutes = numberOfMinutes(millis() / 1000);
  seconds = numberOfSeconds(millis() / 1000);
  milliseconds = millis() % 1000;
  time = "";
  if (hours < 10) {
    time.concat("0");
  }
  time.concat(hours);
  time.concat(":");
  if (minutes < 10) {
    time.concat("0");
  }
  time.concat(minutes);
  time.concat(":");
  if (seconds < 10) {
    time.concat("0");
  }
  time.concat(seconds);
  if (useMillis){ //adds milliseconds to time string
    time.concat(":");
    if (milliseconds < 100){
      time.concat("0");
      if (milliseconds < 10){
        time.concat("0");
      }
    }
  time.concat(milliseconds);
  }
}

/**
  * Setting up for the main loop
*/
void setup(void) {
  pinMode(sdcs, OUTPUT);  // don't touch the SD card
  pinMode(com, OUTPUT);
  pinMode(flexiforce, INPUT);
  
  // display
  tft.initR(INITR_BLACKTAB);     // Black Tab hardware
  tft.setRotation(1);            // landscape orientation
  tft.fillScreen(WHITE);         // make screen background white
  tft.setTextColor(BLACK);       // make text on screen black 
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  
  // sd card - check presence
  toScreen("Initializing SD Card...", 1, 5);
  while (!sd.begin(sdcs)) {     // sd card missing
    tft.fillScreen(BLACK);
    tft.setTextColor(WHITE);
    toScreen("Card Not Present.", 1, 5);
    toScreen("Please Insert SD Card.", 1, -1);
    delay(1000);
  }
  tft.fillScreen(WHITE);
  tft.setTextColor(BLACK);
  toScreen("Card Initialized.", 1, 5);
  delay(1000);
  // sd card - create next available file
  int i = 0;
  String stringFileName = "DATA";
  String stringDataFileName = "X"; //creates data file for Excel exportation
  stringFileName.concat(i);
  stringFileName.concat(".txt"); //creates file DATA0.txt
  stringDataFileName.concat(stringFileName); //creates file XDATA0.txt
  toScreen(stringFileName.length() + 1, 1, -1);
  toScreen(sizeof(fileName), 1, -1);
  stringFileName.toCharArray(fileName, sizeof(fileName)); //changes to char array from string
  stringDataFileName.toCharArray(dataFileName, sizeof(dataFileName));
  toScreen(sizeof(fileName), 1, -1);
  while (sd.exists(fileName)) {
    i = i + 1;
    stringFileName = "DATA";
    stringDataFileName = "X";
    stringFileName.concat(i);
    stringFileName.concat(".txt"); //increments DATA
    stringDataFileName.concat(stringFileName);
    fileName[stringFileName.length() + 1];
    stringFileName.toCharArray(fileName, sizeof(fileName));
    stringDataFileName.toCharArray(dataFileName, sizeof(dataFileName));
    tft.fillScreen(WHITE);
  }
  //only one file can be open at a time
  datalog = sd.open(fileName, FILE_WRITE); //creates empty file
  datalog.close();
  datalog = sd.open(dataFileName, FILE_WRITE); //creates file with headers for first row in excel
  datalog.println("Blood Pressure\tTimeElapsed(Milliseconds)\tVolume Pumped");
  datalog.close();
  toScreen("Data will be saved in:", 1, -1);
  toScreen(fileName, 1, -1);
  delay(3000);
  
  // initialize screen
  tft.setTextSize(2);
  tft.fillScreen(WHITE);
  int bp = analogRead(flexiforce) / 13 + 65;
  toScreen("BP: ", 2, 5);
  prevAVG = bp;
  toScreen("Vol: " + String(vol), 2, -1);
  updateTime(false);
  toScreen("T: " + time, 2, -1); 
  digitalWrite(com, HIGH);
}

/**
  * Main loop to be repeated
*/
void loop() {
  int bp;
  if (millis() - screenTimeReference > 50){ //grabs value at 20Hz
    bp = analogRead(flexiforce) / 13 + 65;
    currentSample[indexValue] = bp; //puts element in array
    indexValue = indexValue + 1; //increments index
    screenTimeReference = millis(); //"resets" counter
    updateTime(true); //need to include millisecond to text documents
    datalog = sd.open(fileName, FILE_WRITE);
    datalog.println("BP:\t" + String(bp) + "\tTime:\t" + time);
    datalog.println("Volume Delivered:\t" + String(vol) + "\r\n");
    datalog.close();
    datalog = sd.open(dataFileName, FILE_WRITE);
    datalog.println(String(bp) + "\t" + millis() + "\t" + String(vol)); //IMPORTANT: SEPARATE VALUES WITH TAB (\t) ONLY
    datalog.close();
  } 
  
  if (millis() - screenRefresh > 1000){ //one second has elapsed since screen has refreshed
    //Initial Computations
    maxValue = 0;
    minValue = 999;
    for (int i = 0; i <= 20; i++){ //searches for min max values
      if (currentSample[i] > maxValue){
        maxValue = currentSample[i];
      }
      if (currentSample[i] < minValue){
        minValue = currentSample[i];
      }
    }
    datalog = sd.open(fileName, FILE_WRITE);
    datalog.println("MaxBP:\t" + String(maxValue) + "\tMinBP:\t" + String(minValue));
    datalog.close();
    addMax(maxValue); 
    addMin(minValue); 
    //Screen output
    tft.fillScreen(WHITE);  //resets screen
    avg = average(maxArray); 
    printArrow(prevAVG, avg, 2); //finds arrow direction
    updateTime(false);
    toScreen("BP: " + String(bp), 2, 5);
    toScreen("Vol: " + String(vol), 2, -1);
    toScreen("T: " + time, 2, -1);
    toScreen("Max: " + String(maxValue), 2, -1); //prints min/max
    toScreen("Min: " + String(minValue), 2, -1);
    indexValue = 0; //resets index for array
    prevAVG = avg; //creates "history" for comparison
    screenRefresh = millis(); //"resets" counter
  }
  
  /*
  Coomands the pump
  Do not create method for this, must run in parallel with other code with respect to time
  Has been modified to NOT use delay(), and instead compares time since 
  code started running
  */
  if (bp > average(maxArray) && millis() - pumpTimeReference > 6000){ //change first condition later
    pumpTimeReference = millis(); //reset reference time for next 3 if blocks
    digitalWrite(com, LOW); //sends LOW (start) value to pump
    pumpBoolean = true;
  }
  if (pumpTimeReference - millis() > 100 && pumpBoolean){ //signal needs to be at least 50 milliseconds for pump to register
    digitalWrite(com, HIGH); //terminates LOW signal in preparation for next LOW
    vol++;
  }
  if (pumpTimeReference - millis() > 1100 && pumpBoolean){ //run pump for 1 second before terminating again
    digitalWrite(com, LOW); //sends LOW (stop) value to pump
  }
  if (pumpTimeReference - millis() > 1200 && pumpBoolean){ //signal needs to be at least 50 milliseconds for pump to register
    digitalWrite(com, HIGH); //terminates LOW signal
    pumpBoolean = false;
    pumpTimeReference = millis(); //reset reference for parent if block
  }
}

