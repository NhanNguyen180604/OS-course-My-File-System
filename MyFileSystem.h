#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <windows.h>

#define FS_PATH "E:\\MyFS.Dat"
#define BYTES_PER_SECTOR 512  //2 bytes
#define SECTORS_PER_CLUSTER 4  //1 byte
#define SECTORS_BEFORE_FAT 1 //1 byte
#define NUMBER_OF_FAT 2 //1 byte
#define FAT_SIZE 4081  //2 byte, each entry in FAT takes up 4 bytes, do math to get this number
#define FAT_ENTRY_SIZE 4
#define FREE 0 //free cluster value in FAT
#define MY_EOF 268435455  //EOF cluster value in FAT
#define VOLUME_SIZE 2097152 //4 bytes, size in sector
#define STARTING_CLUSTER 2
#define NUMBER_OF_CLUSTERS 522247 //size in sector, do math to get this number

class MyFileSystem
{
private:
#pragma pack(push, 1)
    struct MainEntry
    {
        char name[9];
        char extension[4];
        unsigned char attributes;
        WORD creationTime;
        WORD creationDate;
        WORD lastAccessDate;
        WORD lastWriteTime;
        WORD lastWriteDate;
        unsigned int startingCluster;
        unsigned int fileSize;

        void CreateShortName(const std::string& fileName, unsigned int number);
        void SetExtension(const std::string& fileExtension);
        void SetDateAndTime(const WIN32_FILE_ATTRIBUTE_DATA &fileAttributes);
    };
#pragma pack(pop)

    std::fstream f;
    int bytesPerSector;
    int sectorsPerCluster;
    int sectorsBeforeFat;
    int numberOfFat;
    int fatSize;
    int fatEntrySize = FAT_ENTRY_SIZE;
    int volumeSize;

    void WriteBlock(unsigned int offset, std::string data, bool writeCluster, bool padding);
    void WriteBlock(unsigned int offset, unsigned int data, bool writeCluster, bool padding);
    std::vector<unsigned int> ReadFAT(unsigned int startingCluster);
    std::vector<char> ReadBlock(unsigned int offset, unsigned int size);
    bool CheckDuplicateShortName(const std::string& name);

public:
    MyFileSystem();
    ~MyFileSystem();
    
    void ListFiles();
    void ImportFile(const std::string& inputPath);
    void ExportFile(const std::string& outputPath);
    void MyDeleteFile(bool restorable = true);

    void test();
};