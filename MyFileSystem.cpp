#include "MyFileSystem.h"

std::string MyFileSystem::ReadBlock(unsigned int offset, bool readCluster)
{
    f.seekg(offset, f.beg);
    int size = BYTES_PER_SECTOR;
    if (readCluster)
        size *= SECTORS_PER_CLUSTER;
    
    std::vector<char> buffer(size, 0);
    f.read(&buffer[0], size);
    std::string data(buffer.begin(), buffer.end());
    return data;
}

void MyFileSystem::WriteBlock(unsigned int offset, std::string data)
{
    f.seekp(offset, f.beg);
    f.write(data.c_str(), data.size());
}

void MyFileSystem::WriteBlock(unsigned int offset, unsigned int data)
{
    f.seekp(offset, f.beg);
    f.write((char*)&data, sizeof(data));
}

MyFileSystem::MyFileSystem()
{
    f.open(FS_PATH, std::ios::binary | std::ios::in | std::ios::out);
    f.read((char*)&bytesPerSector, 2);
    f.read((char*)&sectorsPerCluster, 1);
    f.read((char*)&sectorsBeforeFat, 1);
    f.read((char*)&numberOfFat, 1);
    f.read((char*)&fatSize, 2);
    f.read((char*)&volumeSize, 4);
}

MyFileSystem::~MyFileSystem()
{
    f.close();
}

std::vector<unsigned int> MyFileSystem::ReadFAT(unsigned int startingCluster)
{
    if (startingCluster < STARTING_CLUSTER)
        return {};

    std::vector<unsigned int> result;
    result.push_back(startingCluster);
    f.seekg(SECTORS_BEFORE_FAT * BYTES_PER_SECTOR + startingCluster * FAT_ENTRY_SIZE, f.beg);
    unsigned int nextCluster = 0;
    f.read((char*)&nextCluster, FAT_ENTRY_SIZE);
    while (nextCluster != EOF)
    {
        result.push_back(nextCluster);
        f.seekg(SECTORS_BEFORE_FAT * BYTES_PER_SECTOR + nextCluster * FAT_ENTRY_SIZE, f.beg);
        std::cout << f.tellg() << '\n';
        f.read((char*)&nextCluster, FAT_ENTRY_SIZE);
    }
    return result;
}