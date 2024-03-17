#include "MyFileSystem.h"

std::vector<std::pair<std::string, unsigned int>> MyFileSystem::GetFileList() {
    std::vector<std::pair<std::string, unsigned int>> fileList;
    std::vector<unsigned int> rdetClusters = GetClustersChain(STARTING_CLUSTER);
    for (unsigned int cluster : rdetClusters)
    {
        unsigned int sectorOffset = sectorsBeforeFat + fatSize + sectorsPerCluster * (cluster - STARTING_CLUSTER);
        unsigned int bytesOffset = sectorOffset * bytesPerSector;
        unsigned int limitOffset = bytesOffset + bytesPerSector * sectorsPerCluster;  //start of next cluster
        for (; bytesOffset < limitOffset; bytesOffset += sizeof(Entry))
        {
            Entry tempEntry;
            f.seekg(bytesOffset, f.beg);
            f.read((char*)&tempEntry, sizeof(Entry));
            //sign of erased file
            if (tempEntry.name[0] == -27)
            {
                continue;
            }
            //sign of empty entry
            if (tempEntry.name[0] == 0)
            {
                return fileList;
            }
            fileList.push_back(std::make_pair(tempEntry.GetInfo(), bytesOffset));
        }
    }
    return fileList;
}

void MyFileSystem::PrintFileList(const std::vector<std::pair<std::string, unsigned int>>& fileList)
{
    int i = 1;
    for (std::pair<std::string, int> p : fileList)
    {
        std::cout << i++ << ". " << p.first << '\n';
    }
}

void MyFileSystem::ListFiles() {
    std::vector<std::pair<std::string, unsigned int>> fileList = MyFileSystem::GetFileList();
    PrintFileList(fileList);
}

void MyFileSystem::DeleteFile(unsigned int bytesOffset, bool restorable) {
    unsigned char deleteValue = 0xE5;
    unsigned char trueValue;
    f.seekg(bytesOffset, f.beg);
    f.read((char*)&trueValue, sizeof(trueValue));
    f.seekp(bytesOffset + ENTRY_NAME_SIZE + FILE_EXTENSION_LENGTH, f.beg);
    f.write((char*)&trueValue, sizeof(trueValue));
    f.seekp(bytesOffset, f.beg);
    f.write((char*)&deleteValue, sizeof(deleteValue));
    if(!restorable)
    {
        f.seekg(bytesOffset, f.beg);
        Entry e;
        f.read((char*)&e, sizeof(Entry));

        //erase clusters and remove from FAT
        std::vector<unsigned int> fileClusters = GetClustersChain(e.startingCluster);
        for (unsigned int cluster : fileClusters)
        {
            unsigned int clusterSectorOffset = sectorsBeforeFat + fatSize + (cluster - STARTING_CLUSTER) * sectorsPerCluster;
            unsigned int clusterOffset = clusterSectorOffset * bytesPerSector;  //in bytes
            WriteBlock(clusterOffset, 0, true, true);

            unsigned int fatOffset = sectorsBeforeFat * bytesPerSector + cluster * fatEntrySize; //in bytes
            f.seekp(fatOffset, f.beg);
            unsigned int empty = 0;
            f.write((char*)&empty, fatEntrySize);
        }

        //find the last entry to overwrite at the current offset

    }
    f.flush();
}

void MyFileSystem::MyDeleteFile()
{
    //list file
    std::vector<std::pair<std::string, unsigned int>> fileList = GetFileList();
    PrintFileList(fileList);
    std::cout << '\n';

    //choose file
    int choice;
    do
    {
        std::cout << "Enter which file to delete: ";
        std::cin >> choice;
    } while (choice <= 0 || choice > fileList.size());

    unsigned int bytesOffset = fileList[choice - 1].second;

    bool restorable;
    std::cout << "Do you want this to be restorable (1 - yes/0 - no): ";
    std::cin >> restorable;
    DeleteFile(bytesOffset, restorable);
}


std::vector<std::pair<std::string, unsigned int>> MyFileSystem::GetDeletedFileList()
{
    std::vector<std::pair<std::string, unsigned int>> fileList;
    std::vector<unsigned int> rdetClusters = GetClustersChain(STARTING_CLUSTER);
    for (unsigned int cluster : rdetClusters)
    {
        unsigned int sectorOffset = sectorsBeforeFat + fatSize + sectorsPerCluster * (cluster - STARTING_CLUSTER);
        unsigned int bytesOffset = sectorOffset * bytesPerSector;
        unsigned int limitOffset = bytesOffset + bytesPerSector * sectorsPerCluster;  //start of next cluster
        for (; bytesOffset < limitOffset; bytesOffset += sizeof(Entry))
        {
            Entry tempEntry;
            f.seekg(bytesOffset, f.beg);
            f.read((char*)&tempEntry, sizeof(Entry));
            //sign of erased file
            if (tempEntry.name[0] == -27)
            {
                std::string fileName = tempEntry.GetInfo();
                fileName[0] = tempEntry.reserved[0];
                fileList.push_back(std::make_pair(fileName, bytesOffset));
            }
            //sign of empty entry
            if (tempEntry.name[0] == 0)
            {
                return fileList;
            }
            
        }
    }
    return fileList;
}

void MyFileSystem::RestoreFile(unsigned int bytesOffset) {
    f.seekg(bytesOffset + ENTRY_NAME_SIZE + FILE_EXTENSION_LENGTH, f.beg);
    //get the first char of the name
    unsigned char trueValue;
    f.read((char*)&trueValue, sizeof(trueValue));
    f.seekp(bytesOffset, f.beg);
    f.write((char*)&trueValue, sizeof(trueValue));
}

void MyFileSystem::MyRestoreFile() {
    std::cout << "Restorable Deleted File List:\n";
    std::vector<std::pair<std::string, unsigned int>> fileList = GetDeletedFileList();
    PrintFileList(fileList);
    std::cout << '\n';

    //choose file
    int choice;
    do
    {
        std::cout << "Enter which file to restore: ";
        std::cin >> choice;
    } while (choice <= 0 || choice > fileList.size());

    unsigned int bytesOffset = fileList[choice - 1].second;
    RestoreFile(bytesOffset);
}
