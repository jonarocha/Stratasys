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


#include "stratFunc.h"          // stratHacker Library
#include <string.h>             // needed for all std:string calls
#include <string>               // needed for all std:string calls
#include <stdio.h>              // essential c library
#include <stdlib.h>             // needed for atoi
#include <wiringPi.h>           // wiringPi headers
#include <lcd.h>                // LCD headers from WiringPi
#include <unistd.h>             // used for sleep miliseconds
#include <iostream>             // input / output library
#include <fstream>              // file input/output library
#include <dirent.h>             // directory library
#include <sys/stat.h>           // used for setting folder permissions in checkDirectory (ie S_IRWXU)
#include <algorithm>            // used to center text on LCD

unsigned int microseconds = 1000000;                        // default delay for LCD messages
#define ARRAY_SIZE 300                                      // default array size
char lineData[8][ARRAY_SIZE];                               // array for recorded data
char identifiers[8][ARRAY_SIZE];                            // array for titles in eedat
char* preferences[ARRAY_SIZE];                                // array for reading eedat file
char keyResults[1][ARRAY_SIZE];                             // array for getting key
int i;                                                      // for loop
std::string noData = "No Data";                             // for comparing NO Data on above values

// tool to count the number of lines in a file
unsigned int stratHacker::lineCounter(std::string& path){
    unsigned int lineCnt = 0;
    FILE *fFile;
    if(fFile = fopen(path.c_str(),"r"))
    {
        char lineData [100];                                                         // array for data being read
        while(fgets (lineData , 100 , fFile) != NULL)                             // read log file line by line
        {
            lineCnt++;
        }
        fclose(fFile);
        return lineCnt;
    }
    return 0;
}

// get machine type from machinePref.txt
std::string stratHacker::machBuild(unsigned int& lineNum, bool nickName){
    
    int line = 0;
    int i;                                                                          // for loop
    //int line = 0;                                                                 // for counting which line we are on
    std::string flp = "/home/pi/stratHacker/machineTypes.txt";                      // list of machines
    const char * fl = flp.c_str();
    FILE *openFile;                                                                 // open file declaration
    if (openFile = fopen(fl,"r")){                                                  // open the log file
        char lineData [50];                                                         // array for data being read
        while(fgets (lineData , 50 , openFile) != NULL)                             // read log file line by line
        {
            std::string fullMachName(lineData);                                     // get specific line from file
            std::string machName = fullMachName.substr(0, fullMachName.find("\n")); // remove line breaks
            std::size_t pos = machName.find(":");                                   // position of ":" in string
            std::string mId = machName.substr (pos+1);                              // get from ":" to the end
            std::string mName = machName.substr(0, machName.find(":"));             // the name of the machine
            line ++;                                                                // move to the next line
            
            if(line == lineNum){                                                    // if we are at the given line number
                fclose(openFile);                                                   // we are done - close the file
                _machinePref = lineNum;                                             // store line num as machine preference
                _storedMachineName = mName;                                         // store our new machine type
                _storedMachineId = mId;                                             // store machine ID - NOT needed
                if(nickName)return mName;                                           // if we are looking for the nickname of the machine
                if(!nickName)return mId;                                            // if we want the name required for encryption/decryption
            }
        }
        fclose(openFile);                                                           // we are done - close the file
    }
    return std::string();                                                           // if we can't find data
}

// function to store chip ans setup all file paths
void stratHacker::storeChipID(std::string &chipID){                                                             // needs to be ran prior to all other functions
    _chipID = chipID;                                                                                           // store incoming chip id
    _stripChip = chipID;                                                                                        // make copy of chip id for  - for stratasys-xxx.py
    _stripChip.erase(std::remove(_stripChip.begin(), _stripChip.end(), '-'), _stripChip.end());                 // chip id less they hyphen  - - for stratasys-xxx.py
    
    _fullChipPath = _w1Path + std::string("/") + chipID;                                                        // full 1-wire path of chip (name id eeprom)
    _nameChipPath = _fullChipPath + std::string("/") + std::string("name");                                     // full name path on chip
    _idChipPath = _fullChipPath + std::string("/") + std::string("id");                                         // full id path on chip
    _eepromChipPath = _fullChipPath + std::string("/") + std::string("eeprom");                                 // full eeprom path on chip
    _savedEE_ChipPath = _savedEE_FullPath + std::string("/") + chipID;                                          // savedEE path for backing up chip
    _backupFolder = _savedEE_ChipPath + std::string("/") + _timeStamp;                                          // full backup path with time stamped directory
    _cannisterLog = "cannisterSerial.Log";                                                                      // log for succesful serials - so we don't write the same serial twice
    _cannisterLogPath = _savedEE_FullPath + std::string("/") + chipID + std::string("/") + _cannisterLog;       // full cannister log path
    
}


void stratHacker::moveMakeTemp(bool show){
    bool tclExists = checkFileExists(_tempChipLogPath.c_str());                         // check if tempChip.log exists
    if(tclExists)                                                                       // if tempChip.log exits
    {
        // move tempChip.log to archives folder
        char moveFile[190];                                                             // array for command we will run
        sprintf(moveFile,"sudo mv -f %s %s/tempChip_%s.log",_tempChipLogPath.c_str(),_archivesPath.c_str(),_timeStamp.c_str());     // build our command
        runProgram(show,moveFile,"BACKUP TEMP LOG","[+] Moving tempChip.log file...",1);                                            // run our command
        emptyFile(_tempChipLogPath.c_str());                                            // create a new empty tempChip.log
    }
    else
    {
        emptyFile(_tempChipLogPath.c_str());                                            // create a new empty tempChip.log
    }
}

// function to run when stratHacker first starts - takes timeStamp, archive tempChip.log,
void stratHacker::bootSequence(bool show){
    printf("[i] Boot sequence started...\n");
    timeStamp();                                                                            // take a time stamp to add to filename backup
    bool logFolderExists = checkDirectory(show,_filePath.c_str(), _archives.c_str(), 1);    // check if archives folder exists
    if(logFolderExists){                                                                    // if folder exists/was made successfully
        moveMakeTemp(0);
    }
    getPreferences();                                                                       // load stored machine and material preferences
}

