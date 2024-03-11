#include <fstream>
#include <iostream>
#include "MyFileSystem.h"

void CreateFS()
{
    std::ofstream file(FS_PATH, std::ios::binary | std::ios::out | std::ios::trunc);
    file.seekp(VOLUME_SIZE * BYTES_PER_SECTOR - 1);
    const char data = 0;
    file.write(&data, 1);
    file.close();
}

void WriteBootSector()
{
    std::fstream file(FS_PATH, std::ios::binary | std::ios::in | std::ios::out);

    unsigned int data = BYTES_PER_SECTOR;
    file.write((char*)&data, 2);

    data = SECTORS_PER_CLUSTER;
    file.write((char*)&data, 1);
    
    data = SECTORS_BEFORE_FAT;
    file.write((char*)&data, 1);

    data = NUMBER_OF_FAT;
    file.write((char*)&data, 1);

    data = FAT_SIZE;
    file.write((char*)&data, 2);

    data = VOLUME_SIZE;
    file.write((char*)&data, 4);  

    file.close();
}

bool CheckFSExists()
{
    std::ifstream f;
    f.open(FS_PATH);
    bool result = (bool)f;
    f.close();
    return result;
}

int main()
{
    if (!CheckFSExists())
    {
        std::cout << "Creating File System file" << '\n';
        CreateFS();
        WriteBootSector();
    }

    MyFileSystem myFS;
    return 0;
}