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
 */

#ifndef stratFunc_h
#define stratFunc_h

#include <stdio.h>

#endif /* stratFunc_h */
#include <string.h>             // string library
#include <string>               // string library
#include <wiringPi.h>           // wiringPi headers
#include <lcd.h>                // LCD headers from WiringPi
#include <chrono>               // used for millis timer


class stratHacker
{
private:
    std::string _filePath;                                      // location to write file (/home/pi/stratHacker)
    std::string _w1Path;                                        // path to 1-wire chip folder (/sys/devices/w1_bus_master1)
    std::string _savedEE_Folder;                                // folder where we put backups (/savedEE/)
    std::string _savedEE_FullPath;                              // folder where we put backups (/home/pi/stratHacker/savedEE/)
    std::string _savedEE_ChipPath;                              // folder where we put backups (savedEE/23-xxxxxxxxxxxx/)
    std::string _backupFolder;                                  // folder where we put backups (savedEE/23-xxxxxxxxxxxx/YYMMDDMMSS/)
    std::string _oldestBackupFolder;                            // folder where we put backups (savedEE/23-xxxxxxxxxxxx/YYMMDDMMSS/)
    std::string _fullChipPath;                                  // path to 1-wire chip folder (/sys/devices/w1_bus_master1/23-xxxxxxxxxxxx/)
    std::string _nameChipPath;                                  // path to name file on chip (/sys/devices/w1_bus_master1/23-xxxxxxxxxxxx/name)
    std::string _idChipPath;                                    // path to id file on chip (/sys/devices/w1_bus_master1/23-xxxxxxxxxxxx/id)
    std::string _eepromChipPath;                                // path to eeprom file on chip (/sys/devices/w1_bus_master1/23-xxxxxxxxxxxx/eeprom)
    std::string _tempChipLog;                                   // name of temp chip log
    std::string _tempChipLogPath;                               // full path to temp chip log
    std::string _errorChipLog;                                  // name of error chip log
    std::string _errorChipLogPath;                              // full path of error chip log
    std::string _cannisterLog;                                  // name of cannister log
    std::string _cannisterLogPath;                              // full path of cannister log
    std::string _archives;                                      // name of archives - where tempChip.log is stored
    std::string _archivesPath;                                  // full path to archive
    std::string _prefFile = "preferences.txt";                  // name of program preferences file
    std::string _chipID;                                        // id number for chip
    std::string _stripChip;                                     // strip less hyphen
    std::string _manLot;                                        // stores manufacturing lot
    std::string _matType;                                       // stores material type
    std::string _decryptProgram;                                // program used for decryption
    std::string _key;                                           // key file for decrypting eeprom
    std::string _machine = "prodigy";                           // machine information
    std::string _decryptFiles;                                  // arguments for stratasys decryption
    std::string _dateMade;                                      // date cartridge manufacturers
    std::string _dateKnacked;                                   // cartridge use by date
    std::string _encryptFileName;                               // file to be used for encrypting
    std::string _newVolume;                                     // new cartridge vollume
    std::string _createBinaryProgram;                           // program for making binary file
    std::string _timeStamp;                                     // time stamp used for time stamps on files/folders
    std::string _keyHex;                                        // key - which is chip number backwards
    std::string _oldestBackup;                                  // oldest backup for current chip should be original
    std::string _errorMessage1;                                 // thrown error message 1
    std::string _errorMessage2;                                 // thrown error message 2
    std::string _lastTextLine;                                  // used in function to read specific line number
    std::string _machNickname;                                   // machine nickname - ie Dimension shows for prodigy
    
    
    unsigned int _materialPref = 1;                             // config file material type line number in main file
    std::string _storedMaterial;                                // name of stored material number above
    unsigned int _machinePref = 1;                              // config file machine type line number in main file
    std::string _storedMachineName;                             // name of stored machine above
    std::string _storedMachineId;                               // name of stored machine id above
    std::string _storedVolume;                                  // custom level fill - always defaults to full (56.3)
    bool _customReady = 0;                                      // determines whether we have all the info we need for a custom hack
    
    
    int _canSerialToWrite;                                      // cannister serial number we are writing
    double _curVolume;                                          // stores cartridge volume
    int _curVolPercent;                                         // current chip material volume
    int _canSerial;                                             // stores original cannister serial
    int lcd2;                                                   // 16x2 lcd setup class
    int _newCanSerial;                                          // new cannister serial number (inc by 1)
    int _errorLevel;                                            // error level thrown
    