void stratHacker::getPreferences(){
    
    printf("[+] Loading preferences... done\n");
    std::string prefPath = _filePath + std::string("/") + _prefFile;                                            // path to our preferences file
    _storedMachineName = readData(0, prefPath, "Machine Type");                                                 // get stored machine type from preferences file
    _machNickname = readData(0, prefPath, "Machine Nickname");                                                  // get the machines Nickname - ie Dimension for Prodigy
    std::string ml = readData(0, prefPath, "Machine Line");                                                     // store line num as machine preference
    if(ml.compare(std::string()) ==0) ml = "1";
    int mlConv = atoi(ml.c_str());                                                                                // convert stored line to int and store it
    _machinePref = mlConv;
    printf("[i] Stored Machine Type... %s\n",_storedMachineName.c_str());                                       // print out stored material type
    _storedMaterial = readData(0, prefPath, "Material Type");                                                   // get stored material type from preferences file
    printf("[i] Stored Material Type... %s\n",_storedMaterial.c_str());                                         // print out stored material type
    _storedVolume = readData(0, prefPath, "Custom Volume");                                                     // get stored volume type from preferences file
    if(_storedVolume.compare(std::string()) == 0) _storedVolume = "56.3";                                        // if there is not a volume set use 100%
    printf("[i] Stored Volume... %s\n",_storedVolume.c_str());                                                  // print out stored volume
    
    
    if(_storedMachineName.compare(noData) != 0 && _storedMaterial.compare(noData) != 0 && _storedVolume.compare(noData) != 0)
    {
        _customReady = 1;                                                                                       // if all three settings exist - we can do custom chips
    }
}

// make backups of all necessary files needed for hacking process
void stratHacker::backupChip(bool show, bool read){
    checkDirectory(show,_filePath.c_str(),_savedEE_Folder.c_str(),1);                       // check / make savedEE directory
    checkDirectory(show,_savedEE_FullPath.c_str(),_chipID.c_str(),1);                       // check / make savedEE/Chip directory
    bool nameExists = checkFileExists(_nameChipPath);                                       // check to see if name file/path exists
    bool idExists = checkFileExists(_idChipPath);                                           // check to see if id file/path exists
    bool eepromExists = checkFileExists(_eepromChipPath);                                   // check to see if eeprom file/path exists
    if(nameExists && idExists && eepromExists)                                              // if name / id / eeprom files exist
    {
        copyFiles(show,_fullChipPath.c_str(),_filePath.c_str(),"name");                     // copy name to our working directory
        copyFiles(show,_fullChipPath.c_str(),_filePath.c_str(),"id");                       // copy id to our working directory
        copyFiles(show,_fullChipPath.c_str(),_filePath.c_str(),"eeprom");                   // copy eeprom to our working directory
        
        // START - LCD/Console Feedback ------------>
        printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
        printf("Making Incremental Chip Backup\n");
        printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
        if(show)
        {
            centerLCD("Incremental Chip",0,0,1);
            centerLCD("Backup Started",1,0,1);
            usleep(microseconds);
        }
        // END - LCD/Console Feedback ------------>
        
        if(!read)
        {
            checkDirectory(show,_savedEE_ChipPath.c_str(),_timeStamp.c_str(),1);            // check / make savedEE/Chip/timeStamp/ directory
            copyFiles(show,_fullChipPath.c_str(),_backupFolder.c_str(),"name");             // copy name from chip
            copyFiles(show,_fullChipPath.c_str(),_backupFolder.c_str(),"id");               // copy id from chip
            copyFiles(show,_fullChipPath.c_str(),_backupFolder.c_str(),"eeprom");           // copy eeprom from chip
        }
    }
    else
    {
        
        // START - LCD/Console Feedback ------------>
        printf("[+] Chip Incremental Backups... failed.\n");
        if(show)
        {
            centerLCD("Incremental Fail",0,0,1);
            centerLCD("Files Not Found",1,0,1);
            usleep(microseconds);
        }
        // END - LCD/Console Feedback ------------>
        
        _errorLevel = 1;                                                                    // throw error
    }
}

// creates the keyfile from the id we copied from the chip
void stratHacker::makeInput(bool show){
    
    // START - LCD/Console Feedback ------------>
    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    printf("Creating Decryption Key File  \n");
    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    printf("[+] Making the Key File... ");
    printf("Done\n");
    if(show)
    {
        centerLCD("Start Make Input",0,0,1);
        centerLCD(_chipID.c_str(),1,0,1);
        usleep(microseconds);
    }
    // END - LCD/Console Feedback ------------>
    
    FILE *infileid;                                             // declare files we are using
    char id_s[8];                                               // array key is read into
    char keyhex[8];                                             // array for new way to store key using sprintf
    int a;                                                      // used in read for loop
    std::string idPath = _filePath + "/id";                     // set filepath for our /id file
    infileid = fopen (idPath.c_str(), "r");                     // open id file - READ
    for (a=0; a<=7; a++)                                        // read data in id file into our id_s array
    {
        id_s[a] = fgetc (infileid);                             // copy array one char at a time
    }
    fclose(infileid);                                           // close the id file
    sprintf (keyhex, "%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x", id_s[7],id_s[6],id_s[5],id_s[4],id_s[3],id_s[2],id_s[1],id_s[0]);      // reverse id_s as keyhex
    _key = keyhex;                                              // save the new key hex to our object
    
    // START - LCD/Console Feedback ------------>
    printf("[i] Keyfile Data... ");
    printf("%s\n", _key.c_str());
    if(show)
    {
        centerLCD("Keyfile ID",0,0,1);
        centerLCD(_key.c_str(),1,0,1);
        usleep(microseconds);
    }
    // END - LCD/Console Feedback ------------>
}

// decrypt our eeprom file using the key file made in our makeInput function
bool stratHacker::eepromDecrypt(bool show, bool custom){
    if(custom)
    {
        if(!_storedMachineName.empty())                                                       // if there is a stored machine preference
        {
            _machine = _storedMachineName;                                                    // set stored machine as default machine
        }
        else
        {
            // START - LCD/Console Feedback ------------>
            printf("[!] No Machine Type has been picked... \n");
            centerLCD("Custom Error",0,0,1);
            centerLCD("Machine Not Set",1,0,1);
            usleep(microseconds);
            // END - LCD/Console Feedback ------------>
            return 0;                                                                       // fail - can't decrypt without machine name
        }
    }
    
    else
    {
        // START - LCD/Console Feedback ------------>
        printf("[i] Stored machine type: %s\n",_machine.c_str());
        // END - LCD/Console Feedback ------------>
    }
    char programPath[300];                                                                  // array for linux shell command created in sprintf
    _decryptProgram = "stratasys-cartridge.py info";                                       // program used for eeprom decryption
    sprintf(programPath,"sudo python2 %s/%s -t %s -e %s -i eeprom",_filePath.c_str(),_decryptProgram.c_str(),_machine.c_str(),_key.c_str()); // run shell command
    // START - LCD/Console Feedback ------------>
    printf("Running: %s\n ",programPath);
    // END - LCD/Console Feedback ------------>
    runProgram(show,programPath,"Eeprom Decrypt","[+] Decrypting eeprom file... ",1);       // display status in console
    return 1;                                                                               // decryption successful
}

