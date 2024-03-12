#include "MyFileSystem.h"

std::vector<char> MyFileSystem::ReadBlock(unsigned int offset, unsigned int size)
{
    f.seekg(offset, f.beg);
    std::vector<char> buffer(size, 0);
    f.read(&buffer[0], size);
    return buffer;
}

void MyFileSystem::WriteBlock(unsigned int offset, std::string data, bool writeCluster, bool padding)
{
    f.seekp(offset, f.beg);
    f.write(data.c_str(), data.size());
    //add padding
    if (padding)
    {
        unsigned int paddingSize = writeCluster ? sectorsPerCluster * bytesPerSector : bytesPerSector;
        paddingSize -= data.size();
        std::string paddingStr(paddingSize, '\0');
        f.write(paddingStr.c_str(), paddingSize);
    }
    f.flush();
}

void MyFileSystem::WriteBlock(unsigned int offset, unsigned int data, bool writeCluster, bool padding)
{
    f.seekp(offset, f.beg);
    f.write((char*)&data, sizeof(data));
    //add padding
    if (padding)
    {
        unsigned int paddingSize = writeCluster ? sectorsPerCluster * bytesPerSector : bytesPerSector;
        paddingSize -= sizeof(data);
        std::string paddingStr(paddingSize, '\0');
        f.write(paddingStr.c_str(), paddingSize);
    }
    f.flush();
}

MyFileSystem::MyFileSystem()
{
    f.open(FS_PATH, std::ios::binary | std::ios::in | std::ios::out);
    //read boot sector
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
    std::vector<unsigned int> result;
    result.push_back(startingCluster);
    f.seekg(sectorsBeforeFat * bytesPerSector + startingCluster * FAT_ENTRY_SIZE, f.beg);
    unsigned int nextCluster = 0;
    f.read((char*)&nextCluster, FAT_ENTRY_SIZE);
    if (nextCluster == 0)
        return {};
    while (nextCluster != MY_EOF)
    {
        result.push_back(nextCluster);
        f.seekg(sectorsBeforeFat * bytesPerSector + nextCluster * FAT_ENTRY_SIZE, f.beg);
        f.read((char*)&nextCluster, FAT_ENTRY_SIZE);
    }
    return result;
}

bool MyFileSystem::CheckDuplicateShortName(const std::string& name)
{
    std::vector<unsigned int> rdetClusters = ReadFAT(2);
    for (unsigned int cluster : rdetClusters)
    {
        unsigned int clusterOffset = sectorsBeforeFat + numberOfFat * fatSize + sectorsPerCluster * (cluster - STARTING_CLUSTER);
        clusterOffset *= bytesPerSector;
        for (int i = 0; i < bytesPerSector * sectorsPerCluster / sizeof(MainEntry); i++)
        {
            std::vector<char> entry = ReadBlock(clusterOffset, sizeof(MainEntry));
            //sign of long entry or erased file
            if (entry[11] == 15 || entry[0] == -27)
                continue;
            //if duplicated
            if (std::string(entry.begin(), entry.begin() + 8) == name)
                return true;
            //if empty entry
            if (entry[0] == 0)
                break;
        }
    }
    return false;
}

void MyFileSystem::ImportFile(const std::string& inputPath)
{
    std::ifstream fin(inputPath, std::ios::binary | std::ios::in);
    if (!fin)
    {
        std::cout << "Path does not exist\n";
        return;
    }

    //get file attributes
    //https://stackoverflow.com/questions/26831838/how-to-get-file-information
    //https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfileattributesexa?redirectedfrom=MSDN
    //https://www.geeksforgeeks.org/convert-stdstring-to-lpcwstr-in-c/
    WIN32_FILE_ATTRIBUTE_DATA fileAttributes{};
    GetFileAttributesEx(std::wstring(inputPath.begin(), inputPath.end()).c_str(), GetFileExInfoStandard, &fileAttributes);
    
    //check file size limit
    unsigned int limit = bytesPerSector * sectorsPerCluster * NUMBER_OF_CLUSTERS;
    if (fileAttributes.nFileSizeHigh > 0 || fileAttributes.nFileSizeLow > limit)
        return;

    //get properties
    std::string fileName = inputPath.substr(inputPath.find_last_of("/\\") + 1);
    std::string extension = fileName.substr(fileName.find_last_of(".") + 1);
    fileName = fileName.substr(0, fileName.find_last_of("."));
    MainEntry *entry = new MainEntry();

    //get name
    entry->SetExtension(extension);
    unsigned int number = 0;
    do
    {
        number++;
        entry->CreateShortName(fileName, number);
    } while (CheckDuplicateShortName(std::string(entry->name, entry->name + 8)));
    //get attributes
    entry->attributes = (unsigned char)(GetFileAttributesA(inputPath.c_str()) & 63);  //prevent overflow
    //get date and time attributes
    entry->SetDateAndTime(fileAttributes);
    //get file size
    entry->fileSize = fileAttributes.nFileSizeLow;

    //create short entry
    //...
    //create short entry

    //find free cluster and write to FAT
    //...
    //find free cluster and write to FAT

    //write entries
    //...
    //write entries

    //write file's content
    //...
    //write file's content
    
    delete entry;
}

void MyFileSystem::test()
{
    std::cout << sizeof(MainEntry) << '\n';
    ImportFile("D:\\wacom settings 2.png");
}

void MyFileSystem::MainEntry::SetExtension(const std::string& fileExtension)
{
    for (int i = 0; i < fileExtension.size(); i++)
        extension[i] = toupper(fileExtension[i]);
    extension[3] = 0;
}

void MyFileSystem::MainEntry::CreateShortName(const std::string& fileName, unsigned int number)
{
    name[8] = 0;
    std::string numberStr = std::to_string(number);
    int size = 8 - numberStr.size();
    //fill number
    for (int i = size; i < 8; i++)
        name[i] = numberStr[i - size];
    name[size - 1] = '~';
    int j = 0;
    //get non-space letters
    for (int i = 0; fileName[i] != '\0' && j < size - 1; i++)
    {
        if (fileName[i] != ' ')
        {
            name[j] = std::toupper(fileName[i]);
            j++;
        }
    }
    //fill with spaces
    while (j < size - 1)
    {
        name[j] = ' ';
        j++;
    }
}

void MyFileSystem::MainEntry::SetDateAndTime(const WIN32_FILE_ATTRIBUTE_DATA& fileAttributes)
{
    FileTimeToDosDateTime(&fileAttributes.ftCreationTime, &creationDate, &creationTime);
    WORD lastAccessTime;
    FileTimeToDosDateTime(&fileAttributes.ftLastAccessTime, &lastAccessDate, &lastAccessTime);
    FileTimeToDosDateTime(&fileAttributes.ftLastWriteTime, &lastWriteDate, &lastWriteTime);
}