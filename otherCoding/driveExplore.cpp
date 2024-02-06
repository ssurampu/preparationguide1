#include <Windows.h>
#include <iostream>
#include <winioctl.h>
#include <vector>
#include <string>

std::wstring g_InputGUID;
typedef struct _LBA_MAP_STRUCT {
    /** Offset into the slice */
    LONGLONG Offset;
    /** LBA */
    LONGLONG LBA;
} LBA_MAP_STRUCT, * PLBA_MAP_STRUCT;

struct PartitionInfo {
    std::wstring type;
    std::wstring name;
    ULONGLONG size;  // Size in bytes
    ULONGLONG startingLBA;  // Starting LBA from the beginning of the disk
    GUID partitionGUID;
    DWORD partitionNumber;
};

struct CommandLineOptions {
    std::wstring driveNo; // Drive number
    std::wstring newPartitionGuid; // GUID for the new partition
    std::wstring partitionNo; // Partition number
};

// Lambda function to check if a string starts with a prefix
auto startsWith = [](const std::wstring& str, const std::wstring& prefix) {
    return str.compare(0, prefix.size(), prefix) == 0;
    };

void PrintCommandLineOptions(const CommandLineOptions& options) {
    std::wcout << L"Drive Number: " << options.driveNo << std::endl;
    std::wcout << L"New Partition GUID: " << options.newPartitionGuid << std::endl;
    std::wcout << L"Partition Number: " << options.partitionNo << std::endl;
}

// Function to display usage instructions
void DisplayUsage() {
    std::wcout << L"Usage: program.exe [-DriveNo <drive_number>] [-NewPartitionGuid <guid>] [-PartitionNo <partition_number>]" << std::endl;
}

// Function to parse command-line options
CommandLineOptions ParseCommandLineOptions(int argc, wchar_t* argv[]) {
    CommandLineOptions options = {};
    std::vector<std::wstring> args(argv, argv + argc);

    // Iterate through command-line arguments
    for (size_t i = 1; i < args.size(); ++i) { // Start from 1 to skip program name
        if (startsWith(args[i], L"-DriveNo")) {
            if (i + 1 < args.size()) { // Check if next argument is available
                options.driveNo = args[i + 1];
                ++i; // Move to the next argument
            }
            else {
                std::cerr << "Error: Missing value for -DriveNo." << std::endl;
                DisplayUsage();
            }
        }
        else if (startsWith(args[i], L"-NewPartitionGuid")) {
            if (i + 1 < args.size()) {
                options.newPartitionGuid = args[i + 1];
                ++i;
            }
            else {
                std::cerr << "Error: Missing value for -NewPartitionGuid." << std::endl;
                DisplayUsage();
            }
        }
        else if (startsWith(args[i], L"-PartitionNo")) {
            if (i + 1 < args.size()) {
                options.partitionNo = args[i + 1];
                ++i;
            }
            else {
                std::cerr << "Error: Missing value for -PartitionNo." << std::endl;
                DisplayUsage();
            }
        }
    }

    return options;
}

std::vector< LBA_MAP_STRUCT > GetLbaRange(HANDLE fh)
{
    std::vector< LBA_MAP_STRUCT > opVec;
    DWORD retbt, dcRet;
    RETRIEVAL_POINTERS_BUFFER rpBuf;
    STARTING_VCN_INPUT_BUFFER vcnBuffer;
    vcnBuffer.StartingVcn.QuadPart = 0;

    int counter = 0;
    do
    {
        DeviceIoControl(fh, FSCTL_GET_RETRIEVAL_POINTERS, &vcnBuffer, sizeof(STARTING_VCN_INPUT_BUFFER), (void*)&rpBuf, sizeof(rpBuf), &retbt, NULL);

        dcRet = GetLastError();
        if (rpBuf.ExtentCount == 0)
        {
            break;
        }
        LBA_MAP_STRUCT lbaStruct;
        lbaStruct.Offset = vcnBuffer.StartingVcn.QuadPart;
        lbaStruct.LBA = rpBuf.Extents[0].Lcn.QuadPart;
        opVec.push_back(lbaStruct);
        vcnBuffer.StartingVcn = rpBuf.Extents->NextVcn;
        counter++;
    } while (dcRet == ERROR_MORE_DATA);
    return opVec;
}