bool stratHacker::storeData(bool show, std::string& fPath, const char* dataName, const char* dataValue, bool fileMake){
    
    
    if(show) printf("[+] Updating %s to %s...",dataName, dataValue);
    
    FILE *infile;                                                                       // file to be opened declaration
    char id_s[300];                                                                     // array for incoming data from eedat
    int line = 0;                                                                       // line number index
    bool dataFound = 0;                                                                 // indicates whether data is in our file
    std::string dataNameStr;
    if(infile = fopen (fPath.c_str(), "r"))                                             // if we can open specified file - READ
    {
        
        char buffer[300];                                                               // buffer for data being read from the file
        while(fgets(buffer, 300, infile) != NULL){                                      // as long as the incoming data is not null
            
            std::string incData = buffer;                                               // convert incoming data as string for manipulation
            incData = incData.substr(0,incData.find(":"));                              // find the location of the ':'
            dataNameStr = dataName;                                                     // convert dataName arg to string for comparing
            if(incData.compare(dataNameStr) == 0)                                       // if incoming data name matches our arg
            {
                dataFound = 1;                                                          // we found our dataName in the file
                dataNameStr = dataNameStr + std::string(": ") + std::string(dataValue) + std::string("\n");     // reconstruct the string for being written
                char *a=new char[dataNameStr.size()+1];                                 // step one converting string to char* array - make char that matches size of incoming data
                a[dataNameStr.size()]=0;                                                // step two converting string to char* array
                memcpy(a,dataNameStr.c_str(),dataNameStr.size());                       // step three converting string to char* array - copy string to char
                preferences[line] = strndup(a, 300);                                    // step three converting string to char* array - copy char to our char array
                line++;                                                                 // move to next line
            }
            else                                                                        // if line does not match our argument
            {
                preferences[line] = strndup(buffer, 300);                               // store incoming data to our preferences array
                line++;                                                                 // move to next line
            }
        }
        fclose(infile);                                                                 // close our file
        line = 0;                                                                       // reset our index
        infile = fopen (fPath.c_str(), "w");                                            // open an empty file in preparation of writing data from our array
        
        while (preferences[line] != NULL)                                               // while we are not at the end of the file
        {
            fputs (preferences[line],infile);                                           // add data to file one line at a time
            preferences[line] = '\0';                                                   // delete array contents after use
            line++;                                                                     // increase line index
        }
        
        if(!dataFound)                                                                  // if we didn't find the arg data in our file
        {
            dataNameStr = dataName;                                                     // convert dataName arg to string for comparing
            dataNameStr = dataNameStr + std::string(": ") + std::string(dataValue) + std::string("\n");     // reconstruct the string for being written
            fputs (dataNameStr.c_str(),infile);                                         // write above data to file
            if(show) printf(" added\n");
            fclose(infile);                                                             // close our file
            return 1;                                                                   // return success
            
        }
        if(show) printf(" updated\n");
        fclose(infile);                                                                 // close our file
        return 1;                                                                       // return success
        
    }
    else
    {
        if(fileMake)                                                                    // if we have a flag to make the file
        {
            FILE *infile;                                                               // file to be opened declaration
            int line = 0;                                                               // line number index
            if(infile = fopen (fPath.c_str(), "w"))                                     // if we can open specified file - READ
            {
                dataNameStr = dataName;
                dataNameStr = dataNameStr + std::string(": ") + std::string(dataValue) + std::string("\n");
                fputs (dataNameStr.c_str(),infile);                                    // write data in argument to our file
            }
            else
            {
                if(show) printf(" create failed\n");
                return 0;
            }
            fclose(infile);                                                             // close file
            if(show) printf(" created\n");
            return 1;
        }
        if(show) printf(" failed\n");
        return 0;
    }
}


// read data from specified file / dataname
std::string stratHacker::readData(bool show, std::string fPath, const char* dataName){
    if(show) printf("[+] Looking for %s in %s... ",dataName,fPath.c_str());
    
    FILE *infile;                                                                       // file to be opened declaration
    char id_s[200];                                                                     // array for incoming data from eedat
    int line = 0;                                                                       // line number index
    
    if(infile = fopen (fPath.c_str(), "r"))                                                // open specified file - READ
    {
        if (infile == NULL)                                                                 // if we can't find the file
        {
            perror ("Error opening file");
            _errorLevel = 1;                                                                // throw error
            return std::string();
        }
        else
        {
            while ( fgets (id_s , 200 , infile) != NULL )                                   // until we read the end of the data file
            {
                std::string incData = id_s;                                                 // convert incoming data as string for manipulation
                incData = incData.substr(0,incData.find(":"));                              // find the location of the ':'
                std::string dataNameStr = dataName;                                         // convert dataName arg to string for comparing
                if(incData.compare(dataNameStr) == 0)                                       // if incoming data name matches our arg
                {
                    fclose (infile);                                                        // close the file - nothing left to read
                    if(show) printf(" found\n");                                            // console confirmation of data existing
                    incData = id_s;                                                         // store entire string to incData (previously trimmed to name only)
                    std::string removeName = dataNameStr + std::string(": ");               // string we are going to remove - dataName and ': '
                    std::string readValue = stripString(incData,removeName);                // strip the above data name - leaving only the value
                    readValue.erase(std::remove(readValue.begin(), readValue.end(), '\n'), readValue.end());               // remove all line breaks from string
                    if(show)printf("[i] %s: %s\n",dataName,readValue.c_str());              // troubleshooting - DELETE
                    return readValue;
                }
            }
        }
        
        fclose (infile);// close the file
    }
    if(show) printf(" not found\n");                                                        // console confirmation of data existing
    if(show) printf("[i] %s doesn't exist...\n",fPath.c_str());                                            // console confirmation of data existing
    
    return std::string();
}

