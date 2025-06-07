#pragma once
#include <unistd.h> 
#include <list>
#include <fcntl.h>
#include <termios.h>
#include <iostream>
#include <fstream>
#include "nlohmann/json.hpp"
using json = nlohmann::json;


std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, int(*)(FILE*)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

std::list<int> getid(std::string a){
    std::string id=exec(("pw-dump | jq -r '.[] | select(.info.props.\"application.name\"==\""+a+"\") | \"\\(.id) \\(.type)\"' | grep \"Node\"").c_str());
    if(id.size()<1) return {0};
    std::list<int> l;
    while (id.find("\n") && id.find("\n")<=id.size()){
        l.push_front(stoi(id.substr(0,id.find(" "))));
        id.erase(0,id.find("\n")+1);
    }
    return l;
}

uint8_t checkvolume(uint8_t id){
    if(id==0){ return 160;}
    return stof((exec(("wpctl get-volume "+std::to_string(id)).c_str()).substr(8,11)))*100;
}

uint8_t changevolume(std::string a, std::string vol,std::string direction){
    if(a.compare("Master")==0){
        system(("amixer set Master "+vol+"%"+direction).c_str());
        return 0;
    }
    std::list<int> id = getid(a);
    for(int n:id){
        if(checkvolume(n)<=150){
            system(("wpctl set-volume "+std::to_string(n)+" "+vol+"%"+direction+"").c_str());
        }
    }
    return 0;
}

uint8_t soundboard(std::string sound){
    system(("./soundboard.sh "+sound).c_str());
    return 0;
}

int init_read(){
    int fd = open("/dev/ttyACM0", O_RDONLY | O_NOCTTY | O_NONBLOCK);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    termios tty{};
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        close(fd);
        return 1;
    }
    cfsetispeed(&tty, B115200);  // Set baud rate
    cfsetospeed(&tty, B115200);
    tty.c_cflag |= (CLOCAL | CREAD);
    tcsetattr(fd, TCSANOW, &tty);


    return fd;
}

int readport(int fd){
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    int ret = select(fd + 1, &readfds, nullptr, nullptr, &timeout);
    if (ret > 0 && FD_ISSET(fd, &readfds)) {
        char buffer[256];
        ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            //std::cout << "Received: " << buffer << std::endl;
            //close(fd);
            return atoi(buffer);
        }
    } else if (ret == 0) {
        //std::cout << "Timeout: no data" << std::endl;
    } else {
        perror("select");
    }

    //close(fd);
    return 0;
}

int changemic(){
    system("amixer set Capture toggle");
    return 0;
}

int sendserial(std::string send){
    std::ofstream serial("/dev/ttyACM0");
    if(!serial.is_open()){
        return 1;
    }
    serial<<send<<std::endl;
    serial.close();
    return 0;
}

int executefunction(int page, int butt){
    std::ifstream file("../frontend/config.json");
    json jsn;
    file >> jsn;
    json button = jsn["pages"][page][butt-1];
    
    if(button.value("type", "").compare("shortcut")==0){ 
        //continue;
        
    }else if(button.value("type", "").compare("changevolume")==0){
        changevolume(button["data"].value("target", ""), button["data"].value("step", ""), button["data"].value("direction", ""));
        return 0;
    }
    soundboard(button["data"].value("file", ""));
    return 0;

}
