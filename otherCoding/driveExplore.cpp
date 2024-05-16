
#include <Windows.h>
#include <iostream>
#include <winioctl.h>
#include <vector>
#include <string>
#include <nvme.h>
#include <map>
#include <algorithm>
#include <rpc.h>
#include <limits>

#define min(x,y) ((x) > (y) ? (x) : (y))
#define min(x,y) ((x) < (y) ? (x) : (y))


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
    std::wstring guid; // GUID
    std::wstring partitionNo; // Partition number
    std::wstring readWrite;
    std::wstring filePath;
    std::wstring listDrives;
    std::wstring listAllPartitions;
    std::wstring listPartitions;
    std::wstring drivesGeometry;
    std::wstring initDrive;
    std::wstring createPartition;
    std::wstring createPartitionWithGuid;
    std::wstring size;
};

// Lambda function to check if a string starts with a prefix
//auto startsWith = [](const std::wstring& str, const std::wstring& prefix) {
//    return str.compare(0, prefix.size(), prefix) == 0;
//    };

auto startsWith = [](const std::wstring& str, const std::wstring& prefix) {
    // Convert strings to lowercase for case-insensitive comparison
    std::wstring strLower(str);
    std::wstring prefixLower(prefix);

    
    for (wchar_t& ch : strLower) {
        ch = std::tolower(ch);
    }
    for (wchar_t& ch : prefixLower) {
        ch = std::tolower(ch);
    }

    return strLower.compare(0, prefixLower.size(), prefixLower) == 0;
    };
void PrintCommandLineOptions(const CommandLineOptions& options) {
    std::wcout << L"driveNo: " << options.driveNo << std::endl;
    std::wcout << L"guid: " << options.guid << std::endl;
    std::wcout << L"partitionNo: " << options.partitionNo << std::endl;
    std::wcout << L"readWrite: " << options.readWrite << std::endl;
    std::wcout << L"filePath: " << options.filePath << std::endl;

    std::wcout << L"listDrives: " << options.listDrives << std::endl;
    std::wcout << L"listAllPartitions " << options.listAllPartitions << std::endl;
    std::wcout << L"listPartitions " << options.listPartitions << std::endl;
    std::wcout << L"drivesGeometry " << options.drivesGeometry << std::endl;
    std::wcout << L"initDrive " << options.initDrive << std::endl;
    std::wcout << L"createPartition " << options.createPartition << std::endl;
    std::wcout << L"createPartitionWithGuid " << options.createPartitionWithGuid << std::endl;
    std::wcout << L"size " << options.size << std::endl;
}

// Function to display usage instructions
void DisplayUsage() {
    std::wcout << L"Usage: driveExplore.exe [-DriveNo <drive_number>] " << std::endl
                << L" [-guid <guid>]" << std::endl
                << L" [-PartitionNo <partition_number>]" << std::endl
                << L" [-readwrite <0 / 1>]" << std::endl
                << L" [-filePath <file path> ]" << std::endl
                << L" [-listDrives]" << std::endl
                << L" [-listAllPartitions]" << std::endl
                << L" [-listPartitions]" << std::endl
                << L" [-drivesGeometry]" << std::endl
                << L" [-initDrive]" << std::endl
                << L" [-createPartition] [-size <size>] [-driveNo <drive no>]" << std::endl
                << L" [-createPartitionWithGuid] [-guid <guid>] [-size] [-driveNo <drive no>]" << std::endl
                << L" [-size]" << std::endl
                << std::endl;
}

