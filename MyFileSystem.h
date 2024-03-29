#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <utility>
#include <iomanip>

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
#define ENTRY_NAME_SIZE 48
#define FILE_EXTENSION_LENGTH 4

typedef unsigned char byte;

class MyFileSystem
{
private:
#pragma pack(push, 1)
    struct Entry
    {
        //name may be shrunk to avoid duplicate
        char name[ENTRY_NAME_SIZE];
        char extension[FILE_EXTENSION_LENGTH];
        //reserved for storing first character of file name when deleted
        char reserved[4] = {0};
        int nameLen;
        unsigned int startingCluster;
        unsigned int fileSize;
        bool hasPassword = 0;
        char padding[11] = {0};
        char hashedPassword[32] = {0};
        byte mac[16] = {0};

        void SetName(const std::string& fileName, unsigned int number, bool adjusted = false);
        std::string GetFullName() const;
        void SetExtension(const std::string& fileExtension);
        std::string GetIndex() const;
        void SetHash(const std::string& hash);
        std::string GetInfo() const;
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
    void ChangeFSPassword();

    std::vector<char> ReadBlock(unsigned int offset, unsigned int size);
    bool CheckDuplicateName(Entry *&entry);
    std::vector<unsigned int> GetClustersChain(unsigned int startingCluster);
    std::vector<unsigned int> GetFreeClusters(unsigned int n);
    //write consecutive clusters to FAT
    void WriteClustersToFAT(const std::vector<unsigned int>& clusters);
    bool WriteFileEntry(Entry *&entry);
    void WriteFileContent(const std::string& data, const std::vector<unsigned int>& clusters);
    std::string ReadFileContent(unsigned int fileSize, const std::vector<unsigned int>& clusters);

    //generate hash using PKCS5_PBKDF2_HMAC with SHA256
    //https://www.cryptopp.com/wiki/PKCS5_PBKDF2_HMAC
    std::string GenerateHash(const std::string& data);
    //encrypt using XChaCha20Poly1305
    void EncryptData(std::string& data, const std::string& key, Entry *&entry);
    void DecryptData(std::string& data, const std::string& key, Entry *&entry);

    //get list of file, pair of offset and entry
    std::vector<std::pair<std::string, unsigned int>> GetFileList(bool getDeleted = false);
    void PrintFileList(const std::vector<std::pair<std::string, unsigned int>>& fileList);

    void ImportFile(const std::string& inputPath, bool setPassword = false);
    bool CheckFilePassword(Entry *&entry, std::string& filePassword);
    void ChangeFilePassword(Entry *&entry, unsigned int offset, bool removed = false);
    void ExportFile(const std::string& outputPath, Entry *&entry);
    void RestoreFile(unsigned int bytesOffset);
    void MyDeleteFile(unsigned int bytesOffset, bool restorable = true);

public:
    MyFileSystem();
    ~MyFileSystem();
    
    bool CheckFSPassword();
    void ChangeFilePassword();
    void ImportFile();
    void ExportFile();
    void ListFiles();
    void MyDeleteFile();
    void MyRestoreFile();

    void HandleInput();
};