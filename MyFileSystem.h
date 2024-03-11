#pragma once
#include <fstream>
#include <string.h>
#include <vector>
#include <iostream>

#define FS_PATH "E:\\MyFS.Dat"
#define BYTES_PER_SECTOR 512  //2 bytes
#define SECTORS_PER_CLUSTER 4  //1 byte
#define SECTORS_BEFORE_FAT 1 //1 byte
#define NUMBER_OF_FAT 2 //1 byte
#define FAT_SIZE 4081  //2 byte, each entry in FAT takes up 4 bytes
#define FAT_ENTRY_SIZE 4
#define FREE 0 //free cluster value in FAT
#define EOF 268435455  //EOF cluster value in FAT
#define VOLUME_SIZE 2097152 //4 bytes, size in sector
#define STARTING_CLUSTER 2

class MyFileSystem
{
private:
    std::fstream f;
    int bytesPerSector;
    int sectorsPerCluster;
    int sectorsBeforeFat;
    int numberOfFat;
    int fatSize;
    int fatEntrySize = FAT_ENTRY_SIZE;
    int volumeSize;

    void WriteBlock(unsigned int offset, std::string data);
    void WriteBlock(unsigned int offset, unsigned int data);
    std::vector<unsigned int> ReadFAT(unsigned int startingCluster);
    std::string ReadBlock(unsigned int offset, bool readCluster);

public:
    MyFileSystem();
    ~MyFileSystem();
    
    void ListFiles();
    void ImportFile(const std::string& inputPath);
    void ExportFile(const std::string& outputPath);
    void DeleteFile(bool restorable = true);
};