#if PLATFORM_LINUX

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

#include "Serial/Serial.h"
#include "Log.h"

struct SerialNative
{
    int mFd = -1;
};

void SER_Initialize() {}
void SER_Shutdown() {}

static bool LooksLikeSerial(const char* name)
{
    return (strncmp(name, "ttyUSB", 6) == 0) ||
           (strncmp(name, "ttyACM", 6) == 0) ||
           (strncmp(name, "ttyS",   4) == 0) ||
           (strncmp(name, "ttyAMA", 6) == 0);
}

std::vector<SerialPortInfo> SER_EnumeratePorts()
{
    std::vector<SerialPortInfo> ports;

    DIR* dir = opendir("/dev");
    if (dir == nullptr)
        return ports;

    dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (!LooksLikeSerial(entry->d_name))
            continue;

        char fullPath[PATH_MAX] = {};
        snprintf(fullPath, sizeof(fullPath), "/dev/%s", entry->d_name);

        // Best-effort check that we can access this device.
        struct stat st;
        if (stat(fullPath, &st) != 0)
            continue;

        SerialPortInfo info;
        info.mPortName = fullPath;
        info.mDescription = entry->d_name;
        ports.push_back(info);
    }

    closedir(dir);
    return ports;
}

static speed_t BaudToSpeed(uint32_t baud)
{
    switch (baud)
    {
    case 9600:    return B9600;
    case 19200:   return B19200;
    case 38400:   return B38400;
    case 57600:   return B57600;
    case 115200:  return B115200;
    case 230400:  return B230400;
    case 460800:  return B460800;
    case 921600:  return B921600;
    case 1200:    return B1200;
    case 2400:    return B2400;
    case 4800:    return B4800;
    default:      return B9600;
    }
}

SerialNative* SER_Open(const char* portName, const SerialConfig& cfg)
{
    int fd = open(portName, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
    {
        LogWarning("SER_Open: failed to open %s (errno %d)", portName, errno);
        return nullptr;
    }

    termios tio = {};
    if (tcgetattr(fd, &tio) != 0)
    {
        close(fd);
        LogWarning("SER_Open: tcgetattr failed for %s", portName);
        return nullptr;
    }

    speed_t speed = BaudToSpeed(cfg.mBaudRate);
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);

    // 8-N-1 defaults, then adjust.
    tio.c_cflag &= ~CSIZE;
    switch (cfg.mDataBits)
    {
    case 5: tio.c_cflag |= CS5; break;
    case 6: tio.c_cflag |= CS6; break;
    case 7: tio.c_cflag |= CS7; break;
    default: tio.c_cflag |= CS8; break;
    }

    if (cfg.mStopBits == 2) tio.c_cflag |= CSTOPB;
    else                    tio.c_cflag &= ~CSTOPB;

    switch (cfg.mParity)
    {
    case SerialParity::None:
        tio.c_cflag &= ~PARENB;
        break;
    case SerialParity::Odd:
        tio.c_cflag |= PARENB;
        tio.c_cflag |= PARODD;
        break;
    case SerialParity::Even:
        tio.c_cflag |= PARENB;
        tio.c_cflag &= ~PARODD;
        break;
    default:
        tio.c_cflag &= ~PARENB;
        break;
    }

    if (cfg.mFlowControl) tio.c_cflag |= CRTSCTS;
    else                  tio.c_cflag &= ~CRTSCTS;

    tio.c_cflag |= (CLOCAL | CREAD);

    // Raw mode: no canonical processing, no echo, no signals.
    tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tio.c_iflag &= ~(IXON | IXOFF | IXANY | INLCR | ICRNL);
    tio.c_oflag &= ~OPOST;

    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tio) != 0)
    {
        close(fd);
        LogWarning("SER_Open: tcsetattr failed for %s", portName);
        return nullptr;
    }

    tcflush(fd, TCIOFLUSH);

    SerialNative* native = new SerialNative();
    native->mFd = fd;
    return native;
}

void SER_Close(SerialNative* native)
{
    if (native == nullptr)
        return;
    if (native->mFd >= 0)
    {
        close(native->mFd);
    }
    delete native;
}

int32_t SER_Write(SerialNative* native, const uint8_t* data, uint32_t size)
{
    if (native == nullptr || native->mFd < 0)
        return -1;

    ssize_t n = write(native->mFd, data, size);
    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        return -1;
    }
    return (int32_t)n;
}

int32_t SER_Read(SerialNative* native, uint8_t* buffer, uint32_t maxSize)
{
    if (native == nullptr || native->mFd < 0)
        return -1;

    ssize_t n = read(native->mFd, buffer, maxSize);
    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        return -1;
    }
    if (n == 0)
    {
        // EOF / device disappeared.
        return -1;
    }
    return (int32_t)n;
}

#endif
