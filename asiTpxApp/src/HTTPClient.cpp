#include "HTTPClient.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include <stdexcept>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <mutex>
#include <map>
#include <vector>

#ifdef _WIN32
 // Required for EPICS base 3.14.
 // In EPICS base >= 3.15, WIN32/osdSock.h includes this header.
#include <ws2tcpip.h>
#endif

#include <epicsThread.h>

#define EOL                     "\r\n"      // End of Line

#define MAX_SOCKETS             5
#define MAX_HTTP_RETRIES        1
#define MAX_BUF_SIZE            256

#define DEFAULT_TIMEOUT_CONNECT 3
#define DEFAULT_TIMEOUT_REPLY 30

#define ERR_PREFIX  "HTTPClient"
#define ERR(msg) fprintf(stderr, ERR_PREFIX "::%s: %s\n", functionName, msg)

#define ERR_ARGS(fmt,...) fprintf(stderr, ERR_PREFIX "::%s: " fmt "\n", \
    functionName, __VA_ARGS__)

/*
 * Structure definitions
 *
 */

typedef struct socket
{
    SOCKET fd;
    epicsMutexId mutex;
    bool closed;
} socket_t;

typedef struct request
{
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string content;
    // for server
    size_t contentLength;
    bool close;
} request_t;

typedef struct response
{
    std::map<std::string, std::string> headers;
    bool reconnect;
    bool chunked;
    std::string content;
    size_t contentLength;
    int code;
    char version[40];
} response_t;

/*!
 * Simple parser of url of form http://<host>[:<port>][/path]
 */
struct URL
{
    URL(const std::string& url);
    std::string host;
    int port;
    std::string path;
};

/*
 * Helper functions
 *
 */

/*! If header exists return its value,
 * otherwise return the default value.
 */
static std::string getHeaderValue(
        std::map<std::string, std::string>headers,
        std::string header,
        std::string defaultValue="")
{
    auto search = headers.find(header);
    if (search == headers.end())
        return defaultValue;
    return search->second;
}

/* Parse header text to a map */
static bool parseHeaderLine (std::string header, std::map<std::string, std::string>& headers)
{
    size_t n = header.find_first_of(':');
    if (n == std::string::npos)
        return false;
    std::string key = header.substr(0, n);

    // strip off leading/trailing white spaces
    size_t d1 = header.find_first_not_of(" \t\r\n", n+1);
    size_t d2 = header.find_last_not_of(" \t\r\n");
    std::string value = header.substr(d1, d2+1-d1);

    headers[key] = value;

    return true;
}

/* Read a single line from socket */
static bool readLine (SOCKET s, std::string &line)
{
    const char *functionName = "readLine";

    line.clear();
    while (true) {
        char b;
        int n = recv(s, &b, 1, 0);
        if (n < 0) {
            char error[MAX_BUF_SIZE];
            epicsSocketConvertErrnoToString(error, sizeof(error));
            ERR_ARGS("recv error %d:%s\n", SOCKERRNO, error);
            return false;
        } else if (n == 0) {
            if (line.length() == 0)
                return false;
            else
                return true;
        } else {
            line.push_back(b);
            if (b == '\n')
                break;
        }
    }

    return true;
}

/*! Read HTTP response content until connection is closed */
static bool readIndefiniteContent(SOCKET s, std::string &content)
{
    char buf[4096];
    while (true) {
        int received = recv(s, buf, sizeof(buf), 0);
        if (received < 0) {
            return false;
        } else if (received == 0) {
            return true;
        } else {
            content.append(buf, received);
        }
    }
}

/*! Read HTTP response content with a specified length */
static bool readFixedContent(SOCKET s, std::string &content, size_t length)
{
    std::vector<char> buf(length);
    char *p = buf.data();
    size_t bufSize = length;
    while (bufSize) {
        int received = recv(s, p, (int)bufSize, 0);
        if (received <= 0) {
            return false;
        }
        p += received;
        bufSize -= received;
    }
    content.append(buf.data(), length);

    return true;
}