std::wstring GetPhysicalDiskPath(const std::wstring& driveLetter) {
    // Check if the provided path is a valid drive letter
    if (driveLetter.size() != 2 || driveLetter[1] != L':') {
        std::wcerr << L"Invalid drive letter format. Example: C:" << std::endl;
        return L"";
    }

    // Check if the drive is a fixed drive (e.g., hard disk)
    if (GetDriveType(driveLetter.c_str()) != DRIVE_FIXED) {
        std::wcerr << L"The specified drive is not a fixed drive." << std::endl;
        return L"";
    }

    // Form the device path
    std::wstring devicePath = L"\\\\.\\";
    devicePath += driveLetter;

    // Query the device path to get the physical disk path
    WCHAR targetPath[MAX_PATH];
    std::wcout << "Path to queryDosDevice " << devicePath.c_str() << std::endl;
    DWORD result = QueryDosDeviceW(L"C:", targetPath, MAX_PATH);

    if (result == 0) {
        std::wcerr << L"QueryDosDevice failed with error code " << GetLastError() << std::endl;
        return L"";
    }

    return targetPath;
}

bool GetPartitionInformation(const std::wstring& drivePath) {
    HANDLE hDevice = CreateFile(
        L"c:", //drivePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to open device. Error code: " << GetLastError() << std::endl;
        return false;
    }

    PARTITION_INFORMATION_EX partitionInfo;
    DWORD bytesReturned = 0;

    if (!DeviceIoControl(
        hDevice,
        IOCTL_DISK_GET_PARTITION_INFO_EX,
        nullptr,
        0,
        &partitionInfo,
        sizeof(partitionInfo),
        &bytesReturned,
        nullptr
    )) {
        std::wcerr << L"Failed to get partition information. Error code: " << GetLastError() << std::endl;
        CloseHandle(hDevice);
        return false;
    }

    std::wcout << L"Partition Name: " << drivePath << std::endl;
    std::wcout << L"Partition Size: " << partitionInfo.PartitionLength.QuadPart << L" bytes" << std::endl;
    std::wcout << L"Partition Offset: " << partitionInfo.StartingOffset.QuadPart << L" bytes" << std::endl;

    CloseHandle(hDevice);
    return true;
}

void EnumerateDrives() {
    DWORD drives = GetLogicalDrives();

    for (int driveIndex = 0; driveIndex < 26; ++driveIndex) {
        if ((drives & (1 << driveIndex)) != 0) {
            // Bit is set, indicating a valid drive
            wchar_t driveLetter = static_cast<wchar_t>(L'A' + driveIndex);
            std::wstring drivePath = std::wstring(L"\\\\.\\") + driveLetter + L":";

            // Check if the drive is a valid drive type
            if (GetDriveType(drivePath.c_str()) != DRIVE_UNKNOWN) {
                std::wcout << L"Drive " << driveLetter << L": " << drivePath << std::endl;
            }
        }
    }
}

void EnumeratePhysicalDrives() {
    DWORD drives = GetLogicalDrives();

    for (int driveIndex = 0; driveIndex < 26; ++driveIndex) {
        if ((drives & (1 << driveIndex)) != 0) {
            // Bit is set, indicating a valid drive
            wchar_t driveLetter = static_cast<wchar_t>(L'A' + driveIndex);
            std::wstring drivePath = std::wstring(L"\\\\.\\") + driveLetter + L":";
            HANDLE hDevice = CreateFile(drivePath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (hDevice != INVALID_HANDLE_VALUE) {
                STORAGE_PROPERTY_QUERY query;
                query.PropertyId = StorageDeviceProperty;
                query.QueryType = PropertyStandardQuery;

                STORAGE_DEVICE_DESCRIPTOR deviceDescriptor;
                DWORD bytesReturned;

                if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query),
                    &deviceDescriptor, sizeof(deviceDescriptor), &bytesReturned, nullptr)) {
                    std::wcout << L"Physical Drive " << driveLetter << L": " << drivePath << std::endl;
                    std::wcout << L"Vendor ID: " << deviceDescriptor.VendorIdOffset << std::endl;
                    std::wcout << L"Product ID: " << deviceDescriptor.ProductIdOffset << std::endl;
                    std::wcout << L"Serial Number: " << deviceDescriptor.SerialNumberOffset << std::endl;
                    std::wcout << L"----------------------" << std::endl;
                }
                else {
                    std::wcerr << L"Failed to query storage property. Error code: " << GetLastError() << std::endl;
                }

                CloseHandle(hDevice);
            }
            else {
                std::wcerr << L"Failed to open device. Error code: " << GetLastError() << std::endl;
            }
        }
    }
}