// Function to parse command-line options
bool ParseCommandLineOptions(int argc, wchar_t* argv[], CommandLineOptions &options) {
    
    options.readWrite = L"0";
    options.listDrives = L"0";
    options.listAllPartitions = L"0";
    options.listPartitions = L"0";
    options.drivesGeometry = L"0";
    options.initDrive = L"0";
    options.createPartition = L"0";
    options.size = L"-1";
    options.driveNo = L"-1";
    options.guid = L"-1";
    options.createPartitionWithGuid = L"0";

    std::vector<std::wstring> args(argv, argv + argc);
    std::cerr << "args count" << argc << std::endl;

    if (argc == 1 || startsWith(args[0], L"-help")) {
        DisplayUsage();
        return false;
    }

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
                return false;
            }
        }
        else if (startsWith(args[i], L"-guid")) {
            if (i + 1 < args.size()) {
                options.guid = args[i + 1];
                ++i;
            }
            else {
                std::cerr << "Error: Missing GUID for -guid." << std::endl;
                DisplayUsage();
                return false;
            }
        }
        else if (startsWith(args[i], L"-PartitionNo")) {
            if (i + 1 < args.size()) {
                options.partitionNo = args[i + 1];
                ++i;
            }
            else {
                std::cerr << "Error: Missing partition number for -PartitionNo." << std::endl;
                DisplayUsage();
            }
        }
        else if (startsWith(args[i], L"-readwrite")) {
            if (i + 1 < args.size()) {
                options.readWrite = args[i + 1];
                ++i;
            }
            else {
                std::cerr << "Error: Missing value <0 read / 1 read,write> for -readwrite." << std::endl;
                DisplayUsage();
                return false;
            }
        }
        else if (startsWith(args[i], L"-filePath")) {
            if (i + 1 < args.size()) {
                options.filePath = args[i + 1];
                ++i;
            }
            else {
                std::cerr << "Error: Missing file path for -filePath." << std::endl;
                DisplayUsage();
                return false;
            }
        }
        else if (startsWith(args[i], L"-createPartitionWithGuid")) {
            options.createPartitionWithGuid = L"1";
        }
        else if (startsWith(args[i], L"-size")) {
            if (i + 1 < args.size()) {
                options.size = args[i + 1];
                ++i;
            }
            else {
                std::cerr << "Error: Missing size value -size ." << std::endl;
                DisplayUsage();
                return false;
            }
        }
        else if (startsWith(args[i], L"-listPartitions")) {
            options.listPartitions = L"1";
        }
        else if (startsWith(args[i], L"-listDrives")) {
            options.listDrives = L"1";
        }
        else if (startsWith(args[i], L"-listAllPartitions")) {
            options.listAllPartitions = L"1";
        }
        else if (startsWith(args[i], L"-drivesGeometry")) {
            options.drivesGeometry = L"1";
        }
        else if (startsWith(args[i], L"-initDrive")) {
            options.initDrive = L"1";
        }
        else if (startsWith(args[i], L"-createPartition")) {
            options.createPartition = L"1";
        }
        else if (startsWith(args[i], L"-help")) {
            DisplayUsage();
            exit(0);
        }
    }

    if (options.listPartitions == L"1" && options.driveNo == L"-1") {
        std::cerr << "Error: Missing -driveNo for  -listPartitions." << std::endl;
        DisplayUsage();
        return false;
    }

    if (options.initDrive == L"1" && options.driveNo == L"-1") {
        std::cerr << "Error: Missing -driveNo for  -initDrive." << std::endl;
        DisplayUsage();
        return false;
    }

    if (options.createPartitionWithGuid == L"1") {
        if (options.guid == L"-1" || options.size == L"-1" || options.driveNo == L"-1") {
            std::cerr << "Error: Size should be >= 0 for -size and proper drive number -driveNo should be given. " << std::endl;
            DisplayUsage();
            return false;
        }
    }

    if (options.createPartition == L"1") {
        if (options.size == L"-1" || options.driveNo == L"-1") {
            std::cerr << "Error: Size should be >= 0 for -size and proper drive number -driveNo should be given. " << std::endl;
            DisplayUsage();
            return false;
        }
    }

    return true;
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
        std::wcout << "Fount lba mapping " << std::endl;
        lbaStruct.Offset = vcnBuffer.StartingVcn.QuadPart;
        lbaStruct.LBA = rpBuf.Extents[0].Lcn.QuadPart;
        opVec.push_back(lbaStruct);
        vcnBuffer.StartingVcn = rpBuf.Extents->NextVcn;
        counter++;
    } while (dcRet == ERROR_MORE_DATA);
    std::wcout << " Returing lba mapping count " << opVec.size() << std::endl;
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
    if (drives == 0) {
        return;
    }
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

bool AcquireAdminPrivileges() {
    // Create a shield icon info structure
    SHELLEXECUTEINFOW shellInfo = { sizeof(SHELLEXECUTEINFOW) };
    shellInfo.lpVerb = L"runas"; // Request elevation
    shellInfo.lpFile = L"cmd.exe"; // You can replace with your application executable
    shellInfo.lpParameters = L"/c echo Elevated privileges acquired"; // Command to execute

    // Show UAC prompt to request elevation
    if (!ShellExecuteExW(&shellInfo)) {
        DWORD errorCode = GetLastError();
        if (errorCode == ERROR_CANCELLED) {
            std::cerr << "User denied elevation request." << std::endl;
        }
        else {
            std::cerr << "Failed to acquire administrative privileges. Error code: " << errorCode << std::endl;
        }
        return false;
    }

    return true;
}