/*! Read HTTP response chunked content */
static bool readChunkedContent(SOCKET s, std::string &content)
{
    while (true) {
        // chunk size
        std::string lengthLine;
        if (!readLine(s, lengthLine))
            return false;
        size_t length = std::strtoul(lengthLine.c_str(), NULL, 16);

        // chunk data
        if (length) {
            std::string chunk;
            if (!readFixedContent(s, chunk, length))
                return false;
            content.append(chunk);
        }

        // end of chunk
        std::string line;
        if (!readLine(s, line) || line != "\r\n")
            return false;

        // end of content
        if (length == 0)
            return true;
    }
}

/* Send given data over socket */
static bool write(SOCKET s, const std::string& data)
{
    size_t remaining = data.size();
    const char *p = data.c_str();

    while (remaining) {
        int written = send(s, p, (int)remaining, 0);
        if (written < 0)
            return false;
        remaining -= written;
        p += written;
    }
    return true;
}

/*
 * URL
 *
 */

URL::URL(const std::string& url)
    : host("127.0.0.1"), port(80), path("/")
{
    const char scheme[] = "http://";

    if (url.find(scheme) != 0)
        return;
    size_t begin = strlen(scheme);
    size_t end = url.find('/', begin);
    std::string address;
    if (end == std::string::npos) {
        address = url.substr(begin);
    } else {
        address = url.substr(begin, end - begin);
        path = url.substr(end);
    }
    end = address.find(':');
    if (end == std::string::npos) {
        host = address;
    } else {
        host = address.substr(0, end);
        port = std::stoi(address.substr(end+1));
    }
}

/*
 * httpClient
 *
 */

HTTPClient::HTTPClient ()
    : mNumSockets(MAX_SOCKETS), mSockets(new socket_t[MAX_SOCKETS]),
      mTimeout(DEFAULT_TIMEOUT_REPLY)
{
    for(size_t i = 0; i < mNumSockets; ++i) {
        mSockets[i].mutex = epicsMutexCreate();
    }
}

HTTPClient::~HTTPClient ()
{
    for(size_t i = 0; i < mNumSockets; ++i)
    {
        if (!mSockets[i].closed && mSockets[i].fd != INVALID_SOCKET) {
            epicsMutexDestroy(mSockets[i].mutex);
            epicsSocketDestroy(mSockets[i].fd);
        }
    }
    delete[] mSockets;
}

bool HTTPClient::setHost (const std::string& uri)
{
    const char *functionName = "setHost";

    URL url(uri);
    mHostname = url.host;
    mPort = url.port;
    mPath = url.path;

    memset(&mServerAddress, 0, sizeof(mServerAddress));

    if(hostToIPAddr(mHostname.c_str(), &mServerAddress.sin_addr)) {
        ERR_ARGS("invalid hostname: %s", mHostname.c_str());
        return false;
    }

    // convert hostname to dot decimals
    char serverHost[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &mServerAddress.sin_addr, serverHost, sizeof(serverHost));
    mHostname = serverHost;

    mServerAddress.sin_family = AF_INET;
    mServerAddress.sin_port = htons(mPort);

    for(size_t i = 0; i < mNumSockets; ++i)
    {
        mSockets[i].closed = true;
        mSockets[i].fd = INVALID_SOCKET;
    }

    return true;
}

void HTTPClient::close (socket_t *s)
{
    if (s->fd != INVALID_SOCKET) {
        epicsSocketDestroy(s->fd);
        s->fd = INVALID_SOCKET;
    }
    s->closed = true;
}