// read the data decrypted from eeprom file
void stratHacker::eedatRead(bool show){
    // START - LCD/Console Feedback ------------>
    if(show)
    {
        centerLCD("Reading Chip",0,0,1);
        centerLCD("Data",1,0,1);
        usleep(microseconds);
    }
    // END - LCD/Console Feedback ------------>
    
    std::string eedatPath = "/home/pi/stratHacker/eedat";                               // path to our eedat file - info read from chip
    std::string readCannister = readData(0, eedatPath, "Canister S/N");                 // get cannister serial from eedat
    _canSerial = atoi(readCannister.c_str());                                           // convert serial string to
    //_newCanSerial = ++_canSerial;                                                     // save new serial number to object
    
    // START - LCD/Console Feedback ------------>
    printf("[i] Old Cannister Serial Number: ");
    printf ("%d\n",_canSerial);
    if(show)
    {
        centerLCD("Chip Cannister",0,0,1);
        centerLCD(_canSerial,1,0,1);
        usleep(microseconds);
    }
    // END - LCD/Console Feedback ------------>
    
    _matType = readData(0, eedatPath, "Material type");                      // save material type to object
    
    // START - LCD/Console Feedback ------------>
    printf("[i] Material Type: ");
    printf ("%s\n",_matType.c_str());
    // END - LCD/Console Feedback ------------>
    
    _manLot = readData(0, eedatPath, "Manufacturing lot");                              // create string from manufacturing lot
    
    
    std::string readCartridge = readData(0, eedatPath, "Current material qty");
    _curVolume = atof(readCartridge.c_str());                                                // convert volume to double
    
    int curVolPercent = (_curVolume/56.3) * 100;                                         // calculate volume percentage
    _curVolPercent = curVolPercent;                                                     // save percentage to object
    char volPer[10];                                                                    // array to store percentage
    sprintf(volPer,"%d%%",_curVolPercent);                                              // read percentage into array
    std::string strVolPer = volPer;                                                     // convert array to string
    
    // START - LCD/Console Feedback ------------>
    printf("[i] Current Volume: ");
    printf ("%d%%\n",_curVolPercent);
    if(show)
    {
        std::string combine = _matType + std::string("  ") + strVolPer;                  // combine values to show on LCD
        centerLCD("Existing Data",0,0,1);
        centerLCD(combine.c_str(),1,0,1);
        usleep(microseconds*2);
        centerLCD(combine.c_str(),0,0,1);
    }
    // END - LCD/Console Feedback ------------>
    
    // START - LCD/Console Feedback ------------>
    printf("[i] Reading eedat... ");
    printf("Done\n");
    // END - LCD/Console Feedback ------------>
}










// use stored chip settings to write the chip - custom hack mode
bool stratHacker::customHack(bool show){
    
    if(_storedMachineName.empty() || _storedMaterial.empty())                             // if there is not a stored machine id - error out
    {
        _errorLevel = 1;                                                                // throw error
        return 0;                                                                       // custom hack failed
    }
    else
    {
        _machine = _storedMachineName;                                                    // set machine type to stored value
        _matType = _storedMaterial;                                                     // set material to stored value
        _newVolume = _storedVolume;                                                     // set volume type to stored value
        
        // START - LCD/Console Feedback ------------>
        printf("[+] Using custom machine on the chip... %s (%s)\n",_machine.c_str(),_storedMachineName.c_str());
        printf("[+] Using custom material on the chip... %s\n",_matType.c_str());
        printf("[+] Using custom volume on the chip... %s\n",_storedVolume.c_str());
        // END - LCD/Console Feedback ------------>
        
        double n = atof(_storedVolume.c_str()) * 1.7761;                                // convert volume string to double for percentage
        int percent = n;                                                                // create int percent variable
        if (percent == 99) percent = 100;                                               // if percent at 99 - convert to 100
        
        // START - LCD/Console Feedback ------------>
        if(show)
        {
            std::string lcdOne = _storedMaterial + std::string("  ") + std::to_string(percent) + std::string("%");      // build string to show
            std::string lcdTwo = _machNickname;
            centerLCD("Custom Chip Data",0,0,1);
            centerLCD(lcdTwo.c_str(),1,0,1);
            usleep(microseconds*2);
            centerLCD(lcdTwo.c_str(),0,0,1);
            centerLCD(lcdOne.c_str(),1,0,1);
            usleep(microseconds*2);
            centerLCD(lcdOne.c_str(),0,0,1);
            centerLCD("CONTINUE HACK?",1,0,1);
        }
        // END - LCD/Console Feedback ------------>
        return 1;                                                                       // custom hack success
    }
}

// check to make sure we haven't previously written this cannister serial with a newer serial number
bool stratHacker::checkCannister(bool show){
    
    int oldSerial;
    int newSerial = _newCanSerial;                                                                              // our newest stored serial numner
    
    std::string lastChip = readData(1, _cannisterLogPath, "Last Serial Used");                                  // get the last serial number used from the log file
    
    if(lastChip.compare(std::string()) != 0)                                                                    // if there is a chip serial in the cannister log (string is not empty)
    {
        oldSerial = atoi(lastChip.c_str());                                                                     // convert stored serial to int for manipulation
        // START - LCD/Console Feedback ------------>
        printf("[i] Chip has been previously hacked... \n");
        printf("[i] Last Cannister Serial %d \n",oldSerial);                                                    // send last serial used to console
        // END - LCD/Console Feedback ------------>
        if(oldSerial > newSerial)                                                                               // if our old serial is higher than our new serial
        {
            newSerial = oldSerial;                                                                              // update our new serial to the loged number
        }
        if(newSerial < _canSerial)
        {
            newSerial = _canSerial;
        }
        if(newSerial < 9999999){ _errorLevel = 1; return 0;}                                                    // if our serial number is too low - exit and throw error
        _canSerialToWrite = newSerial + 1;                                                                          // store serial to object as serial to be used
        // START - LCD/Console Feedback ------------>
        printf("[+] Updating Canister Serial to %d\n", _canSerialToWrite);                                              // send new serial to console
        // END - LCD/Console Feedback ------------>
        return 1;
        
    }
    
    else                                                                                                        // if there is not a cannister log for this chip
        
    {
        if(newSerial < _canSerial)
        {
            newSerial = _canSerial;
        }
        if(newSerial < 9999999){ _errorLevel = 1; return 0;}                                                    // if our serial number is too low - exit and throw error
        _canSerialToWrite = newSerial+1;
        // START - LCD/Console Feedback ------------>
        printf("[i] Chip has not previously hacked... \n");                                                     // send info to the console
        printf("[+] Using Canister Serial to %d\n", _canSerialToWrite);                                                 // send serial to be used to the console
        // END - LCD/Console Feedback ------------>
        return 0;
    }
    
    return 0;                                                                                               // chip not in log - return false
}


// using all of the values we have stored - create a binary file
void stratHacker::eepromMakeBinary(bool show){
    
    _dateMade = "2014-01-01 01:02:03";                          // date manufacturers
    _dateKnacked = "2018-01-01 01:02:03";                       // use by date
    _encryptFileName = "newee.bin";                             // name of new eeprom
    std::string iniVolume = "56.3";                             // initial cartridge volume
    
    char programPath[300];
    sprintf(programPath,"sudo python2 ./stratasys-cli.py eeprom -t %s -e %s -s %d -m %s -l 1234 -d \"%s\" -u \"%s\" -n %s -c %s -k %s -v 1 -g STRATASYS -o %s",_machine.c_str(), _stripChip.c_str(), _canSerialToWrite, _matType.c_str(),_dateMade.c_str(),_dateKnacked.c_str(),iniVolume.c_str(),_newVolume.c_str(),_key.c_str(),_encryptFileName.c_str());
    runProgram(show,programPath,"Building Eeprom","[+] Building new eeprom file... ",1);   // run shell comand
}