BOOLEAN SetPartitionGUID(const std::wstring& physicalDrivePath, DWORD newpartitionNumber, const GUID& partitionGuid) {
    if (AcquireAdminPrivileges()) {
        std::cout << "Administrative privileges acquired successfully." << std::endl;
    }
    else {
        std::cerr << "Failed to acquire administrative privileges." << std::endl;
    }
    std::vector<PartitionInfo> partitions;
    std::wcout << "setting new guid  " << GUIDToString(partitionGuid) << std::endl;
    HANDLE hDevice = CreateFileW(
        physicalDrivePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to open device. Error code: " << GetLastError() << std::endl;
        return false;
    }

    DWORD bytesReturned = 0;
   
    DRIVE_LAYOUT_INFORMATION_EX* driveLayout = (DRIVE_LAYOUT_INFORMATION_EX*)HeapAlloc(GetProcessHeap(), 0, sizeof(DRIVE_LAYOUT_INFORMATION_EX) + 1024);
    if (driveLayout == NULL) {
        std::wcout << "Unable to allocate memory for driveLayout" << std::endl;
        return false;
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
        return false;
    }

    DRIVE_LAYOUT_INFORMATION_EX* pDriveLayout = driveLayout;

    for (DWORD partitionNumber = 0; partitionNumber < pDriveLayout->PartitionCount; ++partitionNumber) {
        PARTITION_INFORMATION_EX& partitionInfo = pDriveLayout->PartitionEntry[partitionNumber];

        if (partitionInfo.PartitionNumber == newpartitionNumber) {
            std::cout << "Found a matching partition number " << partitionInfo.PartitionNumber << std::endl;
            partitionInfo.Gpt.PartitionId = partitionGuid;
            if (!DeviceIoControl(
                hDevice,
                IOCTL_DISK_SET_PARTITION_INFO_EX,
                &partitionInfo,
                sizeof(partitionInfo),
                NULL,
                0,
                &bytesReturned,
                NULL
            )) {
                std::cout << "Failed to set partition GUID. Error : " << GetLastError() << std::endl;
                CloseHandle(hDevice);
                return false;
            }
            std::cout << "Successfully set partition GUID." << std::endl;
            CloseHandle(hDevice);
            return true;
        }
    }
    std::cout << "Could not Found a matching partition number " << newpartitionNumber << std::endl;
    CloseHandle(hDevice);
    return false;
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

HRESULT GetDiskPartitions(const std::wstring& physicalDrivePath, PDRIVE_LAYOUT_INFORMATION_EX *layoutInfo) 
{
    HRESULT hr = S_OK;

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
        return HRESULT_FROM_WIN32(GetLastError());
    }

    DWORD bytesReturned = 0;
   
    PDRIVE_LAYOUT_INFORMATION_EX partitionLayout = nullptr;
    partitionLayout = (DRIVE_LAYOUT_INFORMATION_EX*)HeapAlloc(GetProcessHeap(), 0, sizeof(DRIVE_LAYOUT_INFORMATION_EX) + 1024);
    if (partitionLayout == nullptr) {
        std::wcout << "Unable to allocate memory for partitionLayout" << std::endl;
        return HRESULT_FROM_WIN32(GetLastError());
    }
    ZeroMemory(partitionLayout, sizeof(DRIVE_LAYOUT_INFORMATION_EX) + 1024);

    if (!DeviceIoControl(
        hDevice,
        IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
        nullptr,
        0,
        partitionLayout,
        sizeof(DRIVE_LAYOUT_INFORMATION_EX) + 1024,
        &bytesReturned,
        nullptr
    )) {
        std::wcerr << L"Failed to get drive layout. Error code: " << GetLastError() << std::endl;
        CloseHandle(hDevice);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    CloseHandle(hDevice);
    *layoutInfo = partitionLayout;
    return hr;
}


HRESULT GetPartitionsInfo1(const std::wstring& physicalDrivePath, std::vector<PartitionInfo>& partitionsList) 
{
    partitionsList.clear();
    std::wcout << "In GetPartitionsInfo1 " << std::endl;
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
        return HRESULT_FROM_WIN32(GetLastError());
    }
    std::wcout << "successfully opened physical Drive path " << physicalDrivePath << std::endl;

    DWORD bytesReturned = 0;
    SIZE_T driveLayoutSize = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + 1024;

    DRIVE_LAYOUT_INFORMATION_EX* driveLayout = (DRIVE_LAYOUT_INFORMATION_EX*)HeapAlloc(GetProcessHeap(), 0, driveLayoutSize);
    if (driveLayout == NULL) {
        std::wcout << "Unable to allocate memory for driveLayout" << std::endl;
        return E_OUTOFMEMORY;
    }
    ZeroMemory(driveLayout, driveLayoutSize);

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
        return HRESULT_FROM_WIN32(GetLastError());
    }
    std::wcout << "successfully got DRIVE_LAYOUT_INFORMATION_EX byes returned : " << bytesReturned << std::endl;

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
        info.startingLBA = partitionInfo.StartingOffset.QuadPart;

        partitionsList.push_back(info);
    }

    CloseHandle(hDevice);
    return S_OK;
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
        FILE_FLAG_NO_BUFFERING,
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
    std::wcout << "Writing data to the offset " << startingLBA << std::endl;
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
    DWORD bytesRead = 0;
    std::wcout << "Reading data to the offset " << startingLBA << std::endl;
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
    std::wstring dataToWrite(1024, L'T');
    if (WriteDataToDrive(hDevice, startingLBA, length, dataToWrite)) {
        // Read the same data back
        ReadDataFromDrive(hDevice, startingLBA, length);
    }

    CloseHandle(hDevice);
}
int GetPartitionInfoFromPhysicalPath(std::wstring physicalDrivePath, int readWrite)
{
    std::vector<PartitionInfo> partitions;
    HRESULT hr = GetPartitionsInfo1(physicalDrivePath, partitions);
    if (hr != S_OK) {
        std::wcout << "Unable to fetch partition infor for the physical drive : " << physicalDrivePath << std::endl;
        return -1;
    }
    std::wcout << " ===== Information about physicalDrivePath:: " << physicalDrivePath << " =====" << std::endl;
    for (const auto& partition : partitions) {
        std::wcout << L"Partition Type: " << partition.type << std::endl;
        std::wcout << L"Partition Name: " << partition.name << std::endl;
        std::wcout << L"Partition Size: " << partition.size << L" bytes" << std::endl;
        std::wcout << L"Starting LBA: " << partition.startingLBA << std::endl;
        std::wcout << L"Parition Number: " << partition.partitionNumber << std::endl;
        std::wcout << L"Partition GUID: " << GUIDToString(partition.partitionGUID) << std::endl;
        std::wcout << L"----------------------" << std::endl;
        std::wstring guidString = L"{05FCBDDC-2FC8-4993-96EA-F101A3882A70}";
        GUID guid1 = StringToGUID(guidString);
        if (IsEqualGUID(guid1, partition.partitionGUID)) {
            std::wcout << "Found equal guid : " << guidString << std::endl;
        }
        if (readWrite == 1) {
            g_InputGUID = guidString;
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

HRESULT GetAllPhysicalDrivePaths1(std::vector<std::wstring>& physicalDrivePaths) {
    physicalDrivePaths.clear(); // Clear the vector before populating

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
            // Check if there are no more drives or if an error occurred
            if (GetLastError() == ERROR_FILE_NOT_FOUND) {
                // No more drives
                break;
            }
            else {
                // Error occurred, return failure
                return HRESULT_FROM_WIN32(GetLastError());
            }
        }

        // Valid physical drive path, add it to the list
        physicalDrivePaths.push_back(physicalDrivePath);
        CloseHandle(hDevice);
    }

    std::wcout << " Returning from GetAllPhysicalDrivePaths1 " << std::endl;
    return S_OK; // Unexpected error
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

HRESULT GetNvmeGuidForDrive(LPCWSTR DevicePath, GUID* retGuid)
{
    HANDLE diskHandle = INVALID_HANDLE_VALUE;
    HRESULT hr = S_OK;
    PVOID buffer;
    BOOL result = false;
    PSTORAGE_PROPERTY_QUERY query = nullptr;
    PSTORAGE_PROTOCOL_SPECIFIC_DATA protocolData = nullptr;
    PSTORAGE_PROTOCOL_DATA_DESCRIPTOR protocolDataDescr = nullptr;

    ULONG bufferLength = FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters) +
        sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA) + 0x1000;


    buffer = (PVOID)HeapAlloc(GetProcessHeap(), 0, bufferLength);
    if (buffer == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit;
    }
    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;
    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = StorageAdapterProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = ProtocolTypeNvme;
    protocolData->DataType = NVMeDataTypeIdentify;
    protocolData->ProtocolDataRequestValue = 0x0;
    protocolData->ProtocolDataRequestSubValue = 1;
    protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    protocolData->ProtocolDataLength = 0x1000;

    diskHandle = CreateFile(
        DevicePath,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (diskHandle == INVALID_HANDLE_VALUE)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        std::wcout << "Could not open disk with error " << GetLastError() << std::endl;
        goto Exit;
    }
    DWORD returnedLength;
    result = DeviceIoControl(diskHandle,
        IOCTL_STORAGE_QUERY_PROPERTY,
        query,
        bufferLength,
        query,
        bufferLength,
        &returnedLength,
        NULL);

    if (result == FALSE)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        std::wcout << "DeviceIoControl failed with error " << GetLastError() << std::endl;
        goto Exit;
    }


    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)))
    {
        goto Exit;
    }

    protocolData = &protocolDataDescr->ProtocolSpecificData;

    if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) ||
        (protocolData->ProtocolDataLength < 0x1000))
    {
        goto Exit;
    }

    {
        const NVME_IDENTIFY_NAMESPACE_DATA* identifyNamespaceData = (PNVME_IDENTIFY_NAMESPACE_DATA)((PCHAR)protocolData + protocolData->ProtocolDataOffset);
        retGuid->Data1 = (uint32_t)identifyNamespaceData->NGUID[0] << 24 |
            (uint32_t)identifyNamespaceData->NGUID[1] << 16 |
            (uint32_t)identifyNamespaceData->NGUID[2] << 8 |
            (uint32_t)identifyNamespaceData->NGUID[3];
        retGuid->Data2 = (uint16_t)identifyNamespaceData->NGUID[4] << 8 |
            (uint16_t)identifyNamespaceData->NGUID[5];
        retGuid->Data3 = (uint16_t)identifyNamespaceData->NGUID[6] << 8 |
            (uint16_t)identifyNamespaceData->NGUID[7];
        memcpy(retGuid->Data4, identifyNamespaceData->NGUID + 8, 8);
    }

