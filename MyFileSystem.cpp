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

//hashing using PKCS5_PBKDF2_HMAC with SHA256
//https://www.cryptopp.com/wiki/PKCS5_PBKDF2_HMAC
//sample program 1
std::string MyFileSystem::GenerateHash(const std::string& data)
{
    using namespace CryptoPP;

    byte *secret = new byte[data.size()];
    size_t secretLen = data.size();
    for (int i = 0; i < secretLen; i++)
    {
        secret[i] = data[i];
    }
    
    byte salt[] = "Sodium Cloride";
    size_t saltLen = strlen((const char*)salt);

    byte derived[SHA256::DIGESTSIZE];

    PKCS5_PBKDF2_HMAC<SHA256> pbkdf;
	byte unused = 0;

    pbkdf.DeriveKey(derived, sizeof(derived), unused, secret, secretLen, salt, saltLen, 10000, 0.0f);

    std::string result(derived, derived + sizeof(derived));

    delete[] secret;
    return result;
}

//encrypting using XChaCha20Poly1305
//https://www.cryptopp.com/wiki/XChaCha20Poly1305
//example
void MyFileSystem::EncryptData(std::string& data, const std::string& key, Entry *&entry)
{
    if (data.size() == 0)
    {
        std::cout << "Program does not encrypt empty file!\n";
        return;
    }
    using namespace CryptoPP;

    //24 bytes
    const byte iv[] = 
    {
        0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
        0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x58
    };
    
    byte *encrypted = new byte[data.size()];

    XChaCha20Poly1305::Encryption enc;
    enc.SetKeyWithIV((byte*)key.c_str(), key.size(), iv, sizeof(iv));
    enc.EncryptAndAuthenticate(encrypted, entry->mac, sizeof(entry->mac), iv, sizeof(iv), nullptr, 0, (byte*)data.c_str(), data.size());

    data = std::string(encrypted, encrypted + data.size());
    delete[] encrypted;
}

void MyFileSystem::DecryptData(std::string& data, const std::string& key, Entry *&entry)
{
    if (data.size() == 0)
        return;

    using namespace CryptoPP;

    //24 bytes
    const byte iv[] = 
    {
        0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
        0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x58
    };
    
    byte *decrypted = new byte[data.size()];

    XChaCha20Poly1305::Decryption dec;
    dec.SetKeyWithIV((byte*)key.c_str(), key.size(), iv, sizeof(iv));
    dec.DecryptAndVerify(decrypted, entry->mac, sizeof(entry->mac), iv, sizeof(iv), nullptr, 0, (byte*)data.c_str(), data.size());

    data = std::string(decrypted, decrypted + data.size());
    delete[] decrypted;
}

void MyFileSystem::CreateFSPassword()
{    
    //write password byte in boot sector
    f.seekp(10, f.beg);
    bool hasPassword = true;
    f.write((char*)&hasPassword, 1);
    
    //input password
    std::string password;
    std::cin.clear();
    std::cout << "Enter new password: ";
    std::cin >> password;
    std::cout << "Your password for the file system will be: " << password << '\n';
    std::string hashedPassword = GenerateHash(password);

    //write hashed password to volume
    f.seekp(32, f.beg);
    f.write(hashedPassword.c_str(), hashedPassword.size());

    f.flush();
}

bool MyFileSystem::CheckFSPassword(const std::string& password)
{
    f.seekg(32, f.beg);
    std::string encrypedPassword(32, 0);
    f.read(&encrypedPassword[0], 32);

    return encrypedPassword == GenerateHash(password);
}

void MyFileSystem::ChangeFSPassword()
{
    std::cin.clear();
    std::string password;
    std::cout << "Enter file system's password: ";
    std::cin >> password;

    if (!CheckFSPassword(password))
    {
        std::cout << "Wrong password!\n";
        return;
    }

    CreateFSPassword();
}

bool MyFileSystem::CheckFSPassword()
{
    if (hasPassword)
    {
        int attemp = 3;
        std::string password;
        do
        {
            std::cin.clear();
            std::cout << "Enter password to access file system: ";
            std::cin >> password;
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
            if (tempEntry[0] == -27)
                continue;
            //sign of empty entry
            if (tempEntry[0] == 0)
                return false;
            //if duplicated
            std::string tempEntryName(tempEntry.begin(), tempEntry.begin() + ENTRY_NAME_SIZE);
            std::string entryName(entry->name, entry->name + ENTRY_NAME_SIZE);
            std::string tempEntryExtension(tempEntry.begin() + ENTRY_NAME_SIZE, tempEntry.begin() + ENTRY_NAME_SIZE + FILE_EXTENSION_LENGTH);
            std::string entryExtension(entry->extension, entry->extension + FILE_EXTENSION_LENGTH);
            if (tempEntryName == entryName && tempEntryExtension == entryExtension)
                return true;
        }
    }
    return false;
}