void stratHacker::eepromWrite(bool show, int &wrtChip){
    
    // COPY HACKED EEPROM INTO OUR BACKUP FOLDER
    char programPath[190];                                      // array for linux shell command created in sprintf
    sprintf(programPath,"sudo cp %s/newee.bin %s/hacked_eeprom",_filePath.c_str(),_backupFolder.c_str());   // create string of command to be run
    runProgram(show,programPath,"Backup Eeprom","[+] Backing up copy of hacked eeprom... ",1);   // run shell comand
    
    // START - LCD/Console Feedback ------------>
    centerLCD("New Eeprom",0,0,1);
    centerLCD("Backup... Done",1,0,1);
    printf("[+] Eeprom Backed up to %s ... ",_backupFolder.c_str());
    usleep(microseconds);
    printf("done.\n");
    // END - LCD/Console Feedback ------------>
    
    // WRITE OUR HACKED EEPROM TO THE CHIP
    char programPath2[190];                                     // array for linux shell command created in sprintf
    sprintf(programPath2,"sudo cp %s/newee.bin %s/eeprom",_filePath.c_str(),_fullChipPath.c_str());     // create string of command to be run
    if(!wrtChip)                                                                                        // if we are in not in debug mode
    {
        runProgram(show,programPath2,"Write to Chip","[i] Writing eeprom to chip... ",99);                   // copy new eeprom back to chip
        
        // START - LCD/Console Feedback ------------>
        printf("[+] Eeprom Written to %s ... ",_fullChipPath.c_str());
        printf("done.\n");
        
        // END - LCD/Console Feedback ------------>
    }
    else
    {
        // START - LCD/Console Feedback ------------>
        printf("[+] Debug Mode - Eeprom Not Written to %s ... ",_fullChipPath.c_str());
        printf("debug.\n");
        if(show)
        {
            centerLCD("Debug Mode",0,0,1);
            centerLCD("Chip Not Written",1,0,1);
            usleep(microseconds);
        }
        // END - LCD/Console Feedback ------------>
    }
    
}

// checks to see if a directory exists and depending on the mkDir flag - makes it
bool stratHacker::checkDirectory(bool show,const char* dirPath, const char* dirFind, bool makeDir){
    
    std::string fullDirPath = dirPath + std::string("/") + dirFind;             // full path of directory we are looking for
    printf("[+] Checking for directory %s... ",dirPath);                        // print status to console
    
    DIR *dPath = opendir(dirPath);                                              // directory we are searching in
    DIR *dFind = opendir(fullDirPath.c_str());                                  // directory we are searching for
    
    if(dPath)                                                                   // if the directory we are looking in exists
    {
        printf(" exists.\n");                                                   // print status to console
        printf("[+] Checking for sub-directory %s... ",dirFind);                // print status to console
        
        if(makeDir && !dFind)                                                   // is directory doesn't exist and amke directory flag is set - make dir
        {
            printf(" not found.\n");                                            // print status to console
            std::string fullPath = dirPath + std::string("/") + dirFind;        // set full path of directory to be made
            const int dir_err = mkdir(fullPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);     // make directory with proper R/W settings
            
            // START - LCD/Console Feedback ------------>
            printf("[+] Making Directory... ");
            printf("%s\n", dirFind);
            if(show)
            {
                centerLCD("Making Directory",0,0,1);
                centerLCD(dirFind,1,0,1);
                usleep(microseconds);
            }
            
            // END - LCD/Console Feedback ------------>
            
            if (-1 == dir_err)                                                  // if there was a problem creating the directory
            {
                printf("[i] Error creating directory!\n");                      // print status to console
                _errorLevel = 1;                                                // throw error
            }
            return 1;                                                           // directory exists
        }
        else
        {
            printf(" exists.\n",dirPath);                                       // print status to console
            return 1;                                                           // directory exists
        }
        return 0;                                                               // directory to search in does not exist
    }
    else
    {
        printf(" not found.\n",dirPath);                                        // print status to console
        return 0;                                                               // directory to search in does not exist
    }
    return 0;                                                                   // directory to search in does not exist
}

// function for copying files from one location to another using ifstream/ofstream
void stratHacker::copyFiles(bool show,const char* src, const char* dst, const char* fileName){
    std::string fullSrc = src + std::string("/") + fileName;                    // full path of source file
    std::string fullDst = dst + std::string("/") + fileName;                    // full path of destination file
    std::ifstream  srcName(fullSrc.c_str());                                    // create ifstream - input file
    if(!srcName)                                                                // if source file does not exist
    {
        
        centerLCD("Error Opening",0,0,1);
        centerLCD(fileName,1,0,1);
        usleep(microseconds);
        centerLCD("Exiting",0,0,1);
        centerLCD("Program",1,0,1);
        printf ("ERROR OPENING: %s\n", fullSrc.c_str());
        usleep(microseconds);
        // END - LCD/Console Feedback ------------>
        
        _errorLevel = 1; //exit (EXIT_FAILURE);                                 // throw error
    }
    std::ofstream  dstName(fullDst.c_str());                                    // create ofstream - output file
    dstName << srcName.rdbuf();                                                 // copy source to destination
    
    // START - LCD/Console Feedback ------------>
    printf("[+] Copying... %s\n",fullSrc.c_str());
    printf("[+] To... %s\n", fullDst.c_str());
    if(show){
        centerLCD("Copying File",0,0,1);
        centerLCD(fileName,1,0,1);
        usleep(500000);
    }
    
    // END - LCD/Console Feedback ------------>
    
}

// function for running shell commands with error monitoring
int stratHacker::runProgram(bool show, const char* path, std::string processMessage1, std::string processMessage2, int errorLev)
{
    int programSuccess;
    if (system( NULL )){                                                        // if we are able to run the system command
        
        // START - LCD/Console Feedback ------------>
        if(show)
        {
            centerLCD(processMessage1.c_str(),0,0,1);
            centerLCD(" ",1,0,1);
            printf("%s",processMessage2.c_str());
        }
        // END - LCD/Console Feedback ------------>
        printf("[i] Running... %s\n", path);
        programSuccess = system(path);                                          // run the shell command
        if(programSuccess != 0)                                                 // if the command does not run
        {
            // START - LCD/Console Feedback ------------>
            printf(" failed\n",programSuccess);
            if(show)
            {
                centerLCD("Failed!",1,0,1);
                usleep(3000000);
            }
            // END - LCD/Console Feedback ------------>
            
            _errorLevel = errorLev;                                             // throw an error
        }
        else
        {
            // START - LCD/Console Feedback ------------>
            printf(" complete\n",programSuccess);
            if(show)
            {
                centerLCD("Complete!",1,0,1);
                usleep(1000000);
            }
            // END - LCD/Console Feedback ------------>
        }
        
    }
    else                                                                        // system shell is not available
    {
        // START - LCD/Console Feedback ------------>
        printf("Shell Not Available - Exiting\n");
        // END - LCD/Console Feedback ------------>
        
        //EXIT_FAILURE;
        _errorLevel = errorLev;                                                 // throw an error
    }
    
    if (-1 == programSuccess)
    {
        perror("system");
        _errorLevel = errorLev; //exit(1);                                      // throw an error
    }
    
    if (127 == WEXITSTATUS(programSuccess))
    {
        fprintf(stderr, "%s", "Command Failed\n");
        _errorLevel = errorLev; //exit(2);                                      // throw an error
    }
    
    return(WEXITSTATUS(programSuccess));                                        // return fail/success of program
}

