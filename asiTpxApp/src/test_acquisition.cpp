#include "HTTPClient.h"

#include <string>
#include <iostream>

int main(int argc, char **argv)
{
    HTTPClient client;
    std::string response;

    client.setHost("http://localhost:8080");
    client.get("/detector/config", response);
    std::cout << response << std::endl;

    client.get("/dashboard", response);
    std::cout << response << std::endl;

    client.get("/sdcsd", response);
    std::cout << response << std::endl;

    return 0;
}
