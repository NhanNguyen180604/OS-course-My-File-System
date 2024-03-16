#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <windows.h>
#include <streambuf>

#include "cryptlib.h"
#include "pwdbased.h"
#include "sha.h"
#include "hex.h"
#include "chachapoly.h"
#include "filters.h"
#include "files.h"

#define FS_PATH "E:\\MyFS.Dat"
#define BYTES_PER_SECTOR 512  //2 bytes
#define SECTORS_PER_CLUSTER 4  //1 byte
#define SECTORS_BEFORE_FAT 1 //1 byte
#define FAT_SIZE 4089  //2 byte, each entry in FAT takes up 4 bytes, do math to get this number
#define FAT_ENTRY_SIZE 4 //size in bytes
#define FREE 0 //free cluster value in FAT
#define MY_EOF 268435455  //EOF cluster value in FAT
#define VOLUME_SIZE 2097152 //4 bytes, size in sector
#define STARTING_CLUSTER 2
#define NUMBER_OF_CLUSTERS 523265 //size in sector, do math to get this number
#define FINAL_CLUSTER 523266  //cluster starts at 2
#define ENTRY_NAME_SIZE 40
#define FILE_EXTENSION_LENGTH 3

class MyFileSystem
{
private:
#pragma pack(push, 1)
    struct Entry
    {
        //max length name is 38, may be shrunk to avoid duplicate
        char name[40];
        char extension[3];
        unsigned char attributes;
        WORD creationTime;
        WORD creationDate;
        WORD lastAccessDate;
        WORD lastWriteTime;
        WORD lastWriteDate;
        unsigned int startingCluster;
        unsigned int fileSize;
        //reserved for storing first character of file name when deleted
        char reserved[2] = {0};
        char padding[16] = {0};
        char hashedPassword[32] = {0};
        byte mac[16] = {0};

        void SetName(const std::string& fileName, unsigned int number, bool adjusted = false);
        std::string GetName();
        void SetExtension(const std::string& fileExtension);
        void SetDateAndTime(const WIN32_FILE_ATTRIBUTE_DATA &fileAttributes);
        void SetHash(const std::string& hash);
    };
#pragma pack(pop)

    std::fstream f;
    unsigned int bytesPerSector;
    unsigned int sectorsPerCluster;
    unsigned int sectorsBeforeFat;
    unsigned int fatSize;
    unsigned int fatEntrySize = FAT_ENTRY_SIZE;
    unsigned int volumeSize;
    bool hasPassword;

    void CreateFSPassword();
    bool CheckFSPassword(const std::string& password);

    void WriteBlock(unsigned int offset, std::string data, bool writeCluster, bool padding);
    void WriteBlock(unsigned int offset, unsigned int data, bool writeCluster, bool padding);
    std::vector<char> ReadBlock(unsigned int offset, unsigned int size);
    bool CheckDuplicateName(Entry *&entry);
    //read and return consecutive clusters of files
    std::vector<unsigned int> GetClustersChain(unsigned int startingCluster);
    //find n free clusters
    std::vector<unsigned int> GetFreeClusters(unsigned int n);
    //write consecutive clusters to FAT
    void WriteClustersToFAT(const std::vector<unsigned int>& clusters);
    //write file's entry to RDET
    bool WriteFileEntry(Entry *&entry);
    //write file's content to cluster
    void WriteFileContent(const std::string& data, const std::vector<unsigned int>& clusters);
    //read file's content from cluster
    std::string ReadFileContent(unsigned int fileSize, const std::vector<unsigned int>& clusters);

    //generate hash using PKCS5_PBKDF2_HMAC with SHA256
    //https://www.cryptopp.com/wiki/PKCS5_PBKDF2_HMAC
    std::string GenerateHash(const std::string& data);
    //encrypt using 
    void EncryptData(std::string& data, const std::string& key, Entry *&entry);
    void DecryptData(std::string& data, const std::string& key, Entry *&entry);

    //get list of file
    std::vector<Entry*> GetFileList();

    void ImportFile(const std::string& inputPath, bool setPassword = false);
    void ExportFile(const std::string& outputPath, Entry *&entry);

public:
    MyFileSystem();
    ~MyFileSystem();
    
    bool CheckFSPassword();
    void ImportFile();
    void ExportFile();
    void ListFiles();
    void MyDeleteFile(bool restorable = true);


    void test();
};