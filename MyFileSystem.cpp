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

void MyFileSystem::Encrypt(std::string& data, unsigned char key)
{
    for (int i = 0; i < data.size(); i++)
    {
        data[i] += key;
    }
}

void MyFileSystem::Decrypt(std::string& data, unsigned char key)
{
    for (int i = 0; i < data.size(); i++)
    {
        data[i] -= key;
    }
}

void MyFileSystem::CreateFSPassword()
{    
    //write password byte in boot sector
    f.seekp(10, f.beg);
    bool hasPassword = true;
    f.write((char*)&hasPassword, 1);
    
    //input password
    std::string password;
    std::cout << "Enter password (max length is " << MAX_PASSWORD_LENGTH << "): ";
    std::getline(std::cin, password);
    std::cin.clear();

    if (password.size() > MAX_PASSWORD_LENGTH)
        password = password.substr(0, MAX_PASSWORD_LENGTH);
    std::cout << "Your password for the file system will be: " << password << '\n';

    //simple encryption
    srand(time(0));
    unsigned char key = 0;
    while (key == 0)
        key = rand() % 256;
    Encrypt(password, key);

    //write password length and key to boot sector
    unsigned short passwordLength = password.size();
    f.write((char*)&passwordLength, 2); //offset 11d
    f.write((char*)&key, 1); //offset 13d

    //write encrypted password to boot sector
    f.seekp(16, f.beg);
    f.write(password.c_str(), password.size());
    f.flush();
}

bool MyFileSystem::CheckFSPassword(const std::string& password)
{
    //read password length
    f.seekg(11, f.beg);
    unsigned short passwordLength;
    f.read((char*)&passwordLength, 2);
    //read key
    unsigned short key;
    f.read((char*)&key, 2);
    //read password
    std::vector<char> encryptedPasswordVector = ReadBlock(16, passwordLength);
    std::string actualPassword;
    actualPassword.reserve(passwordLength);
    for (int i = 0; i < passwordLength; i++)
    {
        actualPassword += encryptedPasswordVector[i];
    }
    Decrypt(actualPassword, key);
    
    if (password == actualPassword)
        return true;
    return false;
}

bool MyFileSystem::CheckFSPassword()
{
    if (hasPassword)
    {
        int attemp = 3;
        std::string password;
        do
        {
            std::cout << "Enter password to access file system: ";
            std::getline(std::cin, password);
            std::cin.clear();
            if (!CheckFSPassword(password))
            {
                std::cout << "Wrong password!\n";
                attemp--;
                std::cout << "Number of attemps left: " << attemp << '\n';
            }
            else return true;;
        } while (attemp);

        if (attemp == 0)
            return false;
    }
    return true;
}

MyFileSystem::MyFileSystem()
{
    f.open(FS_PATH, std::ios::binary | std::ios::in | std::ios::out);
    //read boot sector
    f.read((char*)&bytesPerSector, 2);
    f.read((char*)&sectorsPerCluster, 1);
    f.read((char*)&sectorsBeforeFat, 1);
    f.read((char*)&fatSize, 2);
    f.read((char*)&volumeSize, 4);
    f.read((char*)&hasPassword, 1);

    
}

MyFileSystem::~MyFileSystem()
{
    f.close();
}