// function to see if a file exists
inline bool stratHacker::checkFileExists(const std::string& name){
    printf("[i] Looking for %s... ",name.c_str());
    if( access( name.c_str(), F_OK ) != -1 ) {                                  // if we can access the file
        printf("found\n");
        return 1;                                                               // return file found
    } else {
        printf("not found\n");
        return 0;                                                               // return file not found
    }
}

// function for setting time stamp - used for tagging files/folders
void stratHacker::timeStamp(){
    
    time_t theTime = time(NULL);                                                // create a time object
    struct tm *aTime = localtime(&theTime);
    int Day    = aTime->tm_mday;
    int Month  = aTime->tm_mon + 1;
    int Year   = aTime->tm_year + 1900;
    int Hour   = aTime->tm_hour;
    int Min    = aTime->tm_min;
    int Sec    = aTime->tm_sec;
    Year = Year % 100;                                                          // convert year to last 2 digits
    char dStamp[15];                                                            // array to hold our new timesStamp
    sprintf(dStamp, "%02d%02d%02d%02d%02d",Year, Month, Day,  Hour, Min);       // create a time/date stamp for the backup folder
    _timeStamp = dStamp;                                                        // store timeStamp to object
}

// function to find oldest folder - used for finding the oldest chip backup for restoring
bool stratHacker::getOldestFolder(const std::string& filePath)
{
    std::string noFolder = "0";
    DIR *dir = opendir(filePath.c_str());                           // dir object for folder we are looking in
    if(dir)                                                         // if directory exists
    {
        long int oldestFolderInt = 2139999999;                      // initial number to compare to - should be MUCH higher
        long int currentFolderInt;                                  // long int for current folder so we can compare to above
        std::string oldestFolderStr;                                // string for oldest folder - this will be returned
        std::string oldestFolder, dot, dotDot, canLog;              // for ignoring sub directory . and ..
        dot = ".";                                                  // will be ignored
        dotDot = "..";                                              // will be ignored
        canLog = "cannisterSerial.Log";                             // will be ignored
        
        // START - LCD/Console Feedback ------------>
        printf("[i] Searching for oldest backup in... %s\n",filePath.c_str());
        // END - LCD/Console Feedback ------------>
        
        struct dirent *ent;                                         // create directory structure
        while((ent = readdir(dir)) != NULL)                         // while there are folders to read
        {
            std::string folderName(ent->d_name);                    // create string containing folder name
            if(folderName != dot && folderName != dotDot && folderName != canLog)           // ignore directories . and ..
            {
                currentFolderInt = atol(folderName.c_str());        // convert folder string to long int for comparison
                if(currentFolderInt <= oldestFolderInt)             // if this folder is older than last
                {
                    oldestFolderInt = currentFolderInt;             // set as the oldest folder
                    oldestFolderStr = folderName;                   // save oldest folder string for future saving
                }
            }
            
            
            
            
        }
        // START - LCD/Console Feedback ------------>
        printf("[i] Found oldest backup... %s\n",oldestFolderStr.c_str());
        // END - LCD/Console Feedback ------------>
        
        _oldestBackup = oldestFolderStr;                         // save our oldest found folder to object
        return 1;                                               // return that we found a folder
    }
    return 0;                                                        // return 0 for no folder found
}


// error handing function - if anything fails this function is ran
int stratHacker::errorCheck(bool show, std::string errorMessage)
{
    if (_errorLevel > 0)                                                                    // if we receive any error
    {
        bool errorLog = checkExists(_errorChipLogPath.c_str(),_chipID.c_str(),"a+",1);      // check error log for the chip id
        checkExists(_tempChipLogPath.c_str(),_chipID.c_str(),"a+",1);               // see if chip exists in our tempChip.log - add if it isn't
        
        if(errorLog && show)                                                                        // if chip is already in the log
        {
            printf("[i] This Chip is already in the error Log...\n");
        }
        else                                                                                // if chip is not in the log
        {
            printf("[i] This Chip was added to the error Log...\n");
        }
        
    }
    
    if(_errorLevel == 99)                                                                   // if we receive a 99 error - 99 is our RESTORE chip error
    {
        // START - LCD/Console Feedback ------------>
        centerLCD("Attempting to",0,0,1);
        centerLCD("Restore Chip",1,0,1);
        printf("[+] Attempting to restore chip to original backup... \n");
        usleep(microseconds*2);
        restoreBackup(1);                                                                    // run the chip restore function
        // END - LCD/Console Feedback ------------>
    }
    
    
    if(_errorLevel == 1 && show)                                                                    // if we receive a 1 error - 1 is a simple fail error
    {
        // START - LCD/Console Feedback ------------>
        centerLCD(errorMessage,0,0,1);
        centerLCD("Error Received",1,0,1);
        usleep(microseconds*3);
        centerLCD("Hack Cancelled!",0,0,1);
        centerLCD("INSERT NEW CHIP",1,0,1);
        printf("[i] There was a problem hacking this chip... \n");
        printf("[i] Please insert a new chip... \n");
        usleep(microseconds*2);
        // END - LCD/Console Feedback ------------>
        
        //_errorLevel = 99;
    }
    
    
    return _errorLevel;                                                                     // return current error level
}

// resets our error level to 0 - 0 is NO ERROR
void stratHacker::errorCheckReset(){
    _errorLevel = 0;
}


// restore function attempts to find the oldest existing backup and revert our chip to that
void stratHacker::restoreBackup(bool show){
    
    
    std::string test = _savedEE_FullPath + std::string("/") + _chipID;
    printf("%s\n",test.c_str());
    getOldestFolder(test.c_str());
    _oldestBackupFolder = _savedEE_FullPath + std::string("/") + _chipID + std::string("/") + _oldestBackup;
    printf("%s\n",_backupFolder.c_str());// find our oldest chip backup
    char programPath2[190];                                                                                                             // array for shell comand
    sprintf(programPath2,"sudo cp -f %s/eeprom %s/eeprom",_oldestBackupFolder.c_str(),_fullChipPath.c_str());                           // build file copy shell command
    runProgram(show,programPath2, "Restore Backup","[+] Restoring chip from backup... ",1);                                                  // run shell command
}