Exit:
    if (buffer != NULL)
    {
        HeapFree(GetProcessHeap(), 0, buffer);
    }
    if (diskHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(diskHandle);
    }
    std::wcout << "Returning guid" << GUIDToString(*retGuid) << std::endl;
    return hr;
}

HANDLE OpenNtfsFile(std::wstring& FilePath)
{
    HANDLE fh = CreateFile(
        FilePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,    // desired access (read-write)
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,                            // security attributes (none)
        OPEN_EXISTING,                   // creation disposition
        FILE_FLAG_NO_BUFFERING,           // flags and attributes
        NULL);
    if (fh == INVALID_HANDLE_VALUE) {
        std::wcout <<"Got error while opening file " << FilePath << " Error : " << GetLastError() << std::endl;
    }
    return fh;
}

VOID doFileOperations(std::wstring filePath)
{
    HANDLE handle = OpenNtfsFile(filePath);
    if(handle == INVALID_HANDLE_VALUE) {
        std::wcout << "Unable to open file : " << filePath << std::endl;
        return;
        
    }
    std::wcout << "Successfully opened file " << filePath << std::endl;
    std::vector<LBA_MAP_STRUCT> lbaMap = GetLbaRange(handle);
    CloseHandle(handle);
    ULONGLONG partitionOffset = 6308233216;
    //ULONGLONG partitionOffset = 2638217216;
    HANDLE h = OpenPhysicalDrive(L"\\\\.\\PHYSICALDRIVE1");
    if (h == INVALID_HANDLE_VALUE) {
        std::wcout << "Unable to opend the drive " << std::endl;
        return;
    }
    for (const auto& lba : lbaMap) {
        std::wcout << "lba " << lba.LBA << std::endl;
        std::wcout << "offset " << lba.Offset << std::endl;
        std::wstring dataToWrite(1024, L's');
        ULONGLONG offset = partitionOffset + lba.LBA * 4096;
        WriteDataToDrive(h, offset, 1024, dataToWrite);
        ReadDataFromDrive(h, offset, 1024);
    }
    CloseHandle(h);
}