    unsigned int _writeType = 0;                                // determines our write mode - ie default / model / support
    unsigned int _newChipCount = 0;                             // number of chips connected that are not in the chip log
    
    
public:
    stratHacker(std::string &filePath, std::string &w1Path){
        _filePath= filePath;                                                        // save main program file path to object
        _w1Path = w1Path;                                                           // save 1-wire chip path to object
        _savedEE_Folder = "savedEE";                                                // folder name where we save backups
        _savedEE_FullPath = _filePath + std::string("/") + _savedEE_Folder;         // full backup folder
        _tempChipLog = "tempChip.log";                                              // name we give to the temp chip log
        _tempChipLogPath = _filePath + std::string("/") + _tempChipLog;             // full path of temp chip log
        _errorChipLog = "errorChip.log";                                            // name we give to error chip log
        _errorChipLogPath = _filePath + std::string("/") + _errorChipLog;           // full path of error chip log
        _cannisterLog = "cannisterSerial.Log";                                      // name we give to cannister log
        _cannisterLogPath = _savedEE_FullPath + std::string("/") + _cannisterLog;   // full path of canister log
        _archives = "archives";                                                     // name we give to archive folder
        _archivesPath = _filePath + std::string("/") + _archives;                   // full path to archive folder
        _errorLevel = 0;                                                            // set defULT ERROR TO 0 - NO ERRORS
        wiringPiSetupGpio();                                                        // initialise WiringPi
        lcd2 = lcdInit (2,16,4,7,8,17,18,27,22,0,0,0,0);                            // initialize lcd
        
    };
    
    void storeChipID(std::string &chipID);                                          // save chip id to object
    void eedatRead(bool show);                                                               // read data from eedat file
    void makeInput(bool show);                                                               // creates the keyfile from the id we copied from the chip
    bool checkDirectory(bool show,const char* dirPath, const char* dirFind, bool makeDir);   // check to see if a directory exists
    void copyFiles(bool show,const char* src, const char* dst, const char* fileName);        // copy files
    void centerLCD(std::string text, int r, int c, bool cen);                       // centers strings on LCD
    void centerLCD(int text, int r, int c, bool cen);                               // centers int on LCD
    void centerLCD(unsigned int text, int r, int c, bool cen);                      // centers int on LCD
    void centerLCD(double text, int r, int c, bool cen);                            // centers double on LCD
    std::string stripString(std::string const& s, std::string const& k);            // strips identifiers from eedat strings
    bool eepromDecrypt(bool show, bool custom);                                                           // decrypt eeprom file
    void eepromWrite(bool show, int& wrtChip);                                                  // write hacked eeprom back to chip
    void eepromMakeBinary(bool show);                                                        // make binary file
    void backupChip(bool show, bool read);                                                              // backup chip contents
    void bootSequence(bool show);                                                            // initial boot sequence for program (ie move temp log)
    bool checkFileExists(const std::string& name);                                  // check if a file exists (and write if flag set)
    void timeStamp();                                                               // create a time stamp for files/folders
    bool getOldestFolder(const std::string& filePath);                              // locate the oldest backup folder
    int errorCheck(bool show, std::string errorMessage);                                                               // check to see if there was an error
    void errorCheckReset();                                                         // reset errror (set error level to 0)
    void restoreBackup(bool show);                                                           // restore chip from backup
    std::string getTempPath();                                                      // return temp chip log path
    std::string getErrorPath();                                                     // return error log path
    std::string getSuccessPath();                                                   // return cannister log path
    std::string getCannister();                                                     // return cannister serial being used for hack
    unsigned int getMaterialPref();                                                  // return stored material preference
    unsigned int getMachinePref();
    bool checkCannister(bool show);                                                          // make sure we aren't reusing a cannister serial
    void changeMaterial(std::string& matType){ _matType = matType;}                 // change the material from what was read on the chip
    unsigned int lineCounter(std::string& path);                                     // counts the number of lines in a specific file
    
    bool checkExists(const char* ffp, const char* checkFor, std::string rw, bool cl);                        // check if a chip exists in log (ie error/temp)
    int runProgram(bool show,const char* path, std::string errorMessage1, std::string errorMessage2, int errorLev);   // run shell command
    
    std::string getLineFromFile(std::string& filePath, unsigned int& lineNum);      // read text file and return text on line
    bool saveConfig(std::string& settingName, std::string& setting);                // save setting into config file
    bool getstoredMaterial();                                                       // get stored material type
    bool getstoredMachine();                                                        // get stored machine type
    bool getstoredVolume();                                                         // get stored volume
    std::string machBuild(unsigned int& lineNum, bool nickName);
    
    bool customHack(bool show);                                                              // set and hack with stored variables (on/off)
    double storedVolume();                                                          // return stored volume as a double
    void setStoredVol(std::string& stVol){_storedVolume = stVol;}                   // save new stored volume
    bool getCustom(){return _customReady;}
    void getPreferences();
    void maxVolume(){ _newVolume = "56.3";}                                        // volume of cartridge - 56.3 = 100%
    