void EnumeratePhysicalDrivesGeomitry() {
    DWORD drives = GetLogicalDrives();

    for (int driveIndex = 0; driveIndex < 26; ++driveIndex) {
        if ((drives & (1 << driveIndex)) != 0) {
            // Bit is set, indicating a valid drive
            wchar_t driveLetter = static_cast<wchar_t>(L'A' + driveIndex);
            std::wstring drivePath = std::wstring(L"\\\\.\\") + driveLetter + L":";

            HANDLE hDevice = CreateFile(drivePath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (hDevice != INVALID_HANDLE_VALUE) {
                DISK_GEOMETRY diskGeometry;
                DWORD bytesReturned;

                if (DeviceIoControl(hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY, nullptr, 0,
                    &diskGeometry, sizeof(diskGeometry), &bytesReturned, nullptr)) {
                    std::wcout << L"Drive " << driveLetter << L": " << drivePath << std::endl;
                    std::wcout << L"Drive Size: " << diskGeometry.Cylinders.QuadPart * diskGeometry.TracksPerCylinder *
                        diskGeometry.SectorsPerTrack * diskGeometry.BytesPerSector
                        << L" bytes" << std::endl;
                    // Add more properties as needed
                    std::wcout << L"----------------------" << std::endl;
                }
                else {
                    std::wcerr << L"Failed to query drive geometry. Error code: " << GetLastError() << std::endl;
                }

                CloseHandle(hDevice);
            }
            else {
                std::wcerr << L"Failed to open device. Error code: " << GetLastError() << std::endl;
            }
        }
    }
}

// Helper function to get the drive letter or mount name associated with a partition
std::wstring GetDriveLetter(const wchar_t* devicePath) {
    wchar_t driveLetter[MAX_PATH];
    if (GetVolumePathNamesForVolumeName(
        devicePath,
        driveLetter,
        MAX_PATH,
        nullptr
    )) {
        return driveLetter;
    }

    return L"";
}

ULONGLONG GetDiskSectorSize(const std::wstring& physicalDrivePath) {
    HANDLE hDevice = CreateFile(
        physicalDrivePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to open device. Error code: " << GetLastError() << std::endl;
        return 0;
    }

    DISK_GEOMETRY diskGeometry;
    DWORD bytesReturned;

    if (DeviceIoControl(
        hDevice,
        IOCTL_DISK_GET_DRIVE_GEOMETRY,
        nullptr,
        0,
        &diskGeometry,
        sizeof(diskGeometry),
        &bytesReturned,
        nullptr
    )) {
        CloseHandle(hDevice);
        return diskGeometry.BytesPerSector;
    }
    else {
        std::wcerr << L"Failed to get disk geometry. Error code: " << GetLastError() << std::endl;
        CloseHandle(hDevice);
        return 0;
    }
}

std::wstring GUIDToString(const GUID& guid) {
    wchar_t buffer[39];  // Assuming a GUID is always 38 characters + null terminator
    StringFromGUID2(guid, buffer, sizeof(buffer) / sizeof(buffer[0]));
    return buffer;
}

std::vector<PartitionInfo> GetPartitionsInfo(const std::wstring& physicalDrivePath) {
    std::vector<PartitionInfo> partitions;

    HANDLE hDevice = CreateFile(
        physicalDrivePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to open device. Error code: " << GetLastError() << std::endl;
        return partitions;
    }

    DWORD bytesReturned = 0;
    /*DRIVE_LAYOUT_INFORMATION_EX driveLayout = {0};
    

    if (!DeviceIoControl(
        hDevice,
        IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
        nullptr,
        0,
        &driveLayout,
        sizeof(driveLayout),
        &bytesReturned,
        nullptr
    )) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            std::wcerr << L"Failed to get drive layout. Error code: " << GetLastError() << std::endl;
            CloseHandle(hDevice);
            return partitions;
        }
    }
    std::vector<BYTE> buffer(bytesReturned);
    if (!DeviceIoControl(
        hDevice,
        IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
        nullptr,
        0,
        buffer.data(),
        static_cast<DWORD>(buffer.size()),
        &bytesReturned,
        nullptr
    )) {
        std::wcerr << L"Failed to get drive layout. Error code: " << GetLastError() << std::endl;
        CloseHandle(hDevice);
        return partitions;
    }*/
    DRIVE_LAYOUT_INFORMATION_EX* driveLayout = (DRIVE_LAYOUT_INFORMATION_EX*)HeapAlloc(GetProcessHeap(), 0, sizeof(DRIVE_LAYOUT_INFORMATION_EX) + 1024);
    if (driveLayout == NULL) {
        std::wcout << "Unable to allocate memory for driveLayout" << std::endl;
        return partitions;
    }
    ZeroMemory(driveLayout, sizeof(DRIVE_LAYOUT_INFORMATION_EX) + 1024);

    if (!DeviceIoControl(
        hDevice,
        IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
        nullptr,
        0,
        driveLayout,
        sizeof(DRIVE_LAYOUT_INFORMATION_EX) + 1024,
        &bytesReturned,
        nullptr
    )) {
        std::wcerr << L"Failed to get drive layout. Error code: " << GetLastError() << std::endl;
        CloseHandle(hDevice);
        return partitions;
    }

    DRIVE_LAYOUT_INFORMATION_EX* pDriveLayout = driveLayout;
    ULONGLONG sectorSize = GetDiskSectorSize(physicalDrivePath);
    std::wcout << "Sector size  " << sectorSize << std::endl;
    for (DWORD partitionNumber = 0; partitionNumber < pDriveLayout->PartitionCount; ++partitionNumber) {
        PARTITION_INFORMATION_EX& partitionInfo = pDriveLayout->PartitionEntry[partitionNumber];

        PartitionInfo info;
        info.type = std::to_wstring(partitionInfo.PartitionStyle);
        if (partitionInfo.PartitionStyle == PARTITION_STYLE_GPT) {
            info.name = partitionInfo.Gpt.Name;
            info.partitionGUID = partitionInfo.Gpt.PartitionId;
        }
        else {
            info.name = L"Partition " + std::to_wstring(partitionNumber + 1);
        }
        
        info.size = partitionInfo.PartitionLength.QuadPart;
        info.partitionNumber = partitionInfo.PartitionNumber;
        
        std::wcout << "partitionInfo.StartingOffset.QuadPart  " << partitionInfo.StartingOffset.QuadPart << std::endl;
        std::wcout << "partitionInfo.StartingOffset.QuadPart with sector  " << partitionInfo.StartingOffset.QuadPart / sectorSize << std::endl;
        // info.startingLBA = partitionInfo.StartingOffset.QuadPart / pDriveLayout->PartitionEntry[0].StartingOffset.QuadPart;
        info.startingLBA = partitionInfo.StartingOffset.QuadPart; // / sectorSize;

        partitions.push_back(info);
    }

    CloseHandle(hDevice);
    return partitions;
}

bool AreGUIDsEqual(const GUID& guid1, const GUID& guid2) {
    return guid1 == guid2;
}

GUID StringToGUID(const std::wstring& guidString) {
    GUID guid;
    //std::wcout << "Input guid: " << guidString << std::endl;
    if (guidString[0] == L'{') {
        if (CLSIDFromString(guidString.c_str(), &guid) != S_OK) {
            // Handle error, for now, we set the GUID to zeros
            ZeroMemory(&guid, sizeof(GUID));
            std::wcout << "Guid not found or invalid." << std::endl;
        }
    }
    else {
        if (CLSIDFromProgID(guidString.c_str(), &guid) != S_OK) {
            // Handle error, for now, we set the GUID to zeros
            ZeroMemory(&guid, sizeof(GUID));
            std::wcout << "Guid not found or invalid." << std::endl;
        }
    }
  
    //std::wcout << "Guid found: " << GUIDToString(guid) << std::endl;
    return guid;
}

HANDLE OpenPhysicalDrive(const std::wstring& physicalDrivePath) {
    HANDLE hDevice = CreateFile(
        physicalDrivePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to open device. Error code: " << GetLastError() << std::endl;
        return nullptr;
    }

    return hDevice;
}

bool WriteDataToDrive(HANDLE hDevice, ULONGLONG startingLBA, DWORD length, const std::wstring& data) {
    // Set the file pointer to the desired starting LBA
    LARGE_INTEGER li;
    li.QuadPart = startingLBA;
    if (SetFilePointerEx(hDevice, li, nullptr, FILE_BEGIN) == 0) {
        std::wcerr << L"Failed to set file pointer. Error code: " << GetLastError() << std::endl;
        return false;
    }

    // Write data to the specified LBA
    DWORD bytesWritten;

    if (WriteFile(hDevice, data.c_str(), length, &bytesWritten, nullptr)) {
        std::wcout << L"Wrote " << bytesWritten << L" bytes to LBA " << startingLBA << std::endl;
        return true;
    }
    else {
        std::wcerr << L"Failed to write data. Error code: " << GetLastError() << std::endl;
        return false;
    }
}

bool ReadDataFromDrive(HANDLE hDevice, ULONGLONG startingLBA, DWORD length) {
    // Set the file pointer to the desired starting LBA
    LARGE_INTEGER li;
    li.QuadPart = startingLBA;

    if (SetFilePointerEx(hDevice, li, nullptr, FILE_BEGIN) == 0) {
        std::wcerr << L"Failed to set file pointer. Error code: " << GetLastError() << std::endl;
        return false;
    }

    // Read data from the specified LBA
    wchar_t buffer[1024];  // Adjust the buffer size as needed
    
    DWORD bytesRead;

    if (ReadFile(hDevice, buffer, length, &bytesRead, nullptr)) {
        std::wcout << L"Read " << bytesRead << L" bytes from LBA " << startingLBA << std::endl;
        std::wcout << L"Data: " << buffer << std::endl;
        return true;
    }
    else {
        std::wcerr << L"Failed to read data. Error code: " << GetLastError() << std::endl;
        return false;
    }
}

void DoIOToPhysicalDrive(std::wstring physicalDrivePath, ULONGLONG startingLBA, DWORD length)
{
    HANDLE hDevice = OpenPhysicalDrive(physicalDrivePath);

    if (hDevice == nullptr) {
        std::wcerr << L"Failed to open physical drive." << std::endl;
        return;
    }

    //std::wstring dataToWrite = L"The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog";
    std::wstring dataToWrite(1024, L's');
    if (WriteDataToDrive(hDevice, startingLBA, length, dataToWrite)) {
        // Read the same data back
        ReadDataFromDrive(hDevice, startingLBA, length);
    }

    CloseHandle(hDevice);
}
int GetPartitionInfoFromPhysicalPath(std::wstring physicalDrivePath, BOOLEAN readWrite)
{
    //std::wstring physicalDrivePath = L"\\\\.\\PHYSICALDRIVE0";
    std::vector<PartitionInfo> partitions = GetPartitionsInfo(physicalDrivePath);
    std::wcout << " ===== Information about physicalDrivePath:: " << physicalDrivePath << " =====" << std::endl;
    for (const auto& partition : partitions) {
        std::wcout << L"Partition Type: " << partition.type << std::endl;
        std::wcout << L"Partition Name: " << partition.name << std::endl;
        std::wcout << L"Partition Size: " << partition.size << L" bytes" << std::endl;
        std::wcout << L"Starting LBA: " << partition.startingLBA << std::endl;
        std::wcout << L"Parition Number: " << partition.partitionNumber << std::endl;
        std::wcout << L"Partition GUID: " << GUIDToString(partition.partitionGUID) << std::endl;
        std::wcout << L"----------------------" << std::endl;
        if (readWrite) {
            //std::wstring guidString = L"{5E081143-EB0C-4D1B-BC14-D431A223A30F}";
            GUID guid1;
            if (!g_InputGUID.empty()) {
                guid1 = StringToGUID(g_InputGUID);
                std::wcout << "Checking input GUID have a matching partition" << std::endl;
                if (AreGUIDsEqual(guid1, partition.partitionGUID) == true) {
                    std::wcout << L"Found matching Guid. Trying a read/write operation" << std::endl;
                    DoIOToPhysicalDrive(physicalDrivePath, partition.startingLBA, 1024);
                }
            }
        }
       
    }

    return 0;

}

std::vector<std::wstring> GetAllPhysicalDrivePaths() {
    std::vector<std::wstring> physicalDrivePaths;

    for (int driveNumber = 0; ; ++driveNumber) {
        std::wstring physicalDrivePath = L"\\\\.\\PHYSICALDRIVE" + std::to_wstring(driveNumber);

        HANDLE hDevice = CreateFile(
            physicalDrivePath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (hDevice == INVALID_HANDLE_VALUE) {
            // Invalid handle, no more drives
            break;
        }

        // Valid physical drive path, add it to the list
        physicalDrivePaths.push_back(physicalDrivePath);
        CloseHandle(hDevice);
    }

    return physicalDrivePaths;
}

std::wstring GetSystemDrivePath() {
    wchar_t systemDrive[MAX_PATH] = { 0 };
    if (GetSystemDirectory(systemDrive, MAX_PATH) == 0) {
        std::cerr << "Error: Unable to retrieve system directory. Error code: " << GetLastError() << std::endl;
        return L"";
    }

    std::wstring systemDrivePath = systemDrive;
    systemDrivePath.erase(2); // Remove the trailing backslash

    return L"\\\\.\\" + systemDrivePath;
}


int wmain(int argc, wchar_t* argv[]) {
    CommandLineOptions cops = ParseCommandLineOptions(argc, argv);
    PrintCommandLineOptions(cops);
    if (cops.driveNo.empty()) {
        return 0;
    }
    /*std::wstring systemDrivePath = GetSystemDrivePath();
    if (systemDrivePath.c_str() != L"") {
        std::wcout << "System drive path : " << systemDrivePath << std::endl;
        return 0;
    }*/

    
   /* std::wstring physicalDriveName;
    if (argc <= 1) {
        std::wcout << "privde the physical drive number. Example 1 and/or guid as optional" << std::endl;
        return -1;
    }
    if (argc == 2) {
        physicalDriveName = argv[1];
    }
    
   if(argc >= 3){
        g_InputGUID = argv[2];
        std::wcout << "Received GUID for read/write " << g_InputGUID << std::endl;
    }*/
    if (cops.driveNo.empty()) {
        DisplayUsage();
        return -1;
    }
    if (!cops.newPartitionGuid.empty()) {
        g_InputGUID = cops.newPartitionGuid;
        std::wcout << "Input Guid  " << g_InputGUID << std::endl;
    }

    std::wstring physicalDrivePath = L"\\\\.\\PHYSICALDRIVE" + cops.driveNo;
    std::wcout << "Input physical drive " << argv[1] << " Physical drive path  " << physicalDrivePath << std::endl;
    GetPartitionInfoFromPhysicalPath(physicalDrivePath, false);


   /* std::vector<std::wstring> physicalDrivePaths = GetAllPhysicalDrivePaths();
    for (const auto& physicalDrivePath : physicalDrivePaths) {
        GetPartitionInfoFromPhysicalPath(physicalDrivePath, false);
    }*/
    
    //EnumeratePhysicalDrivesGeomitry();
    //EnumeratePhysicalDrives();
    //EnumerateDrives();
    // Example usage
    /*std::wstring driveLetter = L"C:";
    std::wstring physicalDiskPath = GetPhysicalDiskPath(driveLetter);

    if (physicalDiskPath.empty()) {
        std::wcout << L"Unable to get the physical disk details "  << std::endl;
        return 1;
    }
    std::wcout << L"Physical Disk Path for " << driveLetter << L": " << physicalDiskPath << std::endl;

    if (GetPartitionInformation(physicalDiskPath)) {
        std::wcout << L"Successfully retrieved partition information." << std::endl;
    }*/
    return 0;
}