void stratHacker::emptyFile(const char* ffp){                   // create an empty file
    const char * fl = ffp;                                      // convert file location string to Const Char
    FILE * tcLog;                       // open the log file
    if(tcLog = fopen(ffp,"w"))
    {
        printf("[+] Creating blank tempChip.Log... done\n");
        //fputs("\n",tcLog);                                      // add line break after chip
        fclose(tcLog);
    }
}


// function to open a log and see if a chip exists
bool stratHacker::checkExists(const char* ffp, const char* checkFor, std::string rw, bool cl){
    
    const char * fl = ffp;                                      // convert file location string to Const Char
    FILE * tcLog;                       // open the log file
    if(tcLog = fopen(ffp,rw.c_str()))
    {
        char lineData [16];                                         // array for data being read
        while(fgets (lineData , 16 , tcLog) != NULL)                // read log file line by line
        {
            std::string chipLog(lineData);                          // convert char array to a string for comparing
            std::string lookingFor(checkFor);
            if (lookingFor.compare(chipLog) == 0)                   // compare incoming chip string to one being read in log
            {
                fclose(tcLog);
                return 1;                                           // if chip exists return true
            }
        }
        
        // if chip does not exist in log
        
        if(cl)                                                      // if cl flag is true - write chip to log
        {
            // START - LCD/Console Feedback ------------>
            printf("[i] Creating %s... ", ffp);
            // END - LCD/Console Feedback ------------>
            
            const char * ch = checkFor;                             // convert chip data to const char
            fputs(ch,tcLog);                                        // add chip data to log
            fputs("\n",tcLog);                                      // add line break after chip
            // close file
            
            // START - LCD/Console Feedback ------------>
            printf("done\n");
            // END - LCD/Console Feedback ------------>
        }
        fclose(tcLog);
    }
    return 0;                                                   // return false - chip didin't exist - added it
}



// tool for streamlining displaying text on LCD either centered or left justified - STRING
void stratHacker::centerLCD(std::string text, int r, int c, bool cen){
    
    if(text.length() > 16)
    {
        text.erase(16,text.length());
    }
    
    if(cen)                                         // if center flag is true
    {
        c = (16 - text.length())/2;                 // calculate the column location to center the text on LCD
    }
    for(i = 0; i < c; i++)                          // put blank spaces in areas before text
    {
        lcdPosition(lcd2,i,r);                      // position cursor
        lcdPuts(lcd2," ");                          // put a blank on lcd
    }
    for(i = i+text.length(); i < 16; i++)           // put blank spaces in areas after text
    {
        lcdPosition(lcd2,i,r);                      // position cursor
        lcdPuts(lcd2," ");                          // put a blank on lcd
    }
    
    lcdPosition(lcd2,c,r);                          // position cursor
    lcdPrintf(lcd2,"%s",text.c_str());              // print incoming text
    
}


// tool for streamlining displaying text on LCD either centered or left justified - INT
void stratHacker::centerLCD(int text, int r, int c, bool cen){
    
    std::string validate = std::to_string(text);
    
    if(cen)                                         // if center flag is true
    {
        c = (16 - validate.length())/2;             // calculate the column location to center the text on LCD
        
    }
    for(i = 0; i < c; i++)                          // put blank spaces in areas before text
    {
        lcdPosition(lcd2,i,r);                      // position cursor
        lcdPuts(lcd2," ");                          // put a blank on lcd
    }
    for(i = i+validate.length(); i < 16; i++)       // put blank spaces in areas after text
    {
        lcdPosition(lcd2,i,r);                      // position cursor
        lcdPuts(lcd2," ");                          // put a blank on lcd
    }
    
    lcdPosition(lcd2,c,r);                          // position cursor
    lcdPrintf(lcd2,"%s",validate.c_str());          // print incoming text
}

// tool for streamlining displaying text on LCD either centered or left justified - INT
void stratHacker::centerLCD(unsigned int text, int r, int c, bool cen){
    
    std::string validate = std::to_string(text);
    
    if(cen)                                         // if center flag is true
    {
        c = (16 - validate.length())/2;             // calculate the column location to center the text on LCD
    }
    for(i = 0; i < c; i++)                          // put blank spaces in areas before text
    {
        lcdPosition(lcd2,i,r);                      // position cursor
        lcdPuts(lcd2," ");                          // put a blank on lcd
    }
    for(i = i+validate.length(); i < 16; i++)       // put blank spaces in areas after text
    {
        lcdPosition(lcd2,i,r);                      // position cursor
        lcdPuts(lcd2," ");                          // put a blank on lcd
    }
    
    lcdPosition(lcd2,c,r);                          // position cursor
    lcdPrintf(lcd2,"%s",validate.c_str());          // print incoming text
}

// tool for streamlining displaying text on LCD either centered or left justified - DOUBLE
void stratHacker::centerLCD(double text, int r, int c, bool cen){
    
    std::string validate = std::to_string(text);    // convert to string for length calculation
    int textLength = validate.length() - 5;         // ignore double 5 trailing zeros
    
    if(cen)                                         // if center flag is true
    {
        c = (16 - textLength)/2;                    // calculate the column location to center the text on LCD
    }
    for(i = 0; i < c; i++)                          // put blank spaces in areas before text
    {
        lcdPosition(lcd2,i,r);                      // position cursor
        lcdPuts(lcd2," ");                          // put a blank on lcd
    }
    for(i = i + textLength; i < 16; i++)            // put blank spaces in areas after text
    {
        lcdPosition(lcd2,i,r);                      // position cursor
        lcdPuts(lcd2," ");                          // put a blank on lcd
    }
    
    lcdPosition(lcd2,c,r);                          // position cursor
    lcdPrintf(lcd2,"%.1f",text);                    // print incoming text
}

// function to strip a string from another string
std::string stratHacker::stripString(std::string const& j, std::string const& m){       // string find/remove function
    std::string s = j;                                                                  // make a copy of the full string
    std::string k = m;                                                                  // make a copy of string to be removed
    std::size_t found = s.find(k);                                                      // find total full string length
    s.erase (found,k.length());                                                         // remove string from full string
    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());                            // remove all line breaks from string
    return s;                                                                           // return modified string
}


// read text file and return text on line
std::string stratHacker::getLineFromFile(std::string& filePath, unsigned int& lineNum){
    int i;                                                                          // for loop
    int line = 0;                                                                   // for counting which line we are on
    const char * fl = filePath.c_str();
    FILE *openFile;                                                                 // open file declaration
    if (openFile = fopen(fl,"r")){                                                  // open the log file
        char lineData [16];                                                         // array for data being read
        while(fgets (lineData , 16 , openFile) != NULL)                             // read log file line by line
        {
            std::string fileData(lineData);                                         // store incoming data
            line ++;
            if(lineNum == line)
            {
                fclose(openFile);                                                   // close the file
                fileData.erase(std::remove(fileData.begin(), fileData.end(), '\n'), fileData.end());        // remove line breaks
                _lastTextLine = fileData;                                           // store last line of text
                return fileData;                                                    // return text
            }
        }
        fclose(openFile);
    }
    
    return std::string();
    
}