bool HTTPClient::connect (socket_t *s)
{
    const char *functionName = "connect";

    if(!s->closed)
        return true;

    s->fd = epicsSocketCreate(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(s->fd == INVALID_SOCKET)
    {
        ERR("couldn't create socket");
        return false;
    }

    setNonBlock(s, true);

    if(::connect(s->fd, (struct sockaddr*)&mServerAddress, sizeof(mServerAddress)) < 0)
    {
        // Connection actually failed
#ifdef _WIN32
        if(SOCKERRNO != SOCK_EWOULDBLOCK)
#else
        if(SOCKERRNO != SOCK_EINPROGRESS)
#endif
        {
            char error[MAX_BUF_SIZE];
            epicsSocketConvertErrnoToString(error, sizeof(error));
            ERR_ARGS("failed to connect to %s:%d [%d:%s]", mHostname.c_str(),
                    mPort, SOCKERRNO, error);
            epicsSocketDestroy(s->fd);
            s->fd = INVALID_SOCKET;
            return false;
        }
        // Server didn't respond immediately, wait a little
        else
        {
            fd_set set;
            struct timeval tv;
            int ret;

            FD_ZERO(&set);
            FD_SET(s->fd, &set);
            tv.tv_sec  = DEFAULT_TIMEOUT_CONNECT;
            tv.tv_usec = 0;

            ret = select(int(s->fd) + 1, NULL, &set, NULL, &tv);
            if(ret <= 0)
            {
                const char *error = ret == 0 ? "TIMEOUT" : "select failed";
                ERR_ARGS("failed to connect to %s:%d [%d:%s]", mHostname.c_str(),
                        mPort, SOCKERRNO, error);
                epicsSocketDestroy(s->fd);
                s->fd = INVALID_SOCKET;
                return false;
            }
        }
    }

    // cache local host address
    if (mLocalHost.empty()) {
        struct sockaddr_in localAddress = {};
        char localHost[INET_ADDRSTRLEN];
        osiSocklen_t addrSize = (osiSocklen_t) sizeof(localAddress);
        getsockname(s->fd, (struct sockaddr *) &localAddress, &addrSize);
        inet_ntop(AF_INET, &localAddress.sin_addr, localHost, sizeof(localHost));
        mLocalHost = localHost;
    }

    setNonBlock(s, false);
    s->closed = false;
    return true;
}

bool HTTPClient::setNonBlock (socket_t *s, bool nonBlock)
{
#if defined(_WIN32)
    unsigned long int flags;
    flags = nonBlock;
    if (socket_ioctl(s->fd, FIONBIO, &flags) < 0)
        return false;
#else
    int flags = fcntl(s->fd, F_GETFL, 0);
    if(flags < 0)
        return false;

    flags = nonBlock ? flags | O_NONBLOCK : flags & ~O_NONBLOCK;
    if (fcntl(s->fd, F_SETFL, flags) < 0)
        return false;
#endif
    return true;
}

bool HTTPClient::doRequest (const request_t& request, response_t& response)
{
    const char *functionName = "doRequest";

    socket_t *s = NULL;
    for (size_t i = 0; i < mNumSockets; ++i) {
        if(epicsMutexTryLock(mSockets[i].mutex) == epicsMutexLockOK) {
            s = &mSockets[i];
            break;
        }
    }
    if (!s) {
        ERR("no available socket");
        return false;
    }

    std::string data;
    data += request.method + " " + request.path + " HTTP/1.1" + EOL;
    data += "Host: " + mHostname + EOL;
    for (const auto& el: request.headers) {
        data +=  el.first + ": " + el.second + EOL;
    }
    if (request.content.length()) {
        data += "Content-Length: " + std::to_string(request.content.length()) + EOL;
    }
    data += EOL;
    data += request.content;

    size_t retries = 0;
reconnect:
    if (!connect(s)) {
        ERR("failed to reconnect socket");
        return false;
    }

    if (!write(s->fd, data)) {
        this->close(s);
        if (retries++ < MAX_HTTP_RETRIES) {
            goto reconnect;
        }
        char error[MAX_BUF_SIZE];
        epicsSocketConvertErrnoToString(error, sizeof(error));
        ERR_ARGS("send failed %d:%s", SOCKERRNO, error);
        return false;
    }

    struct timeval recvTimeout;
    fd_set fds;
    if(mTimeout >= 0) {
        recvTimeout.tv_sec = mTimeout;
        recvTimeout.tv_usec = 0;
        FD_ZERO(&fds);
        FD_SET(s->fd, &fds);
    }

    int ret = select(int(s->fd)+1, &fds, NULL, NULL, &recvTimeout);
    if (ret <= 0) {
        if (ret == 0)
            ERR_ARGS("timeout (%ds) in waiting for reply", mTimeout);
        else {
            char error[MAX_BUF_SIZE];
            epicsSocketConvertErrnoToString(error, sizeof(error));
            ERR_ARGS("select() failed ret=%d error=%d:%s", ret, SOCKERRNO, error);
        }
        this->close(s);
        epicsMutexUnlock(s->mutex);
        return false;
    }

    // status line
    std::string status;
    if (!readLine(s->fd, status)) {
        this->close(s);
        if (retries++ < MAX_HTTP_RETRIES) {
            goto reconnect;
        }
        char error[MAX_BUF_SIZE];
        epicsSocketConvertErrnoToString(error, sizeof(error));
        ERR_ARGS("failed to read response line %s", error);
        epicsMutexUnlock(s->mutex);
        return false;
    }
    sscanf(status.c_str(), "%s %d", response.version, &response.code);

    // headers
    while (true)
    {
        std::string line;
        if (!readLine(s->fd, line))
        {
            ERR("failed to read header line");
            this->close(s);
            epicsMutexUnlock(s->mutex);
            return false;
        }
        // check EOL
        size_t len = line.length();
        if (len < 2 || line[len-2] != '\r' || line[len-1] != '\n')
            continue;
        // end of header
        if (len == 2)
            break;
        if (!parseHeaderLine(line, response.headers))
            continue;
    }
    // treat some specific headers
    response.contentLength = std::stoi(getHeaderValue(response.headers, "Content-Length", "0"));
    response.reconnect = getHeaderValue(response.headers, "Connection", "") == "close";
    response.chunked =  getHeaderValue(response.headers, "Transfer-Encoding", "") == "chunked";

    // content
    if (response.chunked) {
        if (!readChunkedContent(s->fd, response.content)) {
            this->close(s);
            epicsMutexUnlock(s->mutex);
            return false;
        }
    } else if (response.headers.find("Content-Length") == response.headers.end()) {
        if (!readIndefiniteContent(s->fd, response.content)) {
            this->close(s);
            epicsMutexUnlock(s->mutex);
            return false;
        }
    } else if (response.contentLength) {
        if (!readFixedContent(s->fd, response.content, response.contentLength)) {
            this->close(s);
            epicsMutexUnlock(s->mutex);
            return false;
        }
    }

    // close socket if requested
    if(response.reconnect)
        this->close(s);
    
    epicsMutexUnlock(s->mutex);
    return true;
}

bool HTTPClient::get (const std::string& path, std::string & value)
{
    const char *functionName = "get";

    request_t request = {};
    request.method = "GET";
    request.path = path;

    response_t response = {};

    if(!doRequest(request, response))
    {
        ERR_ARGS("[%s] request failed", path.c_str());
        return false;
    }
    
    value = response.content;

    if(response.code != 200)
    {
        ERR_ARGS("[%s] server returned error code %d",
                path.c_str(), response.code);
        return false;
    }

    return true;
}

bool HTTPClient::put (const std::string& path, const std::string& content, std::string& value)
{
    const char *functionName = "put";

    request_t request = {};
    request.method = "PUT";
    request.path = path;
    request.headers["Content-Type"] = "plain/text";
    request.content = content;

    response_t response = {};

    if(!doRequest(request, response))
    {
        ERR_ARGS("[%s] request failed", path.c_str());
        return false;
    }
    
    value = response.content;

    if(response.code != 200)
    {
        ERR_ARGS("[%s] server returned error code %d",
                path.c_str(), response.code);
        return false;
    }

    return true;
}

bool HTTPClient::post (const std::string& path, const std::string& content, std::string& value)
{
    const char *functionName = "post";

    request_t request = {};
    request.method = "POST";
    request.path = path;
    request.headers["Content-Type"] = "plain/text";
    request.content = content;

    response_t response = {};

    if(!doRequest(request, response))
    {
        ERR_ARGS("[%s] request failed", path.c_str());
        return false;
    }
    
    value = response.content;

    if(response.code != 200)
    {
        ERR_ARGS("[%s] server returned error code %d",
                path.c_str(), response.code);
        return false;
    }

    return true;
}
