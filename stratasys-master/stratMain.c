/* stratHacker_V2 Stratasys Cartridge Chip Hacker
 ** Copyright (c) 2015, sneaks.hacks <sneaks.hacks@gmail.com>
 ** All rights reserved.
 **
 **  Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions are met:
 **     * Redistributions of source code must retain the above copyright
 **       notice, this list of conditions and the following disclaimer.
 **     * Redistributions in binary form must reproduce the above copyright
 **       notice, this list of conditions and the following disclaimer in the
 **       documentation and/or other materials provided with the distribution.
 **     * Neither the name of the organization nor the
 **       names of its contributors may be used to endorse or promote products
 **       derived from this software without specific prior written permission.
 **
 ** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 ** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 ** DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 ** DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 ** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 ** LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ** ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 ** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **
 ** COMPILE INFO: c++ -o stratMain stratMain.c stratFunc.c -lwiringPi -lwiringPiDev -std=c++11
 ** TAR Compress: tar -cvf stratHacker_V2_002_02.tar ./stratHacker
 ** TAR Uncompress:  tar -xvf stratHacker_V2_002_02.tar
 **
 */



#include "stratFunc.h"          // stratHacker specific library
#include <wiringPi.h>           // wiringPi headers
#include <lcd.h>                // LCD headers from WiringPi
#include <stdio.h>              // needed for the printf function below
#include <dirent.h>             // used for searching directories
#include <string.h>             // string library
#include <iostream>             // input / output library
#include <fstream>              // file input output library
#include <unistd.h>             // used for sleep miliseconds
#include <chrono>               // used for our millis() function

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <stdlib.h>
#include <cstdio>
#include <memory>
#include <algorithm>



bool chipExistsFound = 0;       // indicates chip was found in log
int debug = 0;                  // debug mode
int newChipCount = 0;           // number of new chips
int loggedChips = 0;            // keeps track of logged chips
unsigned int modeNum = 0;       // represents current mode/menu number
unsigned int setting = 0;

const int leftPin = 5;          // left button input pin
const int upPin = 6;            // up button input pin
const int downPin = 12;         // down button input pin
const int rightPin = 16;        // right button input pin
const int selectPin = 20;       // select button input pin

const int redPin = 19;          // red LED output pin
const int greenPin = 13;        // green LED output pin
const int bluePin = 26;         // blue LED output pin

const int lcdPowerPin = 10;     // transistor pin that powers LCD & backlight
const unsigned int maxModes = 4;                        // maximum number of modes - for use with button navigation
unsigned int microSec = 1000000;                        // default sleep delay
std::string chipId = "23-";                             // chip folder prefix
std::string eedatPath = "/home/pi/stratHacker/eedat";   // path to our eedat file - info read from chip
std::string progPath = "/home/pi/stratHacker";          // program home directory
std::string w1Path = "/sys/devices/w1_bus_master1";     // 1-wire chip directory
std::string tempFilePath;                               // path to temp log file
std::string errorFilePath;                              // path to error log file
bool lookForChip = 0;                                   // indicate whether we are ready to look for chip
bool readOnly = 0;                                      // read mode - used to disable backup of chip to saved EE
bool hack = 1;                                          // flag to disable hacking halfway through process
std::string firstNew;                                   // first new chip found in directory search
bool cHack = 0;                                         // indicates custom hack
bool initBoot;                                          // indicates first time booting up
bool modeLFC = 0;                                       // if we need to go to a setting change prior to turning on LookForChip
unsigned int lastMode = 0;                              // record the last mode we were in