HRESULT
FlushDisk(
    _In_ HANDLE deviceHandle
)
{
    HRESULT hr = S_OK;
    ULONG bytesReturned;
    BOOL success;

    success = DeviceIoControl(
        deviceHandle,
        IOCTL_DISK_UPDATE_PROPERTIES,
        NULL,
        0,
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    if (!success) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        std::wcout << "Failed to flush disk " << GetLastError() << std::endl;
        goto Exit;
    }

Exit:
    return hr;
}

HRESULT
ClearDiskPartitions(HANDLE deviceHandle)
{
    HRESULT hr = S_OK;
    ULONG bytesReturned;
    BOOL success;

    // Delete all partitions
    success = DeviceIoControl(
        deviceHandle,
        IOCTL_DISK_DELETE_DRIVE_LAYOUT,
        NULL,
        0,
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    if (!success) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        std::wcout << "Failed to delete disk partitions " << GetLastError() << std::endl;
        goto Exit;
    }

    hr = FlushDisk(deviceHandle);
    if (FAILED(hr)) {
        std::wcout << "Failed to flush disk after deleting disk partitions " << GetLastError() << std::endl;
        goto Exit;
    }

Exit:
    return hr;
}

HRESULT GetANewGuid(GUID &newGuid)
{
    HRESULT hr = S_OK;
    hr = CoCreateGuid(&newGuid);
    if (FAILED(hr)) {
        std::wcout << "Failed to create new  GUID " << GetLastError() << std::endl;
    }
    return hr;
}

HRESULT CreateGPTDisk(HANDLE deviceHandle)
{
    HRESULT hr = S_OK;
    BOOL success;
    CREATE_DISK createDisk;
    createDisk.PartitionStyle = PARTITION_STYLE_GPT;
    createDisk.Gpt.MaxPartitionCount = 0;
    hr = CoCreateGuid(&createDisk.Gpt.DiskId);
    if (FAILED(hr)) {
        std::wcout << "Failed to create new GPT disk GUID " << GetLastError() << std::endl;
        return hr;
    }

    DWORD bytesReturned = 0;
    success = DeviceIoControl(
        deviceHandle,
        IOCTL_DISK_CREATE_DISK,
        &createDisk,
        sizeof(createDisk),
        NULL,
        0,
        &bytesReturned,
        nullptr
    );
    if (!success) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        std::wcout << "Failed to create new GPT disk partition " << GetLastError() << std::endl;
        return hr;
    }

    hr = FlushDisk(deviceHandle);
    if (FAILED(hr)) {
        std::wcout << "Failed to flush after creating new disk partition " << hr << std::endl;
        return hr;
    }
    
}

