#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <string>
#include <osiSock.h>    // EPICS os independent socket

// Forward declarations
typedef struct request  request_t;
typedef struct response response_t;
typedef struct socket   socket_t;


class HTTPClient
{
public:
    HTTPClient ();
    ~HTTPClient();

    //! set server host address
    bool setHost(const std::string& uri);
    //! issue HTTP GET requests
    bool get (const std::string& path, std::string& value);
    //! issue HTTP PUT requests
    bool put (const std::string& path, const std::string& content, std::string& value);
    //! issue HTTP POST requests
    bool post (const std::string& path, const std::string& content, std::string& value);

protected:
    //! issue an HTTP request and get server's response
    bool doRequest (const request_t& request, response_t& response);
    //! create the socket connection
    bool connect (socket_t *s);
    //! close the socket connection
    void close (socket_t *s);
    //! enable/disable socket nonblock option
    bool setNonBlock (socket_t *s, bool nonBlock);

private:
    std::string mHostname;
    int mPort;
    std::string mPath;
    struct sockaddr_in mServerAddress;
    std::string mLocalHost;
    size_t mNumSockets;
    socket_t *mSockets;
    int mTimeout;
};

#endif