bool MyFileSystem::CheckDuplicateName(Entry *&entry)
{
    std::vector<unsigned int> rdetClusters = ReadFAT(STARTING_CLUSTER);
    for (unsigned int cluster : rdetClusters)
    {
        unsigned int sectorOffset = sectorsBeforeFat + fatSize + sectorsPerCluster * (cluster - STARTING_CLUSTER);
        unsigned int bytesOffset = sectorOffset * bytesPerSector;
        unsigned int limitOffset = bytesOffset + bytesPerSector * sectorsPerCluster;  //start of next cluster
        for (; bytesOffset < limitOffset; bytesOffset += sizeof(Entry))
        {
            std::vector<char> tempEntry = ReadBlock(bytesOffset, sizeof(Entry));
            //sign of erased file
            if (tempEntry[0] == -27)
                continue;
            //sign of empty entry
            if (tempEntry[0] == 0)
                return false;
            //if duplicated
            char tempEntryName[41] = {0};
            char entryName[41] = {0};
            for (int i = 0; i < 40; i++)
            {
                tempEntryName[i] = tempEntry[i];
                entryName[i] = entry->name[i];
            }
            
            char tempEntryExtension[4] = {0};
            char entryExtension[4] = {0};
            for (int i = 0; i < 3; i++)
            {
                tempEntryExtension[i] = tempEntry[i + 40];
                entryExtension[i] = entry->extension[i];
            }
            if (!strcmp(tempEntryName, entryName) && !strcmp(tempEntryExtension, entryExtension))
                return true;
        }
    }
    return false;
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

std::vector<unsigned int> MyFileSystem::GetFreeClusters(unsigned int n)
{
    std::vector<unsigned int> result;
    //cluster for actual file's data starts from 3
    unsigned int cluster = STARTING_CLUSTER + 1;
    f.seekp(sectorsBeforeFat * bytesPerSector + cluster * FAT_ENTRY_SIZE, f.beg);
    while (n && cluster <= FINAL_CLUSTER)
    {
        unsigned int nextCluster = 0;
        f.read((char*)&nextCluster, FAT_ENTRY_SIZE);
        if (nextCluster == 0)
        {
            n--;
            result.push_back(cluster);
        }
        cluster++;
    }
    if (n)
        return {};
    return result;
}

void MyFileSystem::WriteClustersToFAT(const std::vector<unsigned int>& clusters)
{
    int i = 0;
    while (i < clusters.size() - 1)
    {
        unsigned int offset = sectorsBeforeFat * bytesPerSector + clusters[i] * fatEntrySize;
        f.seekp(offset, f.beg);
        f.write((char*)&clusters[i + 1], fatEntrySize);
        i++;
    }
    unsigned int data = MY_EOF;
    unsigned int offset = sectorsBeforeFat * bytesPerSector + clusters[i] * fatEntrySize;
    f.seekp(offset);
    f.write((char*)&data, fatEntrySize);
    f.flush();
}

bool MyFileSystem::WriteFileEntry(Entry *&entry)
{
    std::vector<unsigned int> rdetClusters = ReadFAT(STARTING_CLUSTER);
    unsigned int lastCluster = rdetClusters[rdetClusters.size() - 1];
    unsigned int sectorOffset = sectorsBeforeFat + fatSize + sectorsPerCluster * (lastCluster - STARTING_CLUSTER);
    unsigned int bytesOffset = sectorOffset * bytesPerSector;
    unsigned int limitOffset = bytesOffset + bytesPerSector * sectorsPerCluster;  //start of next cluster
    for (; bytesOffset < limitOffset; bytesOffset += sizeof(Entry))
    {
        std::vector<char> tempEntry = ReadBlock(bytesOffset, sizeof(Entry));
        if (tempEntry[0] == 0)
        {
            f.seekp(bytesOffset, f.beg);
            f.write((char*)entry, sizeof(Entry));
            f.flush();
            return true;
        }
    }

    //if out of space, append another cluster for RDET
    std::vector<unsigned int> newFreeCluster = GetFreeClusters(1); 
    if (newFreeCluster.empty())
        return false;

    //write to FAT new cluster of RDET
    unsigned int offset = sectorsBeforeFat * bytesPerSector + rdetClusters[rdetClusters.size() - 1] * fatEntrySize;
    f.seekp(offset, f.beg);
    f.write((char*)&newFreeCluster[0], f.beg);
    offset = sectorsBeforeFat * bytesPerSector + newFreeCluster[0] * fatEntrySize;
    f.seekp(offset, f.beg);
    unsigned int eof = MY_EOF;
    f.write((char*)&eof, sizeof(FAT_ENTRY_SIZE));

    //write entry to new cluster
    sectorOffset = sectorsBeforeFat + fatSize + sectorsPerCluster * (newFreeCluster[0] - STARTING_CLUSTER);
    bytesOffset = sectorOffset * bytesPerSector;
    f.seekp(bytesOffset, f.beg);
    f.write((char*)entry, sizeof(Entry));
    f.flush();
    return true;
}

void MyFileSystem::WriteFileContent(std::ifstream& fin, const std::vector<unsigned int>& clusters)
{
    for (unsigned int cluster : clusters)
    {
        const unsigned int clusterSize = bytesPerSector * sectorsPerCluster;
        std::vector<char> data(clusterSize, 0);
        fin.read(&data[0], clusterSize);
        unsigned int sectorOffset = sectorsBeforeFat + fatSize + (cluster - STARTING_CLUSTER) * sectorsPerCluster;
        unsigned int bytesOffset = sectorOffset * bytesPerSector;
        f.seekp(bytesOffset, f.beg);
        f.write(&data[0], clusterSize);
    }
    f.flush();
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
    //get name
    std::string fileName = inputPath.substr(inputPath.find_last_of("/\\") + 1);
    std::string extension = fileName.substr(fileName.find_last_of(".") + 1);
    fileName = fileName.substr(0, fileName.find_last_of("."));
    Entry *entry = new Entry();
    //create short name
    entry->SetExtension(extension);
    unsigned int number = 1;
    entry->SetName(fileName, number);
    while(CheckDuplicateName(entry))
    {
        number++;
        entry->SetName(fileName, number, true);
    }
    //get attributes
    entry->attributes = (unsigned char)(GetFileAttributesA(inputPath.c_str()) & 63);  //prevent overflow
    //get date and time attributes
    entry->SetDateAndTime(fileAttributes);
    //get file size
    entry->fileSize = fileAttributes.nFileSizeLow;

    //find free cluster and write to FAT
    //calculate how many clusters needed
    unsigned int clustersNeeded = (unsigned int)(std::ceil((double)entry->fileSize / (bytesPerSector * sectorsPerCluster)));
    std::vector<unsigned int> freeClusters = GetFreeClusters(clustersNeeded);
    if (freeClusters.empty())
    {
        std::cout << "Out of clusters for file!\n";
        delete entry;
        return;
    }
    entry->startingCluster = freeClusters[0];
    WriteClustersToFAT(freeClusters);

    //write entries
    if (!WriteFileEntry(entry))
    {
        std::cout << "Out of space for file entry!\n";
        delete entry;
        return;
    }

    //write file's content
    WriteFileContent(fin, freeClusters);
    
    delete entry;
}

void MyFileSystem::test()
{
    
}

void MyFileSystem::Entry::SetExtension(const std::string& fileExtension)
{
    for (int i = 0; i < FILE_EXTENSION_LENGTH; i++)
        extension[i] = fileExtension[i];
}

void MyFileSystem::Entry::SetName(const std::string& fileName, unsigned int number, bool adjust)
{
    if (!adjust)
    {
        std::string numberStr = std::to_string(number);
        int size = 40 - numberStr.size();
        //fill number
        for (int i = size; i < 40; i++)
            name[i] = numberStr[i - size];
        name[size - 1] = '~';

        int i = 0;
        for (; fileName[i] != '\0' && i < size - 1; i++)
        {
            name[i] = fileName[i];
        }
        //fill with spaces
        while (i < size - 1)
        {
            name[i++] = ' ';
        }
    }
    //this branch doesnt account for when new number has fewer digits than old number
    else
    {
        int i = 1;
        for (int j = 38; j > -1; j--)
        {
            if (name[j] == '~')
            {
                i = j;
                break;
            } 
        }
        unsigned int oldNumber = std::atoi(std::string(name + i + 1, name + 40).c_str());
        i -= std::to_string(number).size() - std::to_string(oldNumber).size();

        name[i] = '~';
        std::string newNumberStr = std::to_string(number);
        for (int j = i + 1; j < 40; j++)
        {
            name[j] = newNumberStr[j - i - 1];
        }
    }
}

void MyFileSystem::Entry::SetDateAndTime(const WIN32_FILE_ATTRIBUTE_DATA& fileAttributes)
{
    FileTimeToDosDateTime(&fileAttributes.ftCreationTime, &creationDate, &creationTime);
    WORD lastAccessTime;
    FileTimeToDosDateTime(&fileAttributes.ftLastAccessTime, &lastAccessDate, &lastAccessTime);
    FileTimeToDosDateTime(&fileAttributes.ftLastWriteTime, &lastWriteDate, &lastWriteTime);
}