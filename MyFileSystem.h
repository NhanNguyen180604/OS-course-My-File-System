#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <windows.h>
#include <cmath>

#define FS_PATH "E:\\MyFS.Dat"
#define BYTES_PER_SECTOR 512  //2 bytes
#define SECTORS_PER_CLUSTER 4  //1 byte
#define SECTORS_BEFORE_FAT 1 //1 byte
#define FAT_SIZE 4081  //2 byte, each entry in FAT takes up 4 bytes, do math to get this number
#define FAT_ENTRY_SIZE 4 //size in bytes
#define FREE 0 //free cluster value in FAT
#define MY_EOF 268435455  //EOF cluster value in FAT
#define VOLUME_SIZE 2097152 //4 bytes, size in sector
#define STARTING_CLUSTER 2
#define NUMBER_OF_CLUSTERS 522247 //size in sector, do math to get this number
#define FINAL_CLUSTER 522248  //cluster starts at 2
#define FILE_EXTENSION_LENGTH 3

class MyFileSystem
{
private:
#pragma pack(push, 1)
    struct Entry
    {
        char name[41];
        char extension[4];
        unsigned char attributes;
        WORD creationTime;
        WORD creationDate;
        WORD lastAccessDate;
        WORD lastWriteTime;
        WORD lastWriteDate;
        unsigned int startingCluster;
        unsigned int fileSize;

        void SetName(const std::string& fileName, unsigned int number, bool adjusted = false);
        void SetExtension(const std::string& fileExtension);
        void SetDateAndTime(const WIN32_FILE_ATTRIBUTE_DATA &fileAttributes);
    };
#pragma pack(pop)

    std::fstream f;
    unsigned int bytesPerSector;
    unsigned int sectorsPerCluster;
    unsigned int sectorsBeforeFat;
    unsigned int fatSize;
    unsigned int fatEntrySize = FAT_ENTRY_SIZE;
    unsigned int volumeSize;

    void WriteBlock(unsigned int offset, std::string data, bool writeCluster, bool padding);
    void WriteBlock(unsigned int offset, unsigned int data, bool writeCluster, bool padding);
    std::vector<char> ReadBlock(unsigned int offset, unsigned int size);
    bool CheckDuplicateName(Entry *&entry, unsigned int& lastCluster, unsigned int &lastOffset);
    //read and return consecutive clusters of files
    std::vector<unsigned int> ReadFAT(unsigned int startingCluster);
    //find n free clusters
    std::vector<unsigned int> GetFreeClusters(unsigned int n);
    //write consecutive clusters to FAT
    void WriteClustersToFAT(const std::vector<unsigned int>& clusters);
    //write file's entry to RDET
    bool WriteFileEntry(Entry *&entry);
    //write file's content to cluster
    void WriteFileContent(std::ifstream& fin, const std::vector<unsigned int>& clusters);

public:
    MyFileSystem();
    ~MyFileSystem();
    
    void ListFiles();
    void ImportFile(const std::string& inputPath);
    void ExportFile(const std::string& outputPath);
    void MyDeleteFile(bool restorable = true);

    void test();
};