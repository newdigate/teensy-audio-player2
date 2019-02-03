//
// Created by Nicholas Newdigate on 02/02/2019.
//

#ifndef ARDUINO_MIDI_WRITER_TFTSDDIRECTORYBROWSER_H
#define ARDUINO_MIDI_WRITER_TFTSDDIRECTORYBROWSER_H

#include "directives.h"

#if ARDUINO >= 100
  #include <Adafruit_GFX.h>    // Core graphics library

  #if USE_LEGACY_SD_LIB
    #include <SD.h>
  #else  // USE_SD_H
    #include "SdFs.h"
  #endif  // USE_SD_H
  
#else
  #include "Adafruit/Adafruit_GFX.h"    // x86 emulated core graphics library
  #include "SD/SD.h"                    // x86 emulated sd card 
#endif

class TFTSdDirectoryBrowser {
public:
    inline TFTSdDirectoryBrowser(
                Adafruit_GFX *tft,
                #if USE_LEGACY_SD_LIB
                SDClass *sd,
                #else
                SdFat *sd,
                #endif
                int nMax,

                uint16_t cDirectoryForeground,
                uint16_t cDirectoryBackground,
                uint16_t cDirectorySelectedForeground,
                uint16_t cDirectorySelectedBackground,
                uint16_t cFileForeground,
                uint16_t cFileBackground,
                uint16_t cFileSelectedForeground,
                uint16_t cFileSelectedBackground

                ) :

            _tft(tft),
            _sd(sd),
            _nMaxFilesPerPage(nMax),
             _cDirectoryForeground(cDirectoryForeground),
             _cDirectoryBackground(cDirectoryBackground),
             _cDirectorySelectedForeground(cDirectorySelectedForeground),
             _cDirectorySelectedBackground(cDirectorySelectedBackground),
             _cFileForeground(cFileForeground),
             _cFileBackground(cFileBackground),
             _cFileSelectedForeground(cFileSelectedForeground),
             _cFileSelectedBackground(cFileSelectedBackground)
    {
        filenames = new char*[_nMaxFilesPerPage];
        for (int i=0; i<_nMaxFilesPerPage; i++) {
          filenames[i] = NULL;
        }
        dirIndex = new uint16_t[_nMaxFilesPerPage];
        isDir = new bool[_nMaxFilesPerPage];
    }

    inline ~TFTSdDirectoryBrowser(){
        if (filenames != NULL) {
          
          for (int i=0; i<_nMaxFilesPerPage;i++) 
            if (filenames[i] != NULL) {
              delete filenames[i];
              filenames[i] = NULL;
            }
            
          delete [] filenames;
          filenames = NULL;
        }
    }

    void initialize();
    void reload();
    void setSelectFileIndex(int index);
    void update();
    unsigned long getTotalFileCount();
    char * selectedFilename();
private:
    long _currentPage   = 0;
    long _selectedFileIndex     = -1;
    long _totalFiles    = 0;
    uint16_t _nMaxFilesPerPage;
    uint16_t _numFilesOnPage;
    bool _enabled       = false;

    uint16_t _cDirectoryForeground;
    uint16_t _cDirectoryBackground;
    uint16_t _cDirectorySelectedForeground;
    uint16_t _cDirectorySelectedBackground;
    uint16_t _cFileForeground;
    uint16_t _cFileBackground;
    uint16_t _cFileSelectedForeground;
    uint16_t _cFileSelectedBackground;

    File file;
    File dirFile;               
#if USE_LEGACY_SD_LIB
    SDClass *_sd;
#else
    SdFat *_sd;
#endif    
    Adafruit_GFX *_tft;
    char **filenames;
    bool *isDir;
    uint16_t *dirIndex;

    File open(char *s);
    bool open(char *filename, File &file, File &dirFile);
    bool isHidden(const File &file);
    bool openNext(File &file, File &dirFile);
    void rewind(File &file);
    void getFileName(File &file, char *buf);
    void openFileIndex(File &file, File &dir, int index);
    bool isSubDir(File &file);
    void toggleSelection(int i, bool selected);
};


#endif //ARDUINO_MIDI_WRITER_TFTSDDIRECTORYBROWSER_H