bool CreateGptPartition(
    const std::wstring& drivePath, 
    LONGLONG newpartitionSize, 
    const GUID& partitionId, 
    std::wstring partitionName,
    DWORD64 attributes,
    const GUID& partitionType)
{
    HRESULT hr = S_OK;
    PDRIVE_LAYOUT_INFORMATION_EX partitionLayout = nullptr;
    PDRIVE_LAYOUT_INFORMATION_EX newPartitionLayout = nullptr;

    HANDLE hDevice = CreateFileW(
        drivePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to open device. Error code: " << GetLastError() << std::endl;
        return false;
    }


    hr = GetDiskPartitions(drivePath, &partitionLayout);
    if (hr != S_OK) {
        std::wcout << " Unable to get existing partition details for the drive : " << drivePath << std::endl;
        return false;
    }
 
    ULONG partitionLayoutSize =
        sizeof(DRIVE_LAYOUT_INFORMATION_EX) -
        sizeof(partitionLayout->PartitionEntry) +
        partitionLayout->PartitionCount * sizeof(PARTITION_INFORMATION_EX);

    ULONG newPartitionIndex = partitionLayout->PartitionCount;
    ULONG newPartitionCount = newPartitionIndex + 1;
    ULONG newPartitionLayoutSize = partitionLayoutSize + sizeof(PARTITION_INFORMATION_EX);
    newPartitionLayout = (PDRIVE_LAYOUT_INFORMATION_EX)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        newPartitionLayoutSize
    );
    if (newPartitionLayout == nullptr) {
        hr = E_OUTOFMEMORY;
        std::wcout << "Unable to allocate memory for new partition layout: " << GetLastError() << std::endl;
        return false;
    }


    ULONG64 newPartitionOffset = partitionLayout->Gpt.StartingUsableOffset.QuadPart;
    ULONG newPartitionNumber = 0;
    for (ULONG i = 0; i < partitionLayout->PartitionCount; i++) {
        ULONG64 offset =
            partitionLayout->PartitionEntry[i].StartingOffset.QuadPart +
            partitionLayout->PartitionEntry[i].PartitionLength.QuadPart;

        newPartitionOffset = max(newPartitionOffset, offset);
        newPartitionNumber = max(newPartitionNumber, partitionLayout->PartitionEntry[i].PartitionNumber);
    }

    newPartitionNumber++;

    // Make sure the partition table is not full.
    if (newPartitionOffset >= (ULONG64)partitionLayout->Gpt.UsableLength.QuadPart) {
        hr = ERROR_DISK_FULL;
        std::wcout << "Disk size is full " << std::endl;
        return false;
    }

    ULONG64 maxPartitionSize = (ULONG64)partitionLayout->Gpt.UsableLength.QuadPart - newPartitionOffset;
    newpartitionSize = min(newpartitionSize, maxPartitionSize);

    memcpy(newPartitionLayout, partitionLayout, partitionLayoutSize);
    newPartitionLayout->PartitionCount = newPartitionCount;

    newPartitionLayout->PartitionEntry[newPartitionIndex].PartitionLength.QuadPart = newpartitionSize;
    newPartitionLayout->PartitionEntry[newPartitionIndex].PartitionNumber = newPartitionNumber;
    newPartitionLayout->PartitionEntry[newPartitionIndex].StartingOffset.QuadPart = newPartitionOffset;
    newPartitionLayout->PartitionEntry[newPartitionIndex].PartitionStyle = PARTITION_STYLE_GPT;
    newPartitionLayout->PartitionEntry[newPartitionIndex].RewritePartition = 1;
    newPartitionLayout->PartitionEntry[newPartitionIndex].Gpt.PartitionId  = partitionId;
    wcscpy_s(newPartitionLayout->PartitionEntry[newPartitionIndex].Gpt.Name, partitionName.c_str());

    newPartitionLayout->PartitionEntry[newPartitionIndex].Gpt.Attributes = attributes;
    
    
    memcpy(&(newPartitionLayout->PartitionEntry[newPartitionIndex].Gpt.PartitionType), 
        &partitionType, sizeof(newPartitionLayout->PartitionEntry[newPartitionIndex].Gpt.PartitionType));

    //newPartitionLayout->PartitionEntry[newPartitionIndex].Gpt.PartitionType = partitionType;

    DWORD bytesReturned = 0;
    if (!DeviceIoControl(
        hDevice,
        IOCTL_DISK_SET_DRIVE_LAYOUT_EX,
        newPartitionLayout,
        newPartitionLayoutSize,
        NULL,
        0,
        &bytesReturned,
        NULL
    )) {
        std::cerr << "Unable to flush the disk. Error code: " << GetLastError() << std::endl;
        CloseHandle(hDevice);
        return false;
    }

    hr = FlushDisk(hDevice);
    if (hr != S_OK) {
        std::wcout << " Unable to get existing partition details for the drive : " << drivePath << std::endl;
        return false;
    }


    CloseHandle(hDevice);
    if (partitionLayout) {
        HeapFree(GetProcessHeap(), 0, partitionLayout);
    }

    if (newPartitionLayout) {
        HeapFree(GetProcessHeap(), 0, newPartitionLayout);
    }
    return true;
}

HRESULT InitDrive(const std::wstring& drivePath)
{
    HRESULT hr = S_OK;
    HANDLE hDevice = CreateFileW(
        drivePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to open device. Error code: " << GetLastError() << std::endl;
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit;
        
    }

    hr = ClearDiskPartitions(hDevice);
    if (hr != S_OK) {
        std::wcout << " Unable to clear disk partitions  : " << hr << std::endl;
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit;
    }

    hr = CreateGPTDisk(hDevice);
    if (hr != S_OK) {
        std::wcout << " Unable to initialize disk with GPT  : " << hr << std::endl;
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit;
    }

Exit:
    CloseHandle(hDevice);
    return hr;
}

