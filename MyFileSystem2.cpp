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
    for (std::pair<std::string, int> p: fileList) 
    {
        std::cout << i++ << ". " << p.first << '\n';
    }
}

void MyFileSystem::ListFiles() {
    std::vector<std::pair<std::string, unsigned int>> fileList = MyFileSystem::GetFileList();
    PrintFileList(fileList);
}

void MyFileSystem::MyDeleteFile(unsigned int bytesOffset, bool restorable) {
    if (restorable) {
        unsigned char deleteValue = 0xE5;
        unsigned char trueValue;
        f.seekg(bytesOffset, f.beg);
        f.read((char*)&trueValue, sizeof(trueValue));
        f.seekp(bytesOffset + ENTRY_NAME_SIZE + FILE_EXTENSION_LENGTH, f.beg);
        f.write((char*)&trueValue, sizeof(trueValue));
        f.seekp(bytesOffset, f.beg);
        f.write((char*)&deleteValue, sizeof(deleteValue));
    }
    else
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
    MyDeleteFile(bytesOffset, restorable);
}

void MyFileSystem::MyRestoreFile(std::string restoreName) {
    std::vector<unsigned int> rdetClusters = GetClustersChain(STARTING_CLUSTER);
    for (unsigned int cluster : rdetClusters)
    {
        unsigned int sectorOffset = sectorsBeforeFat + fatSize + sectorsPerCluster * (cluster - STARTING_CLUSTER);
        unsigned int bytesOffset = sectorOffset * bytesPerSector;
        unsigned int limitOffset = bytesOffset + bytesPerSector * sectorsPerCluster;  //start of next cluster
        for (; bytesOffset < limitOffset; bytesOffset += sizeof(Entry))
        {
            std::vector<char> tempEntry = ReadBlock(bytesOffset, sizeof(Entry));
            //sign of erased file
            if (tempEntry[0] == -27) {
                std::string fileName = "";
                fileName += tempEntry[62];
                //get the last index of the file name
                int finalIndex = 37;
                while (tempEntry[finalIndex] == ' ') {
                    finalIndex--;
                }
                for (int i = 1; i <= finalIndex; i++) {
                    fileName += tempEntry[i];
                }
                //check duplicate file name;
                if (tempEntry[39] == '1') {
                    fileName = fileName + '.' + tempEntry[40] + tempEntry[41] + tempEntry[42];
                }
                else {
                    char duplicateIndex = tempEntry[39] - 1;
                    fileName = fileName + '(' + duplicateIndex + ')' + '.' + tempEntry[40] + tempEntry[41] + tempEntry[42];
                }
                if (fileName != restoreName) {
                    continue;
                }
                else {
                    f.seekp(bytesOffset, f.beg);
                    f.write((char*)&tempEntry[62], sizeof(tempEntry[62]));
                    return;
                }
            }
            //sign of empty entry
            if (tempEntry[0] == 0)
                return;
            
        }
    }
}

void MyFileSystem::MyRestoreFile()
{

}