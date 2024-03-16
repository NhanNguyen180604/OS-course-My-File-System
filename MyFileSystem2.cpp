std::vector<std::pair<std::string, unsigned int>> MyFileSystem::GetFileList() {
    std::vector<std::pair<std::string, unsigned int>> fileList;
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
                return fileList;
            std::string fileName = "";
            //get the last index of the file name
            int finalIndex = 37;
            while (tempEntry[finalIndex] == ' ') {
                finalIndex--;
            }
            for (int i = 0; i <= finalIndex; i++) {
                fileName += tempEntry[i];
            }
            //check duplicate file name;
            if (tempEntry[39] == '1') {
                fileList.push_back(std::make_pair(fileName + '.' + tempEntry[40] + tempEntry[41] + tempEntry[42], bytesOffset));
            }
            else {
                char duplicateIndex = tempEntry[39] - 1;
                fileList.push_back(std::make_pair(fileName + '(' + duplicateIndex + ')' + '.' + tempEntry[40] + tempEntry[41] + tempEntry[42], bytesOffset));
            }
        }
    }
    return fileList;
}

void MyFileSystem::ListFiles() {
    std::vector<std::pair<std::string, unsigned int>> fileList = MyFileSystem::GetFileList();
    int i = 1;
    for (std::pair<std::string, int> p: fileList) {
        std::cout << i++ << ". " << p.first << std::endl;
    }
}


void MyFileSystem::MyDeleteFile(int delFile, bool restorable) {
    std::vector<std::pair<std::string, unsigned int>> fileList = MyFileSystem::GetFileList();
    unsigned int bytesOffset = fileList[delFile - 1].second;
    unsigned char deleteValue = 0xE5;
    unsigned char trueValue;
    f.seekg(bytesOffset, f.beg);
    f.read((char*)&trueValue, sizeof(trueValue));
    f.seekp(bytesOffset, f.beg);
    f.write((char*)&deleteValue, sizeof(deleteValue));
    if (restorable) {
        f.seekp(bytesOffset + 62, f.beg);
        f.write((char*)&trueValue, sizeof(trueValue));
    }
}

void MyFileSystem::MyRestoreFile(std::string restoreName) {
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