char* get_ip(std::string& network) {
    int fd;
    struct ifreq ifr;
    
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    
    ifr.ifr_addr.sa_family = AF_INET;
    
    strncpy(ifr.ifr_name, network.c_str(), IFNAMSIZ-1);
    
    ioctl(fd, SIOCGIFADDR, &ifr);
    
    close(fd);
    
    printf("%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    return  (char *) inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
}

std::string exec(const char* cmd) {                     // runs command and stores response as string/char*
    std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return "ERROR";
    char buffer[128];
    std::string result = "";
    while (!feof(pipe.get())) {
        if (fgets(buffer, 128, pipe.get()) != NULL)
            result += buffer;
    }
    return result;
}



int main()
{
    wiringPiSetupGpio();                                // initialise WiringPi
    pinMode(lcdPowerPin, OUTPUT);                       // set lcd / backlight pin as output (transistor)
    
    rgbLED newLED(redPin, greenPin, bluePin);           // create new RGB LED Object
    newLED.blue();                                      // turn on the LED - Blue
    
    digitalWrite(lcdPowerPin, HIGH);                    // turn on lcd and backlight
    
    stratHacker newChip(progPath, w1Path);              // create chip object
    errorFilePath = newChip.getErrorPath();             // get the folder path of the error log
    std::string prefPath = newChip.getPrefPath();       // get the folder path of the preference file
    
    std::string machT = newChip.readData(0, prefPath, "Machine Type");            // check for a stored machine type
    if(machT.compare(std::string()) == 0)                                          // if no machine is stored
    {
        modeNum = 44;                                   // boot in settings
        setting = 1;                                    // boot to Machine type
        initBoot = 1;                                   // we are in initial boot of stratHacker
    }
    newChip.bootSequence(0);                            // start stratHacker boot sequence
    
    
    button leftButton(leftPin, 20);                     // setup left button object
    button upButton(upPin, 20);                         // setup up button object
    button downButton(downPin, 20);                     // setup down button object
    button rightButton(rightPin, 20);                   // setup right button object
    button selectButton(selectPin, 20);                 // setup select button object
    shTimer lcdRotate(2000,2000);                       // timer for rotating lcd objects
    shTimer noChangeTimeout(20000);                     // inactivity timeout - if no change is made revert to main mode
    shTimer backBlue(10000);
    shTimer bootMessage(3000);                          // boot message timer
    
    
    while(1){
        
        newChip.errorCheckReset();                      // reset error level back to 0 (no error)
        newChipCount = newChip.newChips();
        while(1){                                       // main chip hacking mode
            
            
            if(rightButton.read() && !lookForChip){noChangeTimeout.resetTimer(); if(modeNum < maxModes){ modeNum++;} }      // if select button is pressed enter settings mode (44) / reset timer
            if(leftButton.read() && !lookForChip){noChangeTimeout.resetTimer(); if(modeNum > 0){ modeNum--;} }              // if select button is pressed enter settings mode (44) / reset timer
            
            //if(backBlue.timer()) newLED.blue();       // turn on blue LED
            char dispMess[20];
            
            newChipCount = newChip.newChips();          // Poll for New Chips that are not in the log
            
            if(newChipCount == 0){ sprintf(dispMess,"INSERT NEW CHIP", newChipCount); newLED.rgbOff();}
            
            else if(newChipCount == 1){ sprintf(dispMess,"PRESS SELECT", newChipCount); newLED.blue();}
            
            else if(newChipCount >1){ sprintf(dispMess,"(%d) Please Wait", newChipCount); newLED.red();}
            
            
            if(modeNum == 0){
                readOnly = 0;
                cHack = 0;
                if(!lookForChip){
                    // START - LCD Feedback ------------>
                    newChip.centerLCD("Clone Hack",0,0,1);
                    newChip.centerLCD(dispMess,1,0,1);
                    // END - LCD Feedback ------------>
                }
                if(selectButton.read() && newChipCount == 1)            // if select button is pressed and a nwe chip exists
                {
                    noChangeTimeout.resetTimer();                       // reset our timeout
                    lookForChip = !lookForChip;                         // start to look for chip
                }
            }
            
            if(modeNum == 1){
                readOnly = 0;
                cHack = 1;
                if(!lookForChip){
                    // START - LCD Feedback ------------>
                    newChip.centerLCD("Custom Hack",0,0,1);
                    newChip.centerLCD(dispMess,1,0,1);
                    // END - LCD Feedback ------------>
                }
                if(selectButton.read() && newChipCount == 1)            // if select button is pressed and a nwe chip exists
                {
                    noChangeTimeout.resetTimer();                       // reset our timeout
                    
                    lastMode = modeNum;                             // record the current Mode so we can return after setting is changed
                    modeNum = 44;                                   // go to settings
                    setting = 0;                                    // go to material select
                    modeLFC = 1;                                    // we need a setting before continuing
                    
                    
                }
            }
            
            if(modeNum == 2){
                readOnly = 1;
                cHack = 0;
                if(!lookForChip){
                    // START - LCD Feedback ------------>
                    newChip.centerLCD("Restore Backup",0,0,1);
                    newChip.centerLCD(dispMess,1,0,1);
                    // END - LCD Feedback ------------>
                }
                if(selectButton.read() && newChipCount == 1)            // if select button is pressed and a nwe chip exists
                {
                    noChangeTimeout.resetTimer();                       // reset our timeout
                    lookForChip = !lookForChip;                         // start to look for chip
                }
            }
            
            if(modeNum == 3){
                readOnly = 1;
                cHack = 0;
                if(!lookForChip){
                    // START - LCD Feedback ------------>
                    newChip.centerLCD("Read Chip",0,0,1);
                    newChip.centerLCD(dispMess,1,0,1);
                    // END - LCD Feedback ------------>
                }
                if(selectButton.read() && newChipCount == 1)            // if select button is pressed and a nwe chip exists
                {
                    noChangeTimeout.resetTimer();                       // reset our timeout
                    lookForChip = !lookForChip;                         // start to look for chip
                }
            }
            
            if(modeNum == 4){
                // START - LCD Feedback ------------>
                newChip.centerLCD("Settings",0,0,1);
                newChip.centerLCD("PRESS SELECT",1,0,1);
                // END - LCD Feedback ------------>
                if(selectButton.read()){noChangeTimeout.resetTimer(); modeNum = 44; lookForChip = 0;}   // if select button is pressed enter settings mode (44) / reset timer
            }
            
            
            
            if(lookForChip && newChipCount == 1)                        // if we are ready to look for a chip in the dock
            {
                newChip.progressBar(600000);                            // progress bar - delay to see if another chip was added recently
                newChipCount = newChip.newChips();                      // get chip count one last time
                if(newChipCount > 1) break;                             // if there are too many chips connected - break
                //newChip.moveMakeTemp(0);                                 // archive the tempChip.log and make a new one - PART OF ONE CHIP MODE TEST
                DIR *dir = opendir(w1Path.c_str());                     // location where stratasys chip shows up
                if(dir)                                                 // if 1-wire directory exists
                {
                    struct dirent *ent;                                 // create objecct
                    while((ent = readdir(dir)) != NULL)                 // while directories exist in 1-wire folder
                    {
                        std::string chipData(ent->d_name);              // store directory names as chip id
                        if(chipData[0] == chipId[0] && chipData[1] == chipId[1] && chipData[2] == chipId[2] )               // if directory matches first 3 letters of chip prefix
                        {
                            tempFilePath = newChip.getTempPath();       // get the folder path of the temp chip log
                            chipExistsFound  = newChip.checkExists(tempFilePath.c_str(),chipData.c_str(),"r",0);            // see if chip exists in our tempChip.log - add if it isn't
                            
                            
                            if (!chipExistsFound)                                                                           // if the chip was not found in our tempChip.log or we are in one Chip Mode
                            {
                                
                                
                                while(modeNum >= 0 && modeNum <= maxModes)
                                {
                                    
                                    lookForChip = 0;                                                                        // stop searching for the chip
                                    tempFilePath = newChip.getTempPath();                                                   // get the folder path of the temp chip log
                                    chipExistsFound  = newChip.checkExists(tempFilePath.c_str(),chipData.c_str(),"a+",1);   // see if chip exists in our tempChip.log - add if it isn't
                                    
                                    
                                    
                                    if(leftButton.read()){modeNum++;}                                               // if up button is pressed increase mode
                                    if(rightButton.read()){modeNum--;}                                              // if down button is pressed decrease mode
                                    
                                    
                                    bool errorChip = newChip.checkExists(errorFilePath.c_str(),chipData.c_str(),"r",0);             // check to see if the chip is in our error log
                                    if(errorChip)                                                                   // if the chip exists in our error log
                                    {
                                        printf("[!] WARNING: This chip has previously errored out...\n");           // notify user via console
                                    }
                                    
                                    newLED.red();                                                                   // turn on led - RED
                                    
                                    if(modeNum != 3){
                                        // START - LCD Feedback ------------>
                                        newChip.centerLCD("Chip Found",0,0,1);
                                        newChip.centerLCD("Preparing Hack",1,0,1);
                                        printf("[i] Preparing to Hack Chip...\n");
                                        usleep(microSec);
                                        // END - LCD Feedback ------------>
                                    }
                                    
                                    if(newChip.errorCheck(1,"Initialize Chip") >= 1){break;}                // CHECK FOR ERROR
                                    
                                    std::string chipIdData(ent->d_name);                                    // convert chip id to string
                                    newChip.storeChipID(chipIdData);                                        // store chip id to object and create paths
                                    if(newChip.errorCheck(1,"Store Chip ID") >= 1){break;}                                   // CHECK FOR ERROR
                                    
                                    // START - LCD Feedback ------------>
                                    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
                                    printf("StratHacker Hacking Initiated\n");
                                    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
                                    // END - LCD Feedback ------------>
                                    
                                    if(modeNum != 2 && modeNum != 4 )                                       // iuf we are not in read only mode
                                    {
                                        newChip.backupChip(0,readOnly);                                     // make backups of our new chip
                                        if(newChip.errorCheck(1,"Eeeprom Backup") >= 1){break;}             // CHECK FOR ERROR
                                    }
                                    
                                    if(modeNum !=2)                                                         // if we are not in restore mode
                                    {
                                        newChip.makeInput(0);                                               // create key file - required for decrypting
                                        if(newChip.errorCheck(1,"Make Input") >= 1){break;}                 // CHECK FOR ERROR
                                        newChip.eepromDecrypt(1,cHack);                                     // decrypt eeprom file (copied from chip)
                                        if(newChip.errorCheck(1,"Eeprom Decrypt") >= 1){break;}             // CHECK FOR ERROR
                                        
                                        
                                        // START - LCD Feedback ------------>
                                        printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
                                        printf("Current Cartridge Information\n");
                                        printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
                                        // END - LCD Feedback ------------>
                                        
                                        newChip.eedatRead(1);                                               // read eedat values
                                        if(newChip.errorCheck(1,"Eedat Read") >= 1){break;}                 // CHECK FOR ERROR
                                        
                                        // START Confirm ----------------->
                                        while(modeNum ==3)                                                  // if we are in read mode
                                        {
                                            newLED.yellow();                                                // turn on led - YELLOW
                                            newChip.centerLCD("SELECT WHEN DONE",1,0,1);                    // wait until done reading
                                            
                                            if(leftButton.read()){lookForChip = 0;  break;}
                                            if(selectButton.read()){lookForChip = 0;  break;}
                                            selectButton.lastState();                                       // SELECT - store last button reading as the last button state
                                            leftButton.lastState();                                         // LEFT - store last button reading as the last button state
                                            
                                        }
                                        
                                        // START Confirm ----------------->
                                        while(modeNum !=3)                                                  // if we aren't in read mode
                                        {
                                            newLED.yellow();                                                // turn on led - YELLOW
                                            newChip.centerLCD("CONTINUE HACK?",1,0,1);
                                            
                                            if(leftButton.read()){lookForChip = 0; hack = 0; break;}
                                            if(selectButton.read()){hack = 1; break;}
                                            selectButton.lastState();                                       // SELECT - store last button reading as the last button state
                                            leftButton.lastState();                                         // LEFT - store last button reading as the last button state
                                            
                                        }
                                        // END Confirm ----------------->
                                        
                                    }
                                    
                                    newLED.red();                                                           // turn on led - RED
                                    
                                    if(!hack && modeNum !=3)                                                // if we aren't in read mode
                                    {
                                        // START - LCD Feedback ------------>
                                        newChip.centerLCD("Job Cancelled",0,0,1);
                                        newChip.centerLCD("INSERT NEW CHIP",1,0,1);
                                        usleep(3000000);
                                        // END - LCD Feedback ------------>
                                        
                                        newLED.blue();                                                      // turn on led - BLUE
                                        lookForChip = 0;
                                        break;
                                    }                                                                       // if hack cancelled break out of loop / look for new chip
                                    
                                    if(modeNum <2)
                                    {
                                        newChip.checkCannister(0);
                                        if(newChip.errorCheck(1,"Cannister Serial") >= 1){break;}           // CHECK FOR ERROR
                                    }
                                    
                                    
                                    if(modeNum == 1)                                                        // if we are in custom mode
                                    {
                                        
                                        
                                        newChip.customHack(1);                                              // display/store saved preferences as settings to be written
                                        if(newChip.errorCheck(1,"Custom Settings") >= 1){break;}            // CHECK FOR ERROR
                                        
                                        // START Confirm ----------------->
                                        while(1)
                                        {
                                            newLED.yellow();                                                // turn on led - YELLOW
                                            if(leftButton.read()){lookForChip = 0; hack = 0; break;}        // wait for cancel
                                            if(selectButton.read()){hack = 1; break;}                       // wait for confirmation
                                            selectButton.lastState();                                       // SELECT - store last button reading as the last button state
                                            leftButton.lastState();                                         // LEFT - store last button reading as the last button state
                                            
                                        }
                                        // END Confirm ----------------->
                                        
                                    }
                                    else
                                    {
                                        newChip.maxVolume();                                                // if we aren't in custom mode - set chip volume to max - 100%
                                        if(newChip.errorCheck(1,"Max Volume") >= 1){break;}                 // CHECK FOR ERROR
                                    }
                                    
                                    newLED.red();                                                           // turn on led - RED
                                    
                                    if(!hack && modeNum !=3)
                                    {
                                        // START - LCD Feedback ------------>
                                        newChip.centerLCD("Job Cancelled",0,0,1);
                                        newChip.centerLCD("INSERT NEW CHIP",1,0,1);
                                        usleep(3000000);
                                        // END - LCD Feedback ------------>
                                        
                                        newLED.blue();                                                      // turn on led - BLUE
                                        
                                        lookForChip = 0;
                                        break;
                                    }
                                    
                                    if(modeNum <2)                                                          // if we are not in restore mode
                                    {
                                        newChip.eepromMakeBinary(1);                                        // create binary file
                                        if(newChip.errorCheck(1,"Make Binary") >= 1){break;}                // CHECK FOR ERROR
                                        newChip.eepromWrite(1,debug);                                       // backup/copy eeprom to chip - disabled in debug mode
                                        if(newChip.errorCheck(1,"Eeprom Write") >= 1){break;}               // CHECK FOR ERROR
                                        std::string successFilePath = newChip.getSuccessPath();             // get the folder path of the cannister success log
                                        std::string cannisterSerial = newChip.getCannister();               // get the cannister serial we just used
                                        //newChip.checkExists(successFilePath.c_str(),cannisterSerial.c_str(),"w+",1);            // check and write the serial to the cannister success
                                        newChip.storeData(1, successFilePath, "Last Serial Used",cannisterSerial.c_str(),1);
                                    }
                                    
                                    if(modeNum == 2)                                                        // if we are in restore mode
                                    {
                                        newChip.restoreBackup(1);
                                        if(newChip.errorCheck(1,"Restore Backup") >= 1){break;}
                                        newLED.green();                                                     // turn on led - GREEN
                                        
                                        // START - LCD Feedback ------------>
                                        newChip.centerLCD("Chip Repaired!",0,0,1);
                                        newChip.centerLCD("REMOVE CHIP",1,0,1);
                                        usleep(8000000);
                                        // END - LCD Feedback ------------>
                                        
                                    }
                                    
                                    if(modeNum <2)
                                    {
                                        newLED.green();                                                     // turn on led - GREEN
                                        
                                        // START - LCD Feedback ------------>
                                        newChip.centerLCD("Hack Successful!",0,0,1);
                                        newChip.centerLCD("REMOVE CHIP",1,0,1);
                                        usleep(8000000);
                                        // END - LCD Feedback ------------>
                                    }
                                    
                                    // START Button Check--------------->
                                    leftButton.lastState();                                                 // LEFT - store last button reading as the last button state
                                    upButton.lastState();                                                   // UP - store last button reading as the last button state
                                    downButton.lastState();                                                 // DOWN - store last button reading as the last button state
                                    rightButton.lastState();                                                // RIGHT - store last button reading as the last button state
                                    selectButton.lastState();                                               // SELECT - store last button reading as the last button state
                                    // END Button Check--------------->
                                    
                                    lookForChip = 0;                                                        // turn off the search for new chips
                                    newChipCount = 0;                                                       // reset the connected chip number to 0
                                    break;                                                                  // break out of loop - if this isn't here it will hack the same chip indefinately
                                    
                                }
                                
                            }
                            if(newChip.errorCheck(0," ") >= 1){break;}                                      // CHECK FOR ERROR
                        }
                        
                        if(newChip.errorCheck(0," ") >= 1){break;}                                          // CHECK FOR ERROR
                        
                    }
                    if(newChip.errorCheck(0," ") >= 1){break;}
                }
            }
            
            if(lookForChip && newChipCount > 1)
            {
                std::string display = std::to_string(newChipCount) + std::string("+ Chip Error");
                newChip.centerLCD(display,1,0,1);
                newLED.red();
                usleep(5000000);
                lookForChip = 0;
            }
            else if(lookForChip && !newChipCount)
            {
                newChip.centerLCD("INSERT NEW CHIP",1,0,1);
                usleep(3000000);
                lookForChip = 0;
            }
            
            // START Button Check--------------->
            leftButton.lastState();                                                                         // LEFT - store last button reading as the last button state
            upButton.lastState();                                                                           // UP - store last button reading as the last button state
            downButton.lastState();                                                                         // DOWN - store last button reading as the last button state
            rightButton.lastState();                                                                        // RIGHT - store last button reading as the last button state
            selectButton.lastState();                                                                       // SELECT - store last button reading as the last button state
            // END Button Check--------------->
            while(modeNum == 44){                                                                           // settings menu
                
                int maxSetting = 11;
                if(upButton.read() && setting < maxSetting){setting++; noChangeTimeout.resetTimer();}               // if up button is pressed increase setting / reset timeout
                
                if(downButton.read() && setting > 0)                                                        // if down button is pressed decrease setting / reset timeout
                {
                    setting--;
                    noChangeTimeout.resetTimer();
                }
                
                if(leftButton.read())
                {
                    modeNum = 3;
                    newChip.getPreferences();                                                               // get preferences from stored xxxPref.txt
                    noChangeTimeout.resetTimer();
                }                                                                                           // if left button is pressed go back to main / reset timeout
                
                
                if(setting == 0)                                                                            // set model type
                {
                    // START - LCD Feedback ------------>getMaterialPref
                    newChip.centerLCD("Set Model Type",0,0,1);
                    newChip.centerLCD("PRESS SELECT",1,0,1);
                    // END - LCD Feedback ------------>
                    if(selectButton.read() || modeLFC)                                                      // select this setting option
                    {
                        while(!noChangeTimeout.timer()){
                            std::string prefML = newChip.readData(0, prefPath, "Material Line");            // get the stored material line number
                            if(prefML.compare(std::string()) == 0) prefML = "1";
                            static unsigned int mLine = atoi(prefML.c_str());                                       // convert the stored line number above
                            std::string flp = "/home/pi/stratHacker/materialTypes.txt";                     // filepath of material type list
                            std::string matLine = newChip.getLineFromFile(flp, mLine);                      // get specific material name from file
                            unsigned int rL = newChip.lineCounter(flp);                                     // number of lines in the specified file
                            if(upButton.read() && mLine > 1){mLine--; noChangeTimeout.resetTimer();}        // if up button is pressed move up list
                            if(downButton.read() && mLine < rL){mLine++; noChangeTimeout.resetTimer();}     // if down button is pressed and we haven't reached the end of the file - move down list
                            if(selectButton.read() || leftButton.read())                                    // if select / left button is pressed save and go back a menu
                            {
                                noChangeTimeout.resetTimer();                                               // reset timeout
                                std::string storeLine = std::to_string(mLine);                              // convert line number to string for saving to config file
                                newChip.storeData(debug, prefPath, "Material Line",storeLine.c_str(),1);    // store the materials line number to preference file
                                newChip.storeData(debug, prefPath, "Material Type",matLine.c_str(),1);      // store actual material to file
                                // START - LCD Feedback ------------>
                                newChip.centerLCD(matLine.c_str(),0,0,1);                                   // show the current material
                                newChip.centerLCD("SAVED!",1,0,1);                                          // indicate it was saved
                                usleep(microSec);                                                           // pause
                                // END - LCD Feedback ------------>
                                newChip.getPreferences();                                                   // update stored preferences to object
                                printf("[i] Stored Material Type... %s\n",matLine.c_str());                 // print out stored material type
                                if(modeLFC){                                                                // makes custom mode jump to here to confirm material type
                                    modeLFC = 0;                                                            // we need material setting before our mode can continue - ie Custom Hack
                                    lookForChip = 1;                                                        // strat looking for a new chip
                                    modeNum = lastMode;                                                     // revert back to previous mode
                                }
                                break;                                                                      // break out of material select menu
                            }
                            
                            // START - LCD Feedback ------------>
                            newChip.centerLCD("SELECT TYPE",0,0,1);                                         // settings menu - select material
                            newChip.centerLCD(matLine.c_str(),1,0,1);                                       // display material from current line number
                            // END - LCD Feedback ------------>
                            
                            // START Button Check--------------->
                            leftButton.lastState();                                                         // LEFT - store last button reading as the last button state
                            upButton.lastState();                                                           // UP - store last button reading as the last button state
                            downButton.lastState();                                                         // DOWN - store last button reading as the last button state
                            rightButton.lastState();                                                        // RIGHT - store last button reading as the last button state
                            selectButton.lastState();                                                       // SELECT - store last button reading as the last button state
                            // END Button Check--------------->
                        }
                    }
                }
                
                if(setting == 1)                                                                            // set machine type
                {
                    // START - LCD Feedback ------------>
                    newChip.centerLCD("Set Machine Type",0,0,1);
                    newChip.centerLCD("PRESS SELECT",1,0,1);
                    
                    //machBuild(
                    // END - LCD Feedback ------------>
                    if(selectButton.read())                                                                 // select this setting option
                    {
                        while(!noChangeTimeout.timer()){                                                    // while we are not in timeout
                            std::string prefTL = newChip.readData(0, prefPath, "Machine Line");             // get the stored machine line number
                            if(prefTL.compare(std::string()) == 0) prefTL = "1";
                            static unsigned int machLine = atoi(prefTL.c_str());                            // get the stored machine line number
                            std::string flp = "/home/pi/stratHacker/machineTypes.txt";                      // filepath of machine type list
                            std::string machId = newChip.machBuild(machLine,0);                             // file name for encrypting / decrypting
                            std::string machName = newChip.machBuild(machLine,1);                           // machine Nickname
                            unsigned int rL = newChip.lineCounter(flp);                                     // number of lines in the specified file
                            if(upButton.read() && machLine > 1){machLine--; noChangeTimeout.resetTimer();}  // if up button is pressed move up list
                            if(downButton.read() && machLine < rL){machLine++; noChangeTimeout.resetTimer();} // if down button is pressed and we haven't reached the end of the file - move down list
                            if(selectButton.read() || leftButton.read())                                    // if select / left button is pressed save and go back a menu
                            {
                                noChangeTimeout.resetTimer();                                               // reset timeout
                                std::string storeLine = std::to_string(machLine);                           // convert line umber to string for saving to config file
                                newChip.storeData(debug, prefPath, "Machine Line",storeLine.c_str(),1);     // store the machine line number to preference file
                                newChip.storeData(debug, prefPath, "Machine Type",machId.c_str(),1);        // store actual machine to file
                                newChip.storeData(debug, prefPath, "Machine Nickname",machName.c_str(),1);        // store actual machine to file
                                // START - LCD Feedback ------------>
                                newChip.centerLCD(machName.c_str(),0,0,1);                                    // show the current material
                                newChip.centerLCD("SAVED!",1,0,1);                                          // indicate it was saved
                                usleep(microSec);                                                           // pause
                                // END - LCD Feedback ------------>
                                newChip.getPreferences();                                                   // update stored preferences to object
                                printf("[i] Stored Machine Type... %s(%s)\n",machName.c_str(),machId.c_str());                 // print out stored material type
                                if(initBoot){                                                               // if we are in initial bootup
                                    initBoot = 0;                                                           // turn off initial boot flag
                                    modeNum = 0;                                                            // revert to original mode
                                }
                                break;                                                                      // break out of material select menu
                            }
                            
                            // START - LCD Feedback ------------>
                            newChip.centerLCD("SELECT TYPE",0,0,1);                                         // settings menu - select material
                            newChip.centerLCD(machName.c_str(),1,0,1);                                        // display material from current line number
                            // END - LCD Feedback ------------>
                            
                            // START Button Check--------------->
                            leftButton.lastState();                                                         // LEFT - store last button reading as the last button state
                            upButton.lastState();                                                           // UP - store last button reading as the last button state
                            downButton.lastState();                                                         // DOWN - store last button reading as the last button state
                            rightButton.lastState();                                                        // RIGHT - store last button reading as the last button state
                            selectButton.lastState();                                                       // SELECT - store last button reading as the last button state
                            // END Button Check--------------->
                        }
                    }
                }
                
                
                if(setting == 2)                                                                            // set model type
                {
                    // START - LCD Feedback ------------>getMaterialPref
                    newChip.centerLCD("Set Chip Volume",0,0,1);
                    newChip.centerLCD("PRESS SELECT",1,0,1);
                    // END - LCD Feedback ------------>
                    if(selectButton.read())                                                                 // select this setting option
                    {
                        double volPer = newChip.storedVolume() * 1.77;                                      // get stored custom volume and convert to percentage
                        int storPer = volPer;
                        if (storPer > 100) storPer = 100;
                        if (storPer < 0) storPer = 0;
                        while(storPer % 5 != 0){                                                            // if volume not divisible by 5
                            storPer = storPer + 1;                                                          // increase until it is
                        }
                        
                        while(!noChangeTimeout.timer()){                                                    // while we haven't timed out from inactivity
                            
                            
                            // START - LCD Feedback ------------>
                            newChip.centerLCD("Adjust Volume",0,0,1);                                       // settings menu - select material
                            std::string dispVol = std::to_string(storPer) + std::string("%");
                            newChip.centerLCD(dispVol.c_str(),1,0,1);                                       // display material from current line number
                            // END - LCD Feedback ------------>
                            
                            if(downButton.read() && storPer > 0){storPer = storPer - 5; noChangeTimeout.resetTimer();}          // if up button is pressed move up list
                            if(upButton.read() && storPer <=95){storPer = storPer + 5; noChangeTimeout.resetTimer();}           // if down button is pressed move down list
                            
                            
                            if(selectButton.read() || leftButton.read())                                    // if select / left button is pressed save and go back a menu
                            {
                                noChangeTimeout.resetTimer();                                               // reset timeout
                                double vvol = storPer/1.77;                                                 // convert to double
                                if(vvol > 56.3) vvol = 56.3;                                                // if we are over the max, roll back to max
                                std::string newStorVal = std::to_string(vvol);                              // convert percenage back to Stratasys volume (56.3  = 100%)
                                newStorVal.erase(4,newStorVal.length());                                    // trim string to first 4 characters - ie 56.23234243 to 56.2
                                newChip.storeData(debug, prefPath, "Custom Volume",newStorVal.c_str(),1);   // store the machine line number to preference file
                                newChip.getPreferences();                                                   // update stored preferences to object
                                
                                // START - LCD Feedback ------------>
                                newChip.centerLCD(dispVol,0,0,1);                                           // show the current material
                                newChip.centerLCD("SAVED!",1,0,1);                                          // indicate it was saved
                                usleep(microSec);                                                           // pause
                                // END - LCD Feedback ------------>
                                
                                break;                                                                      // break out of material select menu
                            }
                            
                            
                            
                            // START Button Check--------------->
                            leftButton.lastState();                                                         // LEFT - store last button reading as the last button state
                            upButton.lastState();                                                           // UP - store last button reading as the last button state
                            downButton.lastState();                                                         // DOWN - store last button reading as the last button state
                            rightButton.lastState();                                                        // RIGHT - store last button reading as the last button state
                            selectButton.lastState();                                                       // SELECT - store last button reading as the last button state
                            // END Button Check--------------->
                        }
                    }
                }
                
                
                if(setting == 3)                                                                            // network settings
                {
                    // START - LCD Feedback ------------>
                    newChip.centerLCD("Clear Chip Log",0,0,1);
                    newChip.centerLCD("PRESS SELECT",1,0,1);
                    // END - LCD Feedback ------------>
                    if(selectButton.read())
                    {
                        printf("[+] Archiving tempChip.log... ");
                        newChip.bootSequence(0);                                                            // archive temp log
                        
                        // START - LCD Feedback ------------>
                        newChip.centerLCD("Chip Log",0,0,1);
                        newChip.centerLCD("Cleared",1,0,1);
                        printf("done\n");
                        usleep(3000000);
                        // END - LCD Feedback ------------>
                    }
                }
                
                
                
                
                if(setting == 4)                                                                            // network settings
                {
                    // START - LCD Feedback ------------>
                    newChip.centerLCD("Chip Status",0,0,1);
                    newChip.centerLCD("PRESS SELECT",1,0,1);
                    // END - LCD Feedback ------------>
                    if(selectButton.read())
                    {
                        while(!noChangeTimeout.timer())
                        {
                            if(selectButton.read() || leftButton.read()) break;                             // if select / left button is pressed save
                            unsigned int logNum = newChip.logCount(tempFilePath);
                            unsigned int errNum = newChip.logCount(errorFilePath);
                            unsigned int newNum = newChip.newChips();
                            std::string toDisplay = std::string("New: ") + std::to_string(newNum) + std::string(" / ") + std::string("Log: ") + std::to_string(logNum);
                            newChip.centerLCD(toDisplay,0,0,1);
                            std::string toDisplay2 = std::string("Error: ") + std::to_string(errNum);
                            newChip.centerLCD(toDisplay2,1,0,1);
                            
                            // START Button Check--------------->
                            leftButton.lastState();                                                         // LEFT - store last button reading as the last button state
                            selectButton.lastState();                                                       // SELECT - store last button reading as the last button state
                            // END Button Check--------------->
                        }
                    }
                }
                
                if(setting == 5)                                                                            // network settings
                {
                    // START - LCD Feedback ------------>
                    newChip.centerLCD("Wi-Fi Info",0,0,1);
                    newChip.centerLCD("PRESS SELECT",1,0,1);
                    // END - LCD Feedback ------------>
                    if(selectButton.read())
                    {
                        std::string mySSID = exec("iwgetid -r");
                        mySSID.erase(std::remove(mySSID.begin(), mySSID.end(), '\n'), mySSID.end());
                        std::string wifi = "wlan0";
                        std::string ipAddr = get_ip(wifi);
                        while(!noChangeTimeout.timer())                                                      // as long as we don't timeout
                        {
                            if(selectButton.read() || leftButton.read()) break;                             // if select / left button is pressed exit
                            // START - LCD Feedback -----Current IPCURRENT IP:",0,0,1);
                            newChip.centerLCD(mySSID.c_str(),0,0,1);
                            newChip.centerLCD(ipAddr.c_str(),1,0,1);
                            // END - LCD Feedback ------------>
                            
                            // START Button Check--------------->
                            leftButton.lastState();                                                         // LEFT - store last button reading as the last button state
                            selectButton.lastState();                                                       // SELECT - store last button reading as the last button state
                            // END Button Check--------------->
                        }
                    }
                }
                
                if(setting == 6)                                                                            // Show Ethernet Modes
                {
                    // START - LCD Feedback ------------>
                    newChip.centerLCD("Ethernet Info",0,0,1);
                    newChip.centerLCD("PRESS SELECT",1,0,1);
                    // END - LCD Feedback ------------>
                    if(selectButton.read())
                    {
                        std::string eth = "eth0";                                                           // adapter to get ip for
                        std::string ipAddr2 = get_ip(eth);                                                  // get ip address (function)
                        while(!noChangeTimeout.timer())                                                     // as long as we don't timeout
                        {
                            if(leftButton.read()) break;                                                    // if left button is pressed exit
                            if(selectButton.read()){                                                        // if select button is pressed
                                newChip.centerLCD("Releasing IP",1,0,1);
                                exec("sudo dhclient -r eth0");                                              // release ip address
                                usleep(1000000);                                                            // wait one second
                                newChip.centerLCD("Renewing IP",1,0,1);
                                exec("sudo dhclient eth0");                                                 // renew ip address
                                usleep(1000000);                                                            // wait one second
                            }
                            
                            // START - LCD Feedback ------------>
                            newChip.centerLCD("Ethernet IP",0,0,1);
                            newChip.centerLCD(ipAddr2.c_str(),1,0,1);                                       // dispay ethernet - 0.0.0.0 if none
                            // END - LCD Feedback ------------>
                            
                            // START Button Check--------------->
                            leftButton.lastState();                                                         // LEFT - store last button reading as the last button state
                            selectButton.lastState();                                                       // SELECT - store last button reading as the last button state
                            // END Button Check--------------->
                        }
                    }
                }
                
                if(setting == 7)                                                                            // Show Ethernet Modes
                {
                    // START - LCD Feedback ------------>
                    newChip.centerLCD("Start Ad-hoc",0,0,1);
                    newChip.centerLCD("PRESS SELECT",1,0,1);
                    // END - LCD Feedback ------------>
                    if(selectButton.read())
                    {
                        std::string adAnswer = exec("sudo bash ./startAdhoc.sh");                           // attempt to start Ad-Hoc and store result
                        while(!noChangeTimeout.timer())                                                     // as long as we don't timeout
                        {
                            if(selectButton.read() || leftButton.read()) break;                             // if select / left button is pressed exit
                            // START - LCD Feedback ------------>
                            newChip.centerLCD("Ad-Hoc Status",0,0,1);
                            adAnswer.erase(std::remove(adAnswer.begin(),adAnswer.end(), '\n'),adAnswer.end()); // remove line breaks
                            newChip.centerLCD(adAnswer.c_str(),1,0,1);                                       // dispay ethernet - 0.0.0.0 if none
                            // END - LCD Feedback ------------>
                            
                            // START Button Check--------------->
                            leftButton.lastState();                                                         // LEFT - store last button reading as the last button state
                            selectButton.lastState();                                                       // SELECT - store last button reading as the last button state
                            // END Button Check--------------->
                        }
                    }
                }
                
                if(setting == 8)                                                                            // Show Ethernet Modes
                {
                    // START - LCD Feedback ------------>
                    newChip.centerLCD("Stop Ad-hoc",0,0,1);
                    newChip.centerLCD("PRESS SELECT",1,0,1);
                    // END - LCD Feedback ------------>
                    if(selectButton.read())
                    {
                        std::string adAnswer = exec("sudo bash ./stopAdhoc.sh");                           // attempt to start Ad-Hoc and store result
                        while(!noChangeTimeout.timer())                                                     // as long as we don't timeout
                        {
                            if(selectButton.read() || leftButton.read()) break;                             // if select / left button is pressed exit
                            // START - LCD Feedback ------------>
                            newChip.centerLCD("Ad-Hoc Status",0,0,1);
                            adAnswer.erase(std::remove(adAnswer.begin(),adAnswer.end(), '\n'),adAnswer.end()); // remove line breaks
                            newChip.centerLCD(adAnswer.c_str(),1,0,1);                                       // dispay ethernet - 0.0.0.0 if none
                            // END - LCD Feedback ------------>
                            
                            // START Button Check--------------->
                            leftButton.lastState();                                                         // LEFT - store last button reading as the last button state
                            selectButton.lastState();                                                       // SELECT - store last button reading as the last button state
                            // END Button Check--------------->
                        }
                    }
                }
                
                
                
                if(setting == 9)                                                                            // Exit Software (closes program for remote reboot)
                {
                    // START - LCD Feedback ------------>
                    newChip.centerLCD("Exit sHV2",0,0,1);
                    newChip.centerLCD("PRESS SELECT",1,0,1);
                    // END - LCD Feedback ------------>
                    if(selectButton.read())
                    {
                        std::string noIP = "0.0.0.0";                                                       // string for no IP address
                        std::string wifi = "wlan0";                                                         // adapter to get ethernet ip
                        std::string eth = "eth0";                                                           // adapter to get ip for
                        std::string ipAddr = get_ip(eth);                                                   // get ip address (function)
                        if(noIP.compare(ipAddr) == 0)                                                       // if we are not connected to wifi
                        {
                            ipAddr = get_ip(wifi);                                                          // get wifi ip address
                            if(noIP.compare(ipAddr) == 0)                                                   // if we are not connected to ethernet
                            {
                                ipAddr = "No Network Found";                                                // display no network
                            }
                            
                        }
                        
                        
                        
                        // START - LCD Feedback ------------>
                        newChip.centerLCD("Exiting sHV2",0,0,1);
                        newChip.centerLCD(" ",1,0,1);
                        usleep(microSec*3);
                        newChip.centerLCD("Restart via SSH",0,0,1);
                        newChip.centerLCD(ipAddr.c_str(),1,0,1);
                        // END - LCD Feedback ------------>
                        exit(1);
                        
                    }
                }
                
                if(setting == 10)                                                                            // Reboot Mode
                {
                    // START - LCD Feedback ------------>
                    newChip.centerLCD("Reboot sHV2",0,0,1);
                    newChip.centerLCD("PRESS SELECT",1,0,1);
                    // END - LCD Feedback ------------>
                    if(selectButton.read())
                    {
                        // START - LCD Feedback ------------>
                        newChip.centerLCD("Rebooting",0,0,1);
                        newChip.centerLCD("Be Right Back!",1,0,1);
                        usleep(microSec*3);
                        newChip.centerLCD(" ",0,0,1);
                        newChip.centerLCD(" ",1,0,1);
                        // END - LCD Feedback ------------>
                        system("sudo reboot");
                        
                    }
                }
                
                if(setting == 11)                                                                            // Shutdown Mode
                {
                    // START - LCD Feedback ------------>
                    newChip.centerLCD("Shutdown shV2",0,0,1);
                    newChip.centerLCD("PRESS SELECT",1,0,1);
                    // END - LCD Feedback ------------>
                    if(selectButton.read())
                    {
                        // START - LCD Feedback ------------>
                        newChip.centerLCD("Shutting Down",0,0,1);
                        newChip.centerLCD("Goodbye!",1,0,1);
                        usleep(microSec*3);
                        newChip.centerLCD(" ",0,0,1);
                        newChip.centerLCD(" ",1,0,1);
                        // END - LCD Feedback ------------>
                        system("sudo shutdown -h now");
                        
                    }
                    
                }
                
                if(setting > maxSetting){setting = maxSetting;}                                                               // if beyond our max mode stop
                if(noChangeTimeout.timer()){modeNum = 0;}                                                   // timeout - go back to main menu
                
                // START Button Check--------------->
                leftButton.lastState();                                                                     // LEFT - store last button reading as the last button state
                upButton.lastState();                                                                       // UP - store last button reading as the last button state
                downButton.lastState();                                                                     // DOWN - store last button reading as the last button state
                rightButton.lastState();                                                                    // RIGHT - store last button reading as the last button state
                selectButton.lastState();                                                                   // SELECT - store last button reading as the last button state
                // END Button Check--------------->
                
            }
        }
    }
    
}