// save setting into config file
bool stratHacker::saveConfig(std::string& filePath, std::string& setting){
    const char * fl = filePath.c_str();                                             // file path
    FILE * configFile;                                                              // create file reference
    if (configFile = fopen(fl,"w+")){                                               // if we can create the file
        fwrite(setting.c_str() , 1 , setting.length() , configFile );               // write setting to file
        fclose(configFile);                                                         // close the file
        return 1;
    }
    
    return 0;
    
}

// get config data from file
bool stratHacker::getstoredMaterial(){
    int i;                                                                          // for loop
    int line = 0;                                                                   // for counting which line we are on
    const char * fl = "/home/pi/stratHacker/materialPref.txt";
    FILE *openFile;                                                                 // open file declaration
    if (openFile = fopen(fl,"r")){                                                  // open the log file
        char lineData [16];                                                         // array for data being read
        while(fgets (lineData , 16 , openFile) != NULL)                             // read log file line by line
        {
            std::string fileData(lineData);                                         // store incoming data
            fileData.erase(std::remove(fileData.begin(), fileData.end(), '\n'), fileData.end());        // remove line breaks
            unsigned int matLine = atoi(fileData.c_str());
            _materialPref = matLine;                                                // store material type
            std::string flp = "/home/pi/stratHacker/materialTypes.txt";             // list of materials
            std::string matName = getLineFromFile(flp, matLine);                    // get specific line from file
            _storedMaterial = matName;                                              // store saved material name type
            
            fclose(openFile);
            return 1;
        }
        fclose(openFile);
    }
    return 0;
}













// get config data from file
bool stratHacker::getstoredVolume(){
    const char * fl = "/home/pi/stratHacker/volumePref.txt";
    FILE *openFile;                                                                 // open file declaration
    if (openFile = fopen(fl,"r")){                                                  // open the log file
        char lineData [16];                                                         // array for data being read
        while(fgets (lineData , 16 , openFile) != NULL)                             // read log file line by line
        {
            std::string fileData(lineData);                                         // store incoming data
            fileData.erase(std::remove(fileData.begin(), fileData.end(), '\n'), fileData.end());        // remove line breaks
            _storedVolume = fileData;                                               // save incoming data to object
            fclose(openFile);                                                       // close the file
            return 1;
        }
        fclose(openFile);
    }
    _storedVolume = "56.3";                                                         // if there is not any data - set default 100%
    return 0;
}



double stratHacker::storedVolume(){
    return atof(_storedVolume.c_str());                                                    // return stored volume as a double
}

unsigned int stratHacker::getMaterialPref(){
    return _materialPref;
}

unsigned int stratHacker::getMachinePref(){
    return _machinePref;
}

std::string stratHacker::getTempPath(){
    return _tempChipLogPath;
}
std::string stratHacker::getErrorPath(){
    return _errorChipLogPath;
}

std::string stratHacker::getSuccessPath(){
    return _cannisterLogPath;
}

std::string stratHacker::getCannister(){
    std::string s = std::to_string(_canSerialToWrite);                  // convert cannister serial to string
    return s;
}


unsigned int stratHacker::logCount(std::string& logFile){                                     // counts entry in log files
    
    
    unsigned int line = 0;
    int i;                                                                          // for loop
    //int line = 0;                                                                 // for counting which line we are on
    const char * fl = logFile.c_str();
    FILE *openFile;                                                                 // open file declaration
    if (openFile = fopen(fl,"r")){                                                  // open the log file
        char lineData [50];                                                         // array for data being read
        while(fgets (lineData , 50 , openFile) != NULL)                             // read log file line by line
        {
            line ++;
        }
        fclose(openFile);
    }
    return line;
    
    
}



unsigned int stratHacker::newChips(){
    std::string chipId = "23-";                             // chip folder prefix
    unsigned int chipCnt = 0;                               // number of connected chips not in the log
    
    // START check for 2nd NEW Chip--------------------------------->
    
    DIR *wTT = opendir(_w1Path.c_str());                     // location where stratasys chip shows up
    if(wTT)
    {
        struct dirent* wttEnt;                                 // create objecct
        while((wttEnt = readdir(wTT)) != NULL)                 // while directories exist in 1-wire folder
        {
            std::string wChipData(wttEnt->d_name);              // store directory names as chip id
            if(wChipData[0] == chipId[0] && wChipData[1] == chipId[1] && wChipData[2] == chipId[2])   // if directory matches first 3 letters of chip prefix
            {
                bool chipExistsFound  = checkExists(_tempChipLogPath.c_str(),wChipData.c_str(),"r",0);               // see if chip exists in our tempChip.log - add if it isn't
                if (!chipExistsFound){                          // if the is not in our temp log
                    chipCnt++;                             // increase our number of NEW chips found
                }
            }
        }
    }
    // END check for 2nd NEW Chip--------------------------------->
    closedir(wTT);
    
    _newChipCount = chipCnt;
    return chipCnt;
}




bool button::read(){                                                    // read the button status and return on/off (
    _reading = digitalRead(_buttonPin);                                 // read the button / 0 on - 1 off (GPIO pulled high)
    unsigned long long bMillis = std::chrono::duration_cast<std::chrono::milliseconds>
    (std::chrono::system_clock::now().time_since_epoch()).count();      // set millis since epoch to bmillis;
    if (_reading != _lastButtonState) {                                 // if button read is different than the last button state
        _lastDebounceTime = bMillis;                                    // save millis() to last debounce time
    }
    if ((bMillis - _lastDebounceTime) > _debounceDelay) {               // if we are beyond the debounce delay time
        if (_reading != _buttonState) {                                 // if the button state has changed
            _buttonState = _reading;                                    // store current button state
            if (_buttonState == 1) {                                    // if button is pressed
                return 1;                                               // return true - button is pressed
            }
        }
    }
    return 0;                                                           // return false - button is not down
}

void stratHacker::progressBar(unsigned int micSec){
    //centerLCD("CHIP SEARCH",0,0,1);
    lcdPosition(lcd2,0,1);                      // position cursor
    lcdPuts(lcd2,"                ");                          // put a blank on lcd
    int i = 0;
    for(i =  0; i < 16; i++){
        lcdPosition(lcd2,i,1);                      // position cursor
        lcdPuts(lcd2,".");                        // put a blank on lcd
        
        usleep(micSec);
    }
    
}