// Function to detect the type of drive
std::wstring DetectDriveType(const std::wstring& drivePath) {
    HANDLE hDevice = CreateFile(drivePath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open device" << std::endl;
        return L"Unknown";
    }

    // Query storage device property
    STORAGE_PROPERTY_QUERY query;
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;
    DWORD bytesReturned = 0;

    // Allocate buffer for storage property data
    const DWORD bufferSize = 4096; // Adjust the buffer size as needed
    std::vector<BYTE> buffer(bufferSize);

    if (!DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query),
        buffer.data(), bufferSize, &bytesReturned, NULL)) {
        std::cerr << "Failed to query storage property" << std::endl;
        CloseHandle(hDevice);
        return L"Unknown";
    }

    // Extract the storage device descriptor from the buffer
    STORAGE_DEVICE_DESCRIPTOR* deviceDescriptor = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());

    if (deviceDescriptor->BusType == BusTypeNvme) {
        CloseHandle(hDevice);
        return L"NVMe Drive";
    }
    else if (deviceDescriptor->BusType == BusTypeScsi) {
        CloseHandle(hDevice);
        return L"Scsi Drive";
    }
    else if (deviceDescriptor->BusType == BusTypeSata) {
        CloseHandle(hDevice);
        return L"Sata Drive";
    }
    else {
        CloseHandle(hDevice);
        return L"Unknown";
    }
}

