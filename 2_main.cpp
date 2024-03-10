#include <fstream>

#define FS_NAME "MyFS.Dat"
#define BYTES_PER_SECTOR 512  //2 bytes
#define SECTORS_PER_CLUSTER 4  //1 byte
#define SECTORS_BEFORE_FAT 1 //1 byte
#define NUMBER_OF_FAT 2 //1 byte
#define FAT_SIZE 4081  //2 byte, each entry in FAT takes up 4 bytes
#define FAT_ENTRY_SIZE 4
#define FREE 0 //free cluster in FAT
#define VOLUME_SIZE 2097152 //4 bytes, size in sector

void CreateFS()
{
    std::ofstream file(FS_NAME, std::ios::binary | std::ios::out | std::ios::trunc);
    file.seekp(VOLUME_SIZE * BYTES_PER_SECTOR - 1);
    const char data = 0;
    file.write(&data, 1);
    file.close();
}

void WriteBootSector()
{
    std::fstream file(FS_NAME, std::ios::binary | std::ios::in | std::ios::out);

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

int main()
{
    CreateFS();
    WriteBootSector();
    return 0;
}