std::vector<unsigned int> MyFileSystem::GetClustersChain(unsigned int startingCluster)
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
    std::vector<unsigned int> rdetClusters = GetClustersChain(STARTING_CLUSTER);
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

void MyFileSystem::WriteFileContent(const std::string& data, const std::vector<unsigned int>& clusters)
{
    int i = 0;
    const unsigned int clusterSize = bytesPerSector * sectorsPerCluster;
    for (unsigned int cluster : clusters)
    {
        unsigned int sectorOffset = sectorsBeforeFat + fatSize + (cluster - STARTING_CLUSTER) * sectorsPerCluster;
        unsigned int bytesOffset = sectorOffset * bytesPerSector;
        f.seekp(bytesOffset, f.beg);
        size_t size = ((size_t)clusterSize < data.size() - i) ? clusterSize : data.size() - i;
        f.write(&data[i], size);
        i += clusterSize;
    }
    f.flush();
}

std::string MyFileSystem::ReadFileContent(unsigned int fileSize, const std::vector<unsigned int>& clusters)
{
    std::string data(fileSize, 0);
    int i = 0;
    const unsigned int clusterSize = bytesPerSector * sectorsPerCluster;
    for (unsigned int cluster : clusters)
    {
        unsigned int sectorOffset = sectorsBeforeFat + fatSize + (cluster - STARTING_CLUSTER) * sectorsPerCluster;
        unsigned int bytesOffset = sectorOffset * bytesPerSector;
        f.seekg(bytesOffset, f.beg);
        size_t size = ((size_t)clusterSize < data.size() - i) ? clusterSize : data.size() - i;
        f.read(&data[i], size);
        i += clusterSize;
    }
    return data;
}

void MyFileSystem::ImportFile(const std::string& inputPath, bool hasPassword)
{
    std::ifstream fin(inputPath, std::ios::binary | std::ios::in);
    if (!fin)
    {
        std::cout << "Path does not exist\n";
        return;
    }

    fin.seekg(0, f.end);
    unsigned int fileSize = fin.tellg();
    fin.clear();
    fin.seekg(0, f.beg);

    //check file size limit
    unsigned int limit = bytesPerSector * sectorsPerCluster * (NUMBER_OF_CLUSTERS - 1);
    if (fileSize > limit)
    {
        std::cout << "File's size is too large!\n";
        fin.close();
        return;
    }

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
    //get file size
    entry->fileSize = fileSize;

    //find free cluster and write to FAT
    //calculate how many clusters needed
    unsigned int clustersNeeded = (unsigned int)(std::ceil((double)entry->fileSize / (bytesPerSector * sectorsPerCluster)));
    if (clustersNeeded == 0)
        clustersNeeded++;
    std::vector<unsigned int> freeClusters = GetFreeClusters(clustersNeeded);
    if (freeClusters.empty())
    {
        std::cout << "Out of clusters for file!\n";
        delete entry;
        fin.close();
        return;
    }
    entry->startingCluster = freeClusters[0];
    WriteClustersToFAT(freeClusters);

    //read the whole file into memory
    std::string fileData(entry->fileSize, 0);
    fin.read(&fileData[0], entry->fileSize);

    //Create file password and encrypt file's content
    if (hasPassword)
    {
        entry->hasPassword = true;
        //set password's hash
        std::string password;
        std::cin.clear();
        std::cout << "Enter file's password: ";
        std::cin >> password;

        std::string hashedPassword = GenerateHash(password);
        std::string doublyHashedPassword = GenerateHash(hashedPassword);
        entry->SetHash(doublyHashedPassword);

        EncryptData(fileData, hashedPassword, entry);
    }

    //write entries
    if (!WriteFileEntry(entry))
    {
        std::cout << "Out of space for file entry!\n";
        delete entry;
        fin.close();
        return;
    }

    //write file's content
    WriteFileContent(fileData, freeClusters);
    
    delete entry;
    fin.close();
}

void MyFileSystem::ImportFile()
{
    std::string path;
    std::cin.clear();
    std::cout << "Enter file path: ";
    std::cin >> path;

    bool hasPassword;
    std::cout << "Do you want to set password? (1 - yes/0 - no): ";
    std::cin >> hasPassword;

    ImportFile(path, hasPassword);
}