int wmain(int argc, wchar_t* argv[]) {

    HRESULT hr = S_OK;
    CommandLineOptions cops;
    bool result = ParseCommandLineOptions(argc, argv, cops);
    PrintCommandLineOptions(cops);
    if (!result) {
        return -1;
    }
    

    //std::wstring localFilePath = L"E:\\notes.txt";
    //std::wstring physicalDiskPath = GetPhysicalDiskPath(localFilePath.substr(0,2));
    //doFileOperations(localFilePath);

    //cops.driveNo = L"1";
    std::wstring physicalDrivePath;
    if (cops.driveNo != L"-1") {
        physicalDrivePath = L"\\\\.\\PHYSICALDRIVE" + cops.driveNo;
        std::wcout << "Input physical drive " << cops.driveNo << " Physical drive path  " << physicalDrivePath << std::endl;
    }

    if (cops.createPartitionWithGuid == L"1") {
        GUID inputGuid = StringToGUID(cops.guid);
        LONGLONG value = std::stoll(cops.size);
        std::wcout << "Given GUID: " << cops.guid << " Size:  " << value << " Drive: " << physicalDrivePath << std::endl;
        std::wstring partitionName = L"Custom partition";
        DWORD64 attributes = GPT_BASIC_DATA_ATTRIBUTE_NO_DRIVE_LETTER;
        const GUID PARTITION_BASIC_DATA_GUID = { 0xebd0a0a2, 0xb9e5, 0x4433, 0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7 };
        bool result = CreateGptPartition(physicalDrivePath, value, inputGuid, partitionName, attributes, PARTITION_BASIC_DATA_GUID);
        std::wcout << "Result of creating : " << result << std::endl;

        int readWrite = std::stoi(cops.readWrite);
        GetPartitionInfoFromPhysicalPath(physicalDrivePath, readWrite);

        return 0;
    }

    //cops.initDrive = L"1";
    if (cops.initDrive == L"1") {
        hr = InitDrive(physicalDrivePath);
        if (hr != S_OK) {
            std::wcout << "Unable initialize disk " << physicalDrivePath << " Error:  " << hr << std::endl;
            return -1;
        }
        std::wcout << "Init drive is completed .." << std::endl;
        //return 0;
    }
    if (cops.createPartition == L"1") {
        std::wstring physicalDrivePath;
        if (cops.driveNo != L"-1") {
            physicalDrivePath = L"\\\\.\\PHYSICALDRIVE" + cops.driveNo;
            std::wcout << "Input physical drive " << cops.driveNo << " Physical drive path  " << physicalDrivePath << std::endl;
        }
        
        PDRIVE_LAYOUT_INFORMATION_EX layoutInfo = nullptr;
        hr = GetDiskPartitions(physicalDrivePath, &layoutInfo);
        if (hr == S_OK && layoutInfo->PartitionCount == 0) {
            const GUID PARTITION_MSFT_RESERVED_GUID = { 0xe3c9e316, 0x0b5c, 0x4db8, 0x81, 0x7d, 0xf9, 0x2d, 0xf0, 0x02, 0x15, 0xae };

            const LONGLONG MSFT_RESERVED = 16777216;
            std::wstring PARTITION_MSFT_RESERVED_STR = L"Microsoft reserved partition";
            DWORD64 attributes = 0;
            GUID inputGuid;
            hr = GetANewGuid(inputGuid);

            std::wcout << "Creating reserved partition with GUID: " 
                << GUIDToString(PARTITION_MSFT_RESERVED_GUID) 
                << " Size:  " << MSFT_RESERVED << " Drive: " << physicalDrivePath << std::endl;
            bool result = CreateGptPartition(
                physicalDrivePath, 
                MSFT_RESERVED, 
                inputGuid,
                PARTITION_MSFT_RESERVED_STR,
                attributes,
                PARTITION_MSFT_RESERVED_GUID);

        }
        //const GUID inputGuid = {0x44B59590, 0xEE00, 0x400A, {0x09, 0x80, 0x83, 0x03, 0x19, 0x21, 0x02, 0x0F}};
        GUID inputGuid;
        if (cops.guid != L"-1") {
            inputGuid = StringToGUID(cops.guid);
        }
        else {
            std::wcout << "No guid is provided " << std::endl;
            hr = GetANewGuid(inputGuid);
        }
        
        
        LONGLONG length = std::stoll(cops.size);
        std::wstring partitionName = L"custom partition";
        DWORD64 attributes = GPT_BASIC_DATA_ATTRIBUTE_NO_DRIVE_LETTER;
        const GUID PARTITION_BASIC_DATA_GUID = { 0xebd0a0a2, 0xb9e5, 0x4433, 0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7 };
        std::wcout << "Creating partition with GUID: " << GUIDToString(inputGuid) << " Size:  " << length << " Drive: " << physicalDrivePath << std::endl;
        bool result = CreateGptPartition(physicalDrivePath, length, inputGuid, partitionName, attributes, PARTITION_BASIC_DATA_GUID);
        std::wcout << "Result of creating : " << result << std::endl;

        int readWrite = std::stoi(cops.readWrite);
        GetPartitionInfoFromPhysicalPath(physicalDrivePath, readWrite);

        return 0;

    }

    if(!cops.filePath.empty()) {
        doFileOperations(cops.filePath);
    }
    
    if(cops.guid != L"-1") {
        g_InputGUID = cops.guid;
        std::wcout << "Input Guid  " << g_InputGUID << std::endl;
    }
    //cops.driveNo = L"0";
    /*if (cops.driveNo != L"-1") {
        std::wstring physicalDrivePath = L"\\\\.\\PHYSICALDRIVE" + cops.driveNo;
        std::wcout << "Input physical drive " << cops.driveNo << " Physical drive path  " << physicalDrivePath << std::endl;
        int readWrite = std::stoi(cops.readWrite);
        GetPartitionInfoFromPhysicalPath(physicalDrivePath, readWrite);
        if (!cops.partitionNo.empty()) {
            std::wcout << " Changing partition guid" << std::endl;
            SetPartitionGUID(physicalDrivePath, std::stoi(cops.partitionNo), StringToGUID(cops.guid));
        }
        return 0;
    }*/
    
    cops.listDrives = L"1";
    if (cops.listDrives == L"1") {
        std::wcout << "Listing Drives information : " << std::endl;
        std::vector<std::wstring> physicalDrivePaths;
        hr = GetAllPhysicalDrivePaths1(physicalDrivePaths);
        if (hr != S_OK) {
            std::wcout << "Unable to get physical drive paths " << hr << std::endl;
            return -1;
        }
        for (const auto& physicalDrivePath : physicalDrivePaths) {
            std::wcout <<"Drive path :: " << physicalDrivePath.c_str() << std::endl;
            GUID nvmeGuid;
            GetNvmeGuidForDrive(physicalDrivePath.c_str(), &nvmeGuid);
            std::wcout << "NVME guid : " << GUIDToString(nvmeGuid) << std::endl;
            std::wstring driveType = DetectDriveType(physicalDrivePath);
            std::wcout << "Drive type : " << driveType << std::endl;
        }
        std::wcout << "Completed list drives : " << std::endl;
    }
    cops.listPartitions = L"1";
    if (cops.listPartitions == L"1" && cops.driveNo != L"-1") {
        std::wstring physicalDrivePath = L"\\\\.\\PHYSICALDRIVE" + cops.driveNo;
        std::wcout << "Input physical drive " << cops.driveNo << " Physical drive path  " << physicalDrivePath << std::endl;
        int readWrite = std::stoi(cops.readWrite);
        GetPartitionInfoFromPhysicalPath(physicalDrivePath, readWrite);
    }
    
    if (cops.listAllPartitions == L"1") {
        std::vector<std::wstring> physicalDrivePaths;
        hr = GetAllPhysicalDrivePaths1(physicalDrivePaths);
        if (hr != S_OK) {
            std::wcout << "Unable to get physical drive paths " << hr << std::endl;
            return -1;
        }
        for (const auto& physicalDrivePath : physicalDrivePaths) {
            std::wcout << "---->  Enumurating physical drive  " << physicalDrivePath.c_str() << "  <--------" << std::endl;
            GetPartitionInfoFromPhysicalPath(physicalDrivePath, false);
        }
    }
    
    if (cops.drivesGeometry == L"1") {
        EnumeratePhysicalDrivesGeomitry();
    }

    return 0;
}
