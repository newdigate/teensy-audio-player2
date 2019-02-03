//
// Created by Nicholas Newdigate on 02/02/2019.
//

#include "TFTSdDirectoryBrowser.h"

#include <Audio.h>

unsigned long TFTSdDirectoryBrowser::getTotalFileCount() {
  return _totalFiles;
}

void TFTSdDirectoryBrowser::initialize() {
   Serial.println("dirFile opening:");
    dirFile = open("/");
    Serial.println("dirFile opened:");
    while (openNext(file, dirFile)) {
        if (!isHidden(file))
            _totalFiles++;

        file.close();
    }
}

File TFTSdDirectoryBrowser::open(char *s){
#if USE_LEGACY_SD_LIB
    return SD.open(s, FILE_READ);
#else
    return _sd->open(s);
#endif
}

void TFTSdDirectoryBrowser::reload() {

    Serial.println("reload");
    dirFile.rewindDirectory();
    Serial.println("rewind complete");

    if (_currentPage > 0) {
        for (int i=0; i<_currentPage; i++){
            int nn = 0;
            Serial.println(nn, DEC);


            while (nn < _nMaxFilesPerPage && openNext(file, dirFile)) {
                if (!isHidden(file)){
                    nn++;
                }
                file.close();
                delay(1);
            }

        }
    }
    Serial.println("paging complete");
    int n = 0;
    int total = 0;
    while (n < _nMaxFilesPerPage && openNext(file, dirFile)) {
        // Skip hidden files.
        if (!isHidden(file)){
            isDir[n] = isSubDir(file);
            if (filenames[n] == NULL)
               filenames[n] = new char[100];
            getFileName(file, filenames[n]);

            // Save dirIndex of file in directory.
            dirIndex[n] = total + (_currentPage * _nMaxFilesPerPage);
            n++;
        }
        total++;
        if (file) file.close();
    }
    _numFilesOnPage = n;
    Serial.println("reload complete");
}

void TFTSdDirectoryBrowser::update() {
    _tft->fillScreen(_cFileBackground);
    for (int i=0; i<_numFilesOnPage; i++) {
        if (i + _currentPage*_nMaxFilesPerPage == _selectedFileIndex) {
            //selected index
            toggleSelection(i, true);
        } else {
            toggleSelection(i, false);
        }
    }
}

bool TFTSdDirectoryBrowser::isHidden(const File &file) {
#if USE_LEGACY_SD_LIB
    return false;
#else
    return file.isHidden();
#endif
}

void TFTSdDirectoryBrowser::rewind(File &file) {
#if USE_LEGACY_SD_LIB
    dirFile.rewindDirectory();
#else
    dirFile.rewind();
#endif
}

void TFTSdDirectoryBrowser::getFileName(File &file, char *buf) {
#if USE_LEGACY_SD_LIB
    strcpy(buf, file.name());
#else
    char *filename = new char[128] ;
    file.getName(filename, 128);
    strcpy(buf, filename);
    delete [] filename;
#endif
}

void TFTSdDirectoryBrowser::openFileIndex(File &file, File &dir, int index) {

#if USE_LEGACY_SD_LIB
    AudioNoInterrupts();
    file = SD.open(filenames[index], FILE_READ);
    AudioInterrupts();
#else
    file.open(&dir, dirIndex[index], O_RDONLY);
#endif

}

bool TFTSdDirectoryBrowser::isSubDir(File &file) {
#if USE_LEGACY_SD_LIB
    return file.isDirectory();
#else
    return file.isDirectory();
#endif
}

void TFTSdDirectoryBrowser::toggleSelection(int i, bool selected) {
    _tft->setCursor(1, i * 8 +1 );
    if (!isDir[i]) {
        _tft->setTextColor(
                selected? _cFileSelectedForeground : _cFileForeground,
                selected? _cFileSelectedBackground : _cFileBackground
        );
    } else {
        _tft->setTextColor(
                selected? _cDirectorySelectedForeground : _cDirectoryForeground,
                selected? _cDirectorySelectedBackground : _cDirectoryBackground
        );
    }
    _tft->print(filenames[i]);
}

void TFTSdDirectoryBrowser::setSelectFileIndex(int index) {
    if (_selectedFileIndex != index) {
        long newPage = index / _nMaxFilesPerPage;
        if (_currentPage != newPage) {
            _currentPage = newPage;
            _selectedFileIndex = index;
            reload();
            update();
        } else {
            toggleSelection(_selectedFileIndex % _nMaxFilesPerPage, false);
            _selectedFileIndex = index;
            toggleSelection(_selectedFileIndex % _nMaxFilesPerPage, true);
        }
    }
}


bool TFTSdDirectoryBrowser::openNext(File &file, File &dirFile) {
    
#if USE_LEGACY_SD_LIB
        AudioNoInterrupts();
        file = dirFile.openNextFile(FILE_READ);
        AudioInterrupts();
        return file;
#else
        return file.openNext(&dirFile, O_RDONLY);
#endif

}

bool TFTSdDirectoryBrowser::open(char *filename, File &file, File &dirFile) {
    
#if USE_LEGACY_SD_LIB
        AudioNoInterrupts();
        file = SD.open(filename, FILE_READ);
        AudioInterrupts();
        return file;
#else
        return file.open(&dirFile, filename, O_RDONLY);
#endif
}

char * TFTSdDirectoryBrowser::selectedFilename() {
  return filenames[_selectedFileIndex % _nMaxFilesPerPage];
}