bool MyFileSystem::CheckFilePassword(Entry *&entry, std::string& filePassword)
{
    std::string hashedPassword(entry->hashedPassword, entry->hashedPassword + 32);
    std::cin.clear();
    std::cout << "Enter file's password: ";
    std::cin >> filePassword;

    std::string key = GenerateHash(filePassword);
    if (GenerateHash(key) != hashedPassword)
    {
        std::cout << "Wrong password!\n";
        return false;
    }

    return true;
}

void MyFileSystem::ChangeFilePassword(Entry *&entry, unsigned int offset, bool removed)
{
    std::vector<unsigned int> fileClusters = GetClustersChain(entry->startingCluster);
    std::string fileData = ReadFileContent(entry->fileSize, fileClusters);
    if (entry->hasPassword)
    {
        std::string filePassword;
        if (!CheckFilePassword(entry, filePassword))
            return;

        DecryptData(fileData, GenerateHash(filePassword), entry);
    }

    if (!removed)
    {
        std::string newPassword;
        std::cin.clear();
        std::cout << "Enter file's new password: ";
        std::cin >> newPassword;

        std::string hashedPassword = GenerateHash(newPassword);
        std::string doublyHashedPassword = GenerateHash(hashedPassword);
        entry->SetHash(doublyHashedPassword);

        EncryptData(fileData, hashedPassword, entry);
    }
    else entry->hasPassword = false;
    
    //rewrite data
    WriteFileContent(fileData, fileClusters);

    //rewrite file entry
    f.seekp(offset, f.beg);
    f.write((char*)entry, sizeof(Entry));
    f.flush();
}

void MyFileSystem::ExportFile(const std::string& outputPath, Entry *&entry)
{
    std::vector<unsigned int> fileClusters = GetClustersChain(entry->startingCluster);
    std::string fileData = ReadFileContent(entry->fileSize, fileClusters);

    if (entry->hasPassword)
    {
        std::string filePassword;
        if (!CheckFilePassword(entry, filePassword))
            return;

        DecryptData(fileData, GenerateHash(filePassword), entry);
    }

    std::string fileName = entry->GetFullName();
    std::ofstream fout(outputPath + fileName, std::ios::binary | std::ios::out);
    fout.write(&fileData[0], fileData.size());
    fout.close();
}

void MyFileSystem::ExportFile()
{
    std::string path;
    std::cin.clear();
    std::cout << "Enter output path: ";
    std::cin >> path;

    //list file, get file entry
    //...
    //list file, get file entry

    //export
    //...
    //export
}

void MyFileSystem::test()
{
    std::cout << sizeof(Entry);
    //CreateFSPassword();
    ImportFile();
    // Entry *e = new Entry();
    // f.seekg(2094080, f.beg);
    // f.read((char*)e, sizeof(Entry));
    // ChangeFilePassword(e, 2094080);
    // delete e;

    // ExportFile();
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
        int size = ENTRY_NAME_SIZE - numberStr.size();
        //fill number
        for (int i = size; i < ENTRY_NAME_SIZE; i++)
            name[i] = numberStr[i - size];
        name[size - 1] = '~';

        int i = 0;
        for (; fileName[i] != '\0' && i < size - 1; i++)
        {
            name[i] = fileName[i];
        }
        nameLen = i;
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
        for (int j = ENTRY_NAME_SIZE - 2; j > -1; j--)
        {
            if (name[j] == '~')
            {
                i = j;
                break;
            } 
        }
        unsigned int oldNumber = std::atoi(std::string(name + i + 1, name + ENTRY_NAME_SIZE).c_str());
        nameLen = std::min(i, nameLen);
        i -= std::to_string(number).size() - std::to_string(oldNumber).size();

        name[i] = '~';
        std::string newNumberStr = std::to_string(number);
        for (int j = i + 1; j < ENTRY_NAME_SIZE; j++)
        {
            name[j] = newNumberStr[j - i - 1];
        }
    }
}

std::string MyFileSystem::Entry::GetFullName()
{
    std::string result(name, name + nameLen);
    result += '.' + std::string(extension, extension + FILE_EXTENSION_LENGTH);
    return result;
}

void MyFileSystem::Entry::SetHash(const std::string& hash)
{
    for (int i = 0; i < 32; i++)
    {
        hashedPassword[i] = hash[i];
    }
}