    unsigned int logCount(std::string& logFile);                                     // counts entry in log files
    void progressBar(unsigned int micSec);
    unsigned int newChips();                                                        // number of New/inlogged chips connected
    unsigned int getNewChips(){ return _newChipCount;}
    void emptyFile(const char* ffp);
    std::string readData(bool show, std::string fPath, const char* dataName);      // open file, find data name, read value
    bool storeData(bool show, std::string& fPath, const char* dataName, const char* dataValue, bool fileMake);
    bool getStoredPref(bool show);
    std::string getPrefPath(){ return _filePath + std::string("/") + _prefFile;};   // path to our preferences file
    void moveMakeTemp(bool show);
    
};

class button
{
    
private:
    const int _buttonPin;                                                   // pin the button is connected to
    bool _reading = 0;                                                      // button pin read
    bool _buttonState;                                                      // the current reading from the input pin
    bool _lastButtonState = 0;                                              // the previous reading from the input pin
    unsigned long long _lastDebounceTime = 0;                               // the last time the output pin was toggled
    long _debounceDelay = 50;                                               // the debounce time; increase if the output flickers
    
public:
    button(const int pin, const long debounceDelay)                         // create object with pin number and debounce delay
    : _buttonPin(pin), _debounceDelay(debounceDelay)
    {
        wiringPiSetupGpio();                                                // initialize WiringPi
        pinMode(_buttonPin, INPUT);                                         // Set button pin as input
    }
    bool read();
    void lastState(){ _lastButtonState = _reading;}                         // store last button reading as the last button state
};



class shTimer
{
private:
    unsigned long long _millis;                                             // millis since epoch
    unsigned long long _currentMillis;                                      // current millis (since last reset)
    unsigned long _delay;                                                   // timer delay in millis
    unsigned long _onFor;                                                   // time we stay on for past delay
    
public:
    shTimer(unsigned long delay, unsigned long onFor) : _delay(delay), _onFor(onFor){};     // set a timer and set how long it is onFor
    shTimer(unsigned long delay) : _delay(delay){};                                         // set only the timer
    
    void setMillis(){ _millis =
        std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();}                     // set our millis()
    
    unsigned long long millis(){ return _millis;}                                           // return current millis
    
    void resetTimer(){ _currentMillis =
        std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();}                     // manual timer reset
    
    bool timer(){
        setMillis();                                            // get the current millis since epoch
        if(_millis - _currentMillis > _delay){                  // if we are past the set delay
            if(_millis - _currentMillis > _delay + _onFor){     // if we are past the delay and the onFor time
                resetTimer();                                   // Reset the timer (set currentMillis to millis)
                return 0;                                       // turn off
            }
            
            setMillis();                                        // get the current millis since epoch
            return 1;                                           // turn on
        }
        else
        {
            return 0;                                           // turn off
        }
    }
    
};


class rgbLED
{
private:
    
    const int _redPin = 19;          // red LED output pin
    const int _greenPin = 13;        // green LED output pin
    const int _bluePin = 26;         // blue LED output pin
    
public:
    rgbLED(const int redPin, const int greenPin, const int bluePin) : _redPin(redPin), _greenPin(greenPin), _bluePin(bluePin){
        wiringPiSetupGpio();
        pinMode(_bluePin, OUTPUT);
        pinMode(_redPin, OUTPUT);
        pinMode(_greenPin, OUTPUT);
        rgbOff();
    };
    
    void rgbOff(){
        digitalWrite(_bluePin, LOW);
        digitalWrite(_redPin, LOW);
        digitalWrite(_greenPin, LOW);
    }
    
    void blue(){
        digitalWrite(_bluePin, HIGH);
        digitalWrite(_redPin, LOW);
        digitalWrite(_greenPin, LOW);
    }
    
    void red(){
        digitalWrite(_bluePin, LOW);
        digitalWrite(_redPin, HIGH);
        digitalWrite(_greenPin, LOW);
    }
    
    void green(){
        digitalWrite(_bluePin, LOW);
        digitalWrite(_redPin, LOW);
        digitalWrite(_greenPin, HIGH);
    }
    
    void yellow(){
        digitalWrite(_bluePin, LOW);
        digitalWrite(_redPin, HIGH);
        digitalWrite(_greenPin, HIGH);
    }
    
    void teal(){
        digitalWrite(_bluePin, HIGH);
        digitalWrite(_redPin, LOW);
        digitalWrite(_greenPin, HIGH);
    }
    
    void white(){
        digitalWrite(_bluePin, HIGH);
        digitalWrite(_redPin, HIGH);
        digitalWrite(_greenPin, HIGH);
    }
    
    
    
};




