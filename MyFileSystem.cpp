#include "MyFileSystem.h"

std::vector<char> MyFileSystem::ReadBlock(unsigned int offset, unsigned int size)
{
    f.seekg(offset, f.beg);
    std::vector<char> buffer(size, 0);
    f.read(&buffer[0], size);
    return buffer;
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
    this->hasPassword = hasPassword;
    
    //input password
    std::string password;
    std::cout << "Enter new password: ";
    std::cin >> password;
    std::cin.ignore();
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
    std::string password;
    std::cout << "Enter file system's password: ";
    std::cin >> password;
    std::cin.ignore();

    if (!CheckFSPassword(password))
    {
        std::cout << "Incorrect password!\n";
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
            std::cout << "Enter password to access file system: ";
            std::cin >> password;
            std::cin.ignore();
            if (!CheckFSPassword(password))
            {
                std::cout << "Incorrect password!\n";
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
    f.seekg(sectorsBeforeFat * bytesPerSector + startingCluster * fatEntrySize, f.beg);
    unsigned int nextCluster = 0;
    f.read((char*)&nextCluster, fatEntrySize);
    if (nextCluster == 0)
        return {};
    while (nextCluster != MY_EOF)
    {
        result.push_back(nextCluster);
        f.seekg(sectorsBeforeFat * bytesPerSector + nextCluster * fatEntrySize, f.beg);
        f.read((char*)&nextCluster, fatEntrySize);
    }
    return result;
}

std::vector<unsigned int> MyFileSystem::GetFreeClusters(unsigned int n)
{
    std::vector<unsigned int> result;
    //cluster for actual file's data starts from 3
    unsigned int cluster = STARTING_CLUSTER + 1;
    f.seekp(sectorsBeforeFat * bytesPerSector + cluster * fatEntrySize, f.beg);
    while (n && cluster <= FINAL_CLUSTER)
    {
        unsigned int nextCluster = 0;
        f.read((char*)&nextCluster, fatEntrySize);
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
    for (unsigned int cluster : rdetClusters)
    {
        unsigned int sectorOffset = sectorsBeforeFat + fatSize + sectorsPerCluster * (cluster - STARTING_CLUSTER);
        unsigned int bytesOffset = sectorOffset * bytesPerSector;
        unsigned int limitOffset = bytesOffset + bytesPerSector * sectorsPerCluster;  //start of next cluster
        for (; bytesOffset < limitOffset; bytesOffset += sizeof(Entry))
        {
            std::vector<char> tempEntry = ReadBlock(bytesOffset, sizeof(Entry));
            if (tempEntry[0] == 0 || (tempEntry[0] == -27 && tempEntry[ENTRY_NAME_SIZE + FILE_EXTENSION_LENGTH] == 0))
            {
                f.seekp(bytesOffset, f.beg);
                f.write((char*)entry, sizeof(Entry));
                f.flush();
                return true;
            }
        }
    }
    
    //if out of space, append another cluster for RDET
    std::vector<unsigned int> newFreeCluster = GetFreeClusters(1); 
    if (newFreeCluster.empty())
        return false;

    //write to FAT new cluster of RDET
    unsigned int offset = sectorsBeforeFat * bytesPerSector + rdetClusters[rdetClusters.size() - 1] * fatEntrySize;
    f.seekp(offset, f.beg);
    f.write((char*)&newFreeCluster[0], fatEntrySize);
    offset = sectorsBeforeFat * bytesPerSector + newFreeCluster[0] * fatEntrySize;
    f.seekp(offset, f.beg);
    unsigned int eof = MY_EOF;
    f.write((char*)&eof, fatEntrySize);

    //write entry to new cluster
    unsigned int sectorOffset = sectorsBeforeFat + fatSize + sectorsPerCluster * (newFreeCluster[0] - STARTING_CLUSTER);
    unsigned int bytesOffset = sectorOffset * bytesPerSector;
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
    std::cout << "Enter file path: ";
    std::getline(std::cin, path);

    bool hasPassword;
    std::cout << "Do you want to set password? (1 - yes/0 - no): ";
    std::cin >> hasPassword;
    std::cin.ignore();

    ImportFile(path, hasPassword);
}

bool MyFileSystem::CheckFilePassword(Entry *&entry, std::string& filePassword)
{
    std::string hashedPassword(entry->hashedPassword, entry->hashedPassword + 32);
    std::cout << "Enter file's password: ";
    std::cin >> filePassword;
    std::cin.ignore();

    std::string key = GenerateHash(filePassword);
    if (GenerateHash(key) != hashedPassword)
    {
        std::cout << "Incorrect password!\n";
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
        {
            std::cout << "Incorrect password!\n";
            return;
        }

        DecryptData(fileData, GenerateHash(filePassword), entry);
    }

    if (!removed)
    {
        std::string newPassword;
        std::cout << "Enter file's new password: ";
        std::cin >> newPassword;
        std::cin.ignore();

        std::string hashedPassword = GenerateHash(newPassword);
        std::string doublyHashedPassword = GenerateHash(hashedPassword);
        entry->SetHash(doublyHashedPassword);

        EncryptData(fileData, hashedPassword, entry);
        entry->hasPassword = true;
    }
    else entry->hasPassword = false;
    
    //rewrite data
    WriteFileContent(fileData, fileClusters);

    //rewrite file entry
    f.seekp(offset, f.beg);
    f.write((char*)entry, sizeof(Entry));
    f.flush();
}

void MyFileSystem::ChangeFilePassword()
{
    std::vector<std::pair<std::string, unsigned int>> fileList = GetFileList();
    if (fileList.empty())
    {
        std::cout << "There is no file!\n";
        return;
    }
    PrintFileList(fileList);

    //choose file
    int choice;
    do
    {
        std::cout << "Enter which file to create/change password: ";
        std::cin >> choice;
        std::cin.ignore();
    } while (choice <= 0 || choice > fileList.size());

    //read entry
    f.seekg(fileList[choice - 1].second);
    Entry *e = new Entry();
    f.read((char*)e, sizeof(Entry));

    bool removed = false;
    if (e->hasPassword)
    {
        std::cout << "Do you want to remove this file's password (1 - yes/0 - no): ";
        std::cin >> removed;
        std::cin.ignore();
    }
    
    ChangeFilePassword(e, fileList[choice - 1].second, removed);

    //free memory
    delete e;
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
    //list file
    std::vector<std::pair<std::string, unsigned int>> fileList = GetFileList();
    if (fileList.empty())
    {
        std::cout << "There is no file!\n";
        return;
    }
    PrintFileList(fileList);
    std::cout << '\n';

    //choose file
    int choice;
    do
    {
        std::cout << "Enter which file to export: ";
        std::cin >> choice;
        std::cin.ignore();
    } while (choice <= 0 || choice > fileList.size());
    
    std::string path;
    std::cout << "Enter output path: ";
    std::getline(std::cin, path);
    if (path[path.size() - 1] != '\\')
        path += '\\';

    //read entry
    f.seekg(fileList[choice - 1].second);
    Entry *e = new Entry();
    f.read((char*)e, sizeof(Entry));
    
    //export
    ExportFile(path, e);

    //free memory
    delete e;
}

std::vector<std::pair<std::string, unsigned int>> MyFileSystem::GetFileList(bool getDeleted) {
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
            if (tempEntry.name[0] == -27 && getDeleted)
            {
                //if the file is not restorable
                if (tempEntry.reserved[0] == 0)
                    continue;

                //if the file is restorable
                std::string fileName = tempEntry.GetInfo();
                fileName[0] = tempEntry.reserved[0];
                fileList.push_back(std::make_pair(fileName, bytesOffset));
            }
            //sign of empty entry
            else if (tempEntry.name[0] == 0)
            {
                return fileList;
            }

            if (tempEntry.name[0] != -27 && !getDeleted)
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

void MyFileSystem::ListFiles() 
{
    std::vector<std::pair<std::string, unsigned int>> fileList = MyFileSystem::GetFileList();
    if (fileList.empty())
    {
        std::cout << "There is no file\n";
        return;
    }
    PrintFileList(fileList);
}

void MyFileSystem::MyDeleteFile(unsigned int bytesOffset, bool restorable) 
{
    f.seekg(bytesOffset, f.beg);
    Entry e;
    f.read((char*)&e, sizeof(Entry));

    //check file password
    if (e.hasPassword)
    {
        Entry *pE = &e;
        std::string password;
        if (!CheckFilePassword(pE, password))
        {
            std::cout << "Incorrect password!\n";
            return;
        }
    }

    unsigned char deleteValue = 0xE5;
    unsigned char trueValue = e.name[0];
    //store first byte of name if restorable
    if (restorable)
    {
        f.seekp(bytesOffset + ENTRY_NAME_SIZE + FILE_EXTENSION_LENGTH, f.beg);
        f.write((char*)&trueValue, sizeof(trueValue));
    }
    //mark first byte as E5
    f.seekp(bytesOffset, f.beg);
    f.write((char*)&deleteValue, sizeof(deleteValue));

    //remove from FAT if not restorable
    if(!restorable)
    {
        std::vector<unsigned int> fileClusters = GetClustersChain(e.startingCluster);
        for (unsigned int cluster : fileClusters)
        {
            unsigned int fatOffset = sectorsBeforeFat * bytesPerSector + cluster * fatEntrySize; //in bytes
            f.seekp(fatOffset, f.beg);
            unsigned int empty = 0;
            f.write((char*)&empty, fatEntrySize);
        }
    }
    f.flush();
}

void MyFileSystem::MyDeleteFile()
{
    //list file
    std::vector<std::pair<std::string, unsigned int>> fileList = GetFileList();
    if (fileList.empty())
    {
        std::cout << "There is no file!\n";
        return;
    }
    PrintFileList(fileList);
    std::cout << '\n';

    //choose file
    int choice;
    do
    {
        std::cout << "Enter which file to delete: ";
        std::cin >> choice;
        std::cin.ignore();
    } while (choice <= 0 || choice > fileList.size());

    unsigned int bytesOffset = fileList[choice - 1].second;

    bool restorable;
    std::cout << "Do you want this to be restorable (1 - yes/0 - no): ";
    std::cin >> restorable;
    std::cin.ignore();
    MyDeleteFile(bytesOffset, restorable);
}

void MyFileSystem::RestoreFile(unsigned int bytesOffset) 
{
    f.seekg(bytesOffset, f.beg);
    Entry e;
    Entry *pE = nullptr;
    f.read((char*)&e, sizeof(Entry));
    pE = &e;

    e.name[0] = e.reserved[0];
    e.reserved[0] = 0;

    //check duplicate name
    int number = std::atoi(e.GetIndex().c_str());
    std::string fileName(e.name, e.name + e.nameLen);
    while (CheckDuplicateName(pE))
    {
        number++;
        e.SetName(fileName, number, true);
    }

    //rewrite entry
    f.seekp(bytesOffset, f.beg);
    f.write((char*)&e, sizeof(Entry));
    f.flush();
}

void MyFileSystem::MyRestoreFile() 
{
    std::vector<std::pair<std::string, unsigned int>> fileList = GetFileList(true);
    if (fileList.size() == 0)
    {
        std::cout << "There is no file to restore!\n";
        return;
    }

    std::cout << "Restorable Deleted File List:\n";
    PrintFileList(fileList);
    std::cout << '\n';

    //choose file
    int choice;
    do
    {
        std::cout << "Enter which file to restore: ";
        std::cin >> choice;
        std::cin.ignore();
    } while (choice <= 0 || choice > fileList.size());

    unsigned int bytesOffset = fileList[choice - 1].second;
    RestoreFile(bytesOffset);
}

void MyFileSystem::HandleInput()
{
    char choice;
    while (true)
    {
        std::cout << '\n';
        std::cout << "1. Create/Change File System's password\n";
        std::cout << "2. List files\n";
        std::cout << "3. Create/Change a file's password\n";
        std::cout << "4. Import a file\n";
        std::cout << "5. Export a file\n";
        std::cout << "6. Delete a file\n";
        std::cout << "7. Restore a file\n";
        std::cout << "Q. Quit\n";
        std::cout << "\nEnter your choice: ";
        std::cin >> choice;
        std::cin.ignore();

        switch (choice)
        {
            case '1':
            {
                if (this->hasPassword)
                    ChangeFSPassword();
                else CreateFSPassword();
                break;
            }
            case '2':
            {
                ListFiles();
                break;
            }
            case '3':
            {
                ChangeFilePassword();
                break;
            }
            case '4':
            {
                ImportFile();
                break;
            }
            case '5':
            {
                ExportFile();
                break;
            }
            case '6':
            {
                MyDeleteFile();
                break;
            }
            case '7':
            {
                MyRestoreFile();
                break;
            }
            default:
            {
                return;
            }
        }
    }
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

std::string MyFileSystem::Entry::GetFullName() const
{
    std::string result(name, name + nameLen);
    result += '.' + std::string(extension, extension + FILE_EXTENSION_LENGTH);
    return result;
}

std::string MyFileSystem::Entry::GetIndex() const
{
    int i = ENTRY_NAME_SIZE;
    while (name[i] != '~')
        i--;
    return std::string(name + i + 1, name + ENTRY_NAME_SIZE).c_str();
}

void MyFileSystem::Entry::SetHash(const std::string& hash)
{
    for (int i = 0; i < 32; i++)
    {
        hashedPassword[i] = hash[i];
    }
}

std::string MyFileSystem::Entry::GetInfo() const
{
    double size = fileSize;
    int i = 0;
    std::string result = GetFullName() + "  (" + GetIndex() + ")  ";
    while (size >= 1000)
    {
        size /= 1000;
        i++;
    }

    //https://stackoverflow.com/questions/16605967/set-precision-of-stdto-string-when-converting-floating-point-values
    //answered by hmjd on May 17, 2013 at 09:50
    int precision = 2;
    std::ostringstream out;
    out.precision(precision);
    out << std::fixed << size;
    result += out.str();

    if (i == 0)
        result += " B";
    else if (i == 1)
        result += " KB";
    else result += " MB";
    return result;